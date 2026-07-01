/* Servidor HTTP local usando libmicrohttpd, carga los datasets al iniciar y expone endpoints para verificar contraseñas desde el navegador */

#include "Password.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <microhttpd.h>
#include <signal.h>

#define PUERTO 8080

#define TAM_RESULTADO_UNO  1024

#define TAM_RESULTADOS_ARCHIVO  65536

#define TAM_MAXIMO_PETICION  (10 * 1024 * 1024)

#define TAM_INICIAL_ACUMULADOR  4096

static char** g_filtradas = NULL;
static int      g_total_filtradas = 0;
static char** g_palabras = NULL;
static int      g_total_palabras = 0;

static struct MHD_Daemon* g_servidor = NULL;

static volatile sig_atomic_t g_seguir_corriendo = 1;


static CRITICAL_SECTION g_lock;


typedef struct {
    char* buffer;
    size_t  tam_usado;
    size_t  tam_reservado;
    int     respuesta_enviada;
} EstadoPeticion;


static EstadoPeticion* crear_estado_peticion(void) {
    EstadoPeticion* estado = (EstadoPeticion*)calloc(1, sizeof(EstadoPeticion));
    return estado;
}


static void destruir_estado_peticion(EstadoPeticion* estado) {
    if (estado == NULL) return;
    free(estado->buffer);
    free(estado);
}


static int acumular_datos(EstadoPeticion* estado, const char* datos,
    size_t tam, size_t limite) {

    if (estado->tam_usado + tam > limite) {
        return 0;
    }

    if (estado->tam_usado + tam > estado->tam_reservado) {
        size_t nuevo_tam = (estado->tam_reservado == 0)
            ? TAM_INICIAL_ACUMULADOR : estado->tam_reservado * 2;
        char* nuevo_buffer;

        while (nuevo_tam < estado->tam_usado + tam) {
            nuevo_tam *= 2;
        }

        nuevo_buffer = (char*)realloc(estado->buffer, nuevo_tam);
        if (nuevo_buffer == NULL) {
            return -1;
        }

        estado->buffer = nuevo_buffer;
        estado->tam_reservado = nuevo_tam;
    }

    memcpy(estado->buffer + estado->tam_usado, datos, tam);
    estado->tam_usado += tam;
    return 1;
}


static void solicitud_completada(void* cls,
    struct MHD_Connection* conexion,
    void** con_cls,
    enum MHD_RequestTerminationCode motivo) {
    EstadoPeticion* estado;

    (void)cls;
    (void)conexion;
    (void)motivo;

    estado = (EstadoPeticion*)(*con_cls);
    destruir_estado_peticion(estado);
    *con_cls = NULL;
}


/* Funciones de carga y Memoria */

int cargar_archivo(const char* ruta, char** lista, int max) {
    FILE* archivo = NULL;
    char    buffer[MAX_PASSWORD_LEN];
    int     total = 0;
    size_t  longitud;

    if (ruta == NULL) {
        fprintf(stderr, "Error: ruta no puede ser NULL\n");
        return -1;
    }
    if (lista == NULL) {
        fprintf(stderr, "Error: lista no puede ser NULL\n");
        return -1;
    }
    if (max <= 0) {
        fprintf(stderr, "Error: max debe ser mayor a 0\n");
        return -1;
    }

    archivo = fopen(ruta, "r");
    if (archivo == NULL) {
        fprintf(stderr, "Error: no se pudo abrir: %s\n", ruta);
        return -1;
    }

    printf("Cargando %s...\n", ruta);

    /* Se revisa total < max ANTES de leer, para no consumir de mas
       una linea del archivo cuando ya se alcanzo el limite. */
    while (total < max) {
        if (fgets(buffer, MAX_PASSWORD_LEN, archivo) == NULL) {
            break;
        }

        longitud = strlen(buffer);
        if (longitud > 0 && buffer[longitud - 1] == '\n') {
            buffer[longitud - 1] = '\0';
            longitud--;
        }
        /* Quitar también '\r' si el archivo viene con CRLF */
        if (longitud > 0 && buffer[longitud - 1] == '\r') {
            buffer[longitud - 1] = '\0';
            longitud--;
        }
        if (longitud == 0) continue;

        lista[total] = (char*)malloc((longitud + 1) * sizeof(char));
        if (lista[total] == NULL) {
            fprintf(stderr, "Error: memoria insuficiente en linea %d\n",
                total + 1);
            fclose(archivo);
            return total;
        }

        strcpy(lista[total], buffer);
        total++;

        if (insertion_sort(lista, total) != 0) {

            fprintf(stderr,
                "Error: fallo el ordenamiento en la linea %d, "
                "se detiene la carga\n", total);
            fclose(archivo);
            return total;
        }
    }


    if (total == max) {
        if (fgets(buffer, MAX_PASSWORD_LEN, archivo) != NULL) {
            fprintf(stderr,
                "Advertencia: %s tiene mas de %d entradas validas; "
                "las restantes NO se cargaron (limite MAX_PASSWORDS/"
                "MAX_PALABRAS alcanzado)\n", ruta, max);
        }
    }

    fclose(archivo);
    printf("Cargadas %d entradas desde %s\n", total, ruta);
    return total;
}


void liberar_memoria(char** lista, int total) {
    int i;
    if (lista == NULL) return;
    for (i = 0; i < total; i++) {
        if (lista[i] != NULL) {
            free(lista[i]);
            lista[i] = NULL;
        }
    }
}


/* Escape de Cadenas para json*/

static void escapar_json(const char* origen, char* destino, size_t tam) {
    size_t i = 0;
    size_t j = 0;

    if (origen == NULL || destino == NULL || tam == 0) return;

    /* Se reserva espacio para el caracter de escape adicional
       y el terminador nulo, por eso "tam - 2" como límite */
    while (origen[i] != '\0' && j < tam - 2) {
        switch (origen[i]) {
        case '"':
            destino[j++] = '\\';
            destino[j++] = '"';
            break;
        case '\\':
            destino[j++] = '\\';
            destino[j++] = '\\';
            break;
        case '\n':
            destino[j++] = '\\';
            destino[j++] = 'n';
            break;
        case '\r':
            destino[j++] = '\\';
            destino[j++] = 'r';
            break;
        case '\t':
            destino[j++] = '\\';
            destino[j++] = 't';
            break;
        default:
            destino[j++] = origen[i];
            break;
        }
        i++;
    }
    destino[j] = '\0';
}


/* Funciones de Verificación */

static int verificar_una(const char* contrasena,
    char* salida, size_t tam) {
    int          resultado_binaria;
    int          resultado_lineal;
    int          puntaje;
    const char* nivel;
    const char* segura;
    char         contrasena_escapada[MAX_PASSWORD_LEN * 2];
    char         palabra_escapada[MAX_PASSWORD_LEN * 2];

    if (contrasena == NULL || salida == NULL || tam == 0) return -1;

    escapar_json(contrasena, contrasena_escapada,
        sizeof(contrasena_escapada));

    EnterCriticalSection(&g_lock);
    resultado_binaria = busqueda_binaria(g_filtradas,
        g_total_filtradas,
        contrasena);
    LeaveCriticalSection(&g_lock);

    if (resultado_binaria == -2) {

        snprintf(salida, tam,
            "{\"error\":\"No se pudo verificar la contrasena "
            "(fallo interno de busqueda)\"}");
        return -1;
    }

    if (resultado_binaria >= 0) {
        snprintf(salida, tam,
            "{\"segura\":false,"
            "\"filtrada\":true,"
            "\"palabra_comun\":false,"
            "\"palabra\":\"\","
            "\"puntaje\":0,"
            "\"nivel\":\"Debil\","
            "\"contrasena\":\"%s\"}",
            contrasena_escapada);
        return 0;
    }

    EnterCriticalSection(&g_lock);
    resultado_lineal = busqueda_lineal(g_palabras,
        g_total_palabras,
        contrasena);
    if (resultado_lineal >= 0) {
        escapar_json(g_palabras[resultado_lineal], palabra_escapada,
            sizeof(palabra_escapada));
    }
    LeaveCriticalSection(&g_lock);

    if (resultado_lineal == -2) {
        snprintf(salida, tam,
            "{\"error\":\"No se pudo verificar la contrasena "
            "(fallo interno de busqueda de palabras)\"}");
        return -1;
    }

    if (resultado_lineal >= 0) {
        snprintf(salida, tam,
            "{\"segura\":false,"
            "\"filtrada\":false,"
            "\"palabra_comun\":true,"
            "\"palabra\":\"%s\","
            "\"puntaje\":0,"
            "\"nivel\":\"Debil\","
            "\"contrasena\":\"%s\"}",
            palabra_escapada,
            contrasena_escapada);
        return 0;
    }

    puntaje = calcular_fortaleza(contrasena);
    if (puntaje < 0) puntaje = 0;

    if (puntaje <= 2) { nivel = "Debil";  segura = "false"; }
    else if (puntaje <= 4) { nivel = "Media";  segura = "false"; }
    else { nivel = "Fuerte"; segura = "true"; }

    snprintf(salida, tam,
        "{\"segura\":%s,"
        "\"filtrada\":false,"
        "\"palabra_comun\":false,"
        "\"palabra\":\"\","
        "\"puntaje\":%d,"
        "\"nivel\":\"%s\","
        "\"contrasena\":\"%s\"}",
        segura, puntaje, nivel, contrasena_escapada);
    return 0;
}


/* Manejador de la señal SIGINT (Ctrl+C) */

static void manejar_sigint(int senal) {
    (void)senal;
    g_seguir_corriendo = 0;
}



static BOOL WINAPI manejador_consola(DWORD tipo_evento) {
    if (tipo_evento == CTRL_CLOSE_EVENT
        || tipo_evento == CTRL_C_EVENT
        || tipo_evento == CTRL_BREAK_EVENT
        || tipo_evento == CTRL_LOGOFF_EVENT
        || tipo_evento == CTRL_SHUTDOWN_EVENT) {

        if (g_servidor != NULL) {
            MHD_stop_daemon(g_servidor);
            g_servidor = NULL;
        }

        EnterCriticalSection(&g_lock);
        liberar_memoria(g_filtradas, g_total_filtradas);
        liberar_memoria(g_palabras, g_total_palabras);
        free(g_filtradas);
        free(g_palabras);
        g_filtradas = NULL;
        g_palabras = NULL;
        LeaveCriticalSection(&g_lock);

        return TRUE;
    }

    return FALSE;
}


/* Manejador de Peticiones http */

static int revisar_tamano_peticion(size_t tam_datos) {
    return tam_datos <= TAM_MAXIMO_PETICION;
}


static enum MHD_Result responder_error(struct MHD_Connection* conexion,
    EstadoPeticion* estado,
    unsigned int codigo_http,
    const char* mensaje_json) {
    struct MHD_Response* respuesta;
    enum MHD_Result ret;

    respuesta = MHD_create_response_from_buffer(
        strlen(mensaje_json), (void*)mensaje_json, MHD_RESPMEM_MUST_COPY);
    if (respuesta == NULL) return MHD_NO;

    MHD_add_response_header(respuesta, "Content-Type", "application/json");
    MHD_add_response_header(respuesta, "Access-Control-Allow-Origin", "*");
    ret = MHD_queue_response(conexion, codigo_http, respuesta);
    MHD_destroy_response(respuesta);

    if (estado != NULL) {
        estado->respuesta_enviada = 1;
    }
    return ret;
}


static enum MHD_Result servir_archivo_estatico(
    struct MHD_Connection* conexion,
    const char* ruta,
    const char* content_type) {
    FILE* archivo;
    long    tam;
    char* contenido;
    struct MHD_Response* respuesta;
    enum MHD_Result ret;

    archivo = fopen(ruta, "rb");
    if (archivo == NULL) {
        return responder_error(conexion, NULL, MHD_HTTP_NOT_FOUND,
            "{\"error\":\"Archivo de interfaz no encontrado en el "
            "servidor\"}");
    }

    if (fseek(archivo, 0, SEEK_END) != 0) {
        fclose(archivo);
        return MHD_NO;
    }
    tam = ftell(archivo);
    if (tam < 0) {
        fclose(archivo);
        return MHD_NO;
    }
    if (fseek(archivo, 0, SEEK_SET) != 0) {
        fclose(archivo);
        return MHD_NO;
    }

    contenido = (char*)malloc((size_t)tam > 0 ? (size_t)tam : 1);
    if (contenido == NULL) {
        fclose(archivo);
        return responder_error(conexion, NULL,
            MHD_HTTP_INTERNAL_SERVER_ERROR,
            "{\"error\":\"Memoria insuficiente al servir archivo\"}");
    }

    if (tam > 0 && fread(contenido, 1, (size_t)tam, archivo)
        != (size_t)tam) {
        fclose(archivo);
        free(contenido);
        return MHD_NO;
    }
    fclose(archivo);

    respuesta = MHD_create_response_from_buffer(
        (size_t)tam, contenido, MHD_RESPMEM_MUST_FREE);
    if (respuesta == NULL) {
        free(contenido);
        return MHD_NO;
    }
    MHD_add_response_header(respuesta, "Content-Type", content_type);
    ret = MHD_queue_response(conexion, MHD_HTTP_OK, respuesta);
    MHD_destroy_response(respuesta);
    return ret;
}


static enum MHD_Result manejar_peticion(
    void* cls,
    struct MHD_Connection* conexion,
    const char* url,
    const char* metodo,
    const char* version,
    const char* datos,
    size_t* tam_datos,
    void** ptr)
{
    struct MHD_Response* respuesta;
    enum MHD_Result      ret;
    char                 salida[4096];
    const char* tipo_contenido;
    EstadoPeticion* estado;

    (void)cls;
    (void)version;

    /* ---- GET / → sirve la interfaz visual desde disco ---- */
    if (strcmp(metodo, "GET") == 0 && strcmp(url, "/") == 0) {
        return servir_archivo_estatico(conexion,
            "web/index.html", "text/html; charset=utf-8");
    }

    /* ---- GET /style.css ---- */
    if (strcmp(metodo, "GET") == 0 && strcmp(url, "/style.css") == 0) {
        return servir_archivo_estatico(conexion,
            "web/style.css", "text/css; charset=utf-8");
    }

    /* ---- GET /app.js ---- */
    if (strcmp(metodo, "GET") == 0 && strcmp(url, "/app.js") == 0) {
        return servir_archivo_estatico(conexion,
            "web/app.js", "application/javascript; charset=utf-8");
    }

    /* ---- GET /estado → info del dataset actual ---- */
    if (strcmp(metodo, "GET") == 0 && strcmp(url, "/estado") == 0) {
        EnterCriticalSection(&g_lock);
        snprintf(salida, sizeof(salida),
            "{\"filtradas\":%d,\"palabras\":%d}",
            g_total_filtradas, g_total_palabras);
        LeaveCriticalSection(&g_lock);

        respuesta = MHD_create_response_from_buffer(
            strlen(salida), salida, MHD_RESPMEM_MUST_COPY);
        if (respuesta == NULL) return MHD_NO;
        MHD_add_response_header(respuesta,
            "Content-Type", "application/json");
        MHD_add_response_header(respuesta,
            "Access-Control-Allow-Origin", "*");
        ret = MHD_queue_response(conexion, MHD_HTTP_OK, respuesta);
        MHD_destroy_response(respuesta);
        return ret;
    }

    /* ---- POST /verificar → verifica una sola contraseña ---- */
    if (strcmp(metodo, "POST") == 0 &&
        strcmp(url, "/verificar") == 0) {

        if (*ptr == NULL) {
            estado = crear_estado_peticion();
            if (estado == NULL) return MHD_NO;
            *ptr = estado;
            return MHD_YES;
        }

        estado = (EstadoPeticion*)(*ptr);

        if (estado->respuesta_enviada) {

            *tam_datos = 0;
            return MHD_YES;
        }

        if (*tam_datos > 0) {
            int r = acumular_datos(estado, datos, *tam_datos,
                TAM_MAXIMO_PETICION);
            *tam_datos = 0;

            if (r == 0) {
                return responder_error(conexion, estado,
                    MHD_HTTP_CONTENT_TOO_LARGE,
                    "{\"error\":\"Peticion demasiado grande\"}");
            }
            if (r == -1) {
                return responder_error(conexion, estado,
                    MHD_HTTP_INTERNAL_SERVER_ERROR,
                    "{\"error\":\"Memoria insuficiente\"}");
            }

            return MHD_YES;
        }


        if (estado->tam_usado == 0) {
            return responder_error(conexion, estado,
                MHD_HTTP_BAD_REQUEST,
                "{\"error\":\"Cuerpo de la peticion vacio\"}");
        }

        {
            char contrasena[MAX_PASSWORD_LEN];
            size_t len = estado->tam_usado < MAX_PASSWORD_LEN - 1
                ? estado->tam_usado : MAX_PASSWORD_LEN - 1;

            memcpy(contrasena, estado->buffer, len);
            contrasena[len] = '\0';
            contrasena[strcspn(contrasena, "\r\n")] = '\0';

            if (verificar_una(contrasena, salida, sizeof(salida)) != 0) {
                return responder_error(conexion, estado,
                    MHD_HTTP_INTERNAL_SERVER_ERROR, salida);
            }

            respuesta = MHD_create_response_from_buffer(
                strlen(salida), salida, MHD_RESPMEM_MUST_COPY);
            if (respuesta == NULL) return MHD_NO;
            MHD_add_response_header(respuesta,
                "Content-Type", "application/json");
            MHD_add_response_header(respuesta,
                "Access-Control-Allow-Origin", "*");
            ret = MHD_queue_response(conexion, MHD_HTTP_OK, respuesta);
            MHD_destroy_response(respuesta);
            estado->respuesta_enviada = 1;
            return ret;
        }
    }

    /* ---- POST /verificar-archivo → verifica varias contraseñas ---- */
    if (strcmp(metodo, "POST") == 0 &&
        strcmp(url, "/verificar-archivo") == 0) {

        if (*ptr == NULL) {
            estado = crear_estado_peticion();
            if (estado == NULL) return MHD_NO;
            *ptr = estado;
            return MHD_YES;
        }

        estado = (EstadoPeticion*)(*ptr);

        if (estado->respuesta_enviada) {
            *tam_datos = 0;
            return MHD_YES;
        }

        if (*tam_datos > 0) {
            int r = acumular_datos(estado, datos, *tam_datos,
                TAM_MAXIMO_PETICION);
            *tam_datos = 0;

            if (r == 0) {
                return responder_error(conexion, estado,
                    MHD_HTTP_CONTENT_TOO_LARGE,
                    "{\"error\":\"Archivo demasiado grande\"}");
            }
            if (r == -1) {
                return responder_error(conexion, estado,
                    MHD_HTTP_INTERNAL_SERVER_ERROR,
                    "{\"error\":\"Memoria insuficiente\"}");
            }
            return MHD_YES;
        }

        /* Cuerpo completo acumulado en estado->buffer /
           estado->tam_usado. Se procesa una unica vez aqui. */
        if (estado->tam_usado == 0) {
            return responder_error(conexion, estado,
                MHD_HTTP_BAD_REQUEST,
                "{\"error\":\"Archivo vacio\"}");
        }

        {

            size_t  cap_resultados = TAM_RESULTADOS_ARCHIVO;
            char* resultados = (char*)malloc(cap_resultados);
            char    linea[MAX_PASSWORD_LEN];
            char    resultado_uno[TAM_RESULTADO_UNO];
            size_t  pos_datos = 0;
            size_t  pos_res = 0;
            size_t  i = 0;
            int     primero = 1;
            int     escrito;
            const char* cuerpo = estado->buffer;
            size_t  tam_cuerpo = estado->tam_usado;

            if (resultados == NULL) {
                return responder_error(conexion, estado,
                    MHD_HTTP_INTERNAL_SERVER_ERROR,
                    "{\"error\":\"Memoria insuficiente para procesar\"}");
            }

            resultados[0] = '[';
            pos_res = 1;

            /* Procesar línea por línea */
            while (pos_datos < tam_cuerpo) {
                int linea_truncada = 0;

                i = 0;
                while (pos_datos < tam_cuerpo
                    && cuerpo[pos_datos] != '\n'
                    && i < MAX_PASSWORD_LEN - 1) {
                    linea[i++] = cuerpo[pos_datos++];
                }


                if (pos_datos < tam_cuerpo && cuerpo[pos_datos] != '\n') {
                    linea_truncada = 1;
                    while (pos_datos < tam_cuerpo
                        && cuerpo[pos_datos] != '\n') {
                        pos_datos++;
                    }
                }

                if (pos_datos < tam_cuerpo) pos_datos++; /* saltar '\n' */
                linea[i] = '\0';

                /* Quitar \r si viene de Windows */
                linea[strcspn(linea, "\r")] = '\0';

                if (strlen(linea) == 0) continue;
                (void)linea_truncada; /* se descarta silenciosamente */

                if (verificar_una(linea, resultado_uno,
                    sizeof(resultado_uno)) != 0) {

                    snprintf(resultado_uno, sizeof(resultado_uno),
                        "{\"error\":\"No se pudo verificar esta linea\"}");
                }

                {
                    size_t necesario =
                        strlen(resultado_uno) + (primero ? 0 : 1) + 2;


                    if (pos_res + necesario >= cap_resultados) {
                        size_t nueva_cap = cap_resultados * 2;
                        char* nuevo_buf;

                        while (nueva_cap < pos_res + necesario) {
                            nueva_cap *= 2;
                        }

                        nuevo_buf = (char*)realloc(resultados, nueva_cap);
                        if (nuevo_buf == NULL) {

                            break;
                        }
                        resultados = nuevo_buf;
                        cap_resultados = nueva_cap;
                    }
                }

                if (!primero) {
                    resultados[pos_res++] = ',';
                }
                primero = 0;

                escrito = snprintf(resultados + pos_res,
                    cap_resultados - pos_res,
                    "%s", resultado_uno);
                if (escrito > 0) {
                    pos_res += (size_t)escrito;
                }
            }


            if (pos_res + 2 > cap_resultados) {
                char* nuevo_buf = (char*)realloc(resultados, pos_res + 2);
                if (nuevo_buf == NULL) {
                    free(resultados);
                    return responder_error(conexion, estado,
                        MHD_HTTP_INTERNAL_SERVER_ERROR,
                        "{\"error\":\"Memoria insuficiente al cerrar "
                        "la respuesta\"}");
                }
                resultados = nuevo_buf;
                cap_resultados = pos_res + 2;
            }

            resultados[pos_res++] = ']';
            resultados[pos_res] = '\0';

            respuesta = MHD_create_response_from_buffer(
                pos_res, resultados, MHD_RESPMEM_MUST_FREE);
            if (respuesta == NULL) {
                free(resultados);
                return MHD_NO;
            }
            MHD_add_response_header(respuesta,
                "Content-Type", "application/json");
            MHD_add_response_header(respuesta,
                "Access-Control-Allow-Origin", "*");
            ret = MHD_queue_response(conexion, MHD_HTTP_OK, respuesta);
            MHD_destroy_response(respuesta);
            estado->respuesta_enviada = 1;
            return ret;
        }
    }

    /* ---- POST /cargar-dataset → reemplaza el dataset filtradas ---- */
    if (strcmp(metodo, "POST") == 0 &&
        strcmp(url, "/cargar-dataset") == 0) {

        if (*ptr == NULL) {
            estado = crear_estado_peticion();
            if (estado == NULL) return MHD_NO;
            *ptr = estado;
            return MHD_YES;
        }

        estado = (EstadoPeticion*)(*ptr);

        if (estado->respuesta_enviada) {
            *tam_datos = 0;
            return MHD_YES;
        }

        if (*tam_datos > 0) {
            int r = acumular_datos(estado, datos, *tam_datos,
                TAM_MAXIMO_PETICION);
            *tam_datos = 0;

            if (r == 0) {
                return responder_error(conexion, estado,
                    MHD_HTTP_CONTENT_TOO_LARGE,
                    "{\"ok\":false,"
                    "\"mensaje\":\"Dataset demasiado grande\"}");
            }
            if (r == -1) {
                return responder_error(conexion, estado,
                    MHD_HTTP_INTERNAL_SERVER_ERROR,
                    "{\"ok\":false,\"mensaje\":\"Memoria insuficiente\"}");
            }
            return MHD_YES;
        }

        if (estado->tam_usado == 0) {
            return responder_error(conexion, estado,
                MHD_HTTP_BAD_REQUEST,
                "{\"ok\":false,\"mensaje\":\"Sin contenido\"}");
        }

        {
            FILE* tmp = NULL;
            char** nueva = NULL;
            int     total_nueva = 0;

            tmp = fopen("dataset_nuevo.txt", "wb");
            if (tmp == NULL) {
                return responder_error(conexion, estado,
                    MHD_HTTP_INTERNAL_SERVER_ERROR,
                    "{\"ok\":false,\"mensaje\":\"Error al guardar\"}");
            }
            fwrite(estado->buffer, 1, estado->tam_usado, tmp);
            fclose(tmp);

            nueva = (char**)malloc(MAX_PASSWORDS * sizeof(char*));
            if (nueva == NULL) {
                snprintf(salida, sizeof(salida),
                    "{\"ok\":false,\"mensaje\":\"Memoria insuficiente\"}");
            }
            else {
                total_nueva = cargar_archivo("dataset_nuevo.txt",
                    nueva, MAX_PASSWORDS);
                if (total_nueva > 0) {
                    EnterCriticalSection(&g_lock);
                    liberar_memoria(g_filtradas, g_total_filtradas);
                    free(g_filtradas);
                    g_filtradas = nueva;
                    g_total_filtradas = total_nueva;
                    LeaveCriticalSection(&g_lock);
                    snprintf(salida, sizeof(salida),
                        "{\"ok\":true,\"total\":%d,"
                        "\"mensaje\":\"Dataset cargado correctamente\"}",
                        total_nueva);
                }
                else {

                    if (total_nueva > 0) {
                        liberar_memoria(nueva, total_nueva);
                    }
                    free(nueva);
                    snprintf(salida, sizeof(salida),
                        "{\"ok\":false,"
                        "\"mensaje\":\"No se pudo cargar el dataset\"}");
                }
            }

            respuesta = MHD_create_response_from_buffer(
                strlen(salida), salida, MHD_RESPMEM_MUST_COPY);
            if (respuesta == NULL) return MHD_NO;
            MHD_add_response_header(respuesta,
                "Content-Type", "application/json");

            ret = MHD_queue_response(conexion, MHD_HTTP_OK, respuesta);
            MHD_destroy_response(respuesta);
            estado->respuesta_enviada = 1;
            return ret;
        }
    }

    /* ---- 404 para cualquier otra ruta ---- */
    tipo_contenido = "application/json";
    snprintf(salida, sizeof(salida),
        "{\"error\":\"Ruta no encontrada\"}");
    respuesta = MHD_create_response_from_buffer(
        strlen(salida), salida, MHD_RESPMEM_MUST_COPY);
    if (respuesta == NULL) return MHD_NO;
    MHD_add_response_header(respuesta,
        "Content-Type", tipo_contenido);
    ret = MHD_queue_response(conexion, MHD_HTTP_NOT_FOUND, respuesta);
    MHD_destroy_response(respuesta);
    return ret;
}


/* Main */

int main(void) {
    struct sockaddr_in direccion_local;
    WSADATA datos_wsa;
    int resultado_wsa;

    SetConsoleOutputCP(CP_UTF8);

    signal(SIGINT, manejar_sigint);

    SetConsoleCtrlHandler(manejador_consola, TRUE);

    InitializeCriticalSection(&g_lock);


    resultado_wsa = WSAStartup(MAKEWORD(2, 2), &datos_wsa);
    if (resultado_wsa != 0) {
        fprintf(stderr,
            "Error: WSAStartup fallo (codigo %d)\n", resultado_wsa);
        DeleteCriticalSection(&g_lock);
        return 1;
    }

    printf("=== SDK Validacion de Contrasenas ===\n");

    g_filtradas = (char**)malloc(MAX_PASSWORDS * sizeof(char*));
    if (g_filtradas == NULL) {
        fprintf(stderr, "Error: no se pudo reservar memoria\n");
        WSACleanup();
        DeleteCriticalSection(&g_lock);
        return 1;
    }

    g_palabras = (char**)malloc(MAX_PALABRAS * sizeof(char*));
    if (g_palabras == NULL) {
        fprintf(stderr, "Error: no se pudo reservar memoria\n");
        free(g_filtradas);
        WSACleanup();
        DeleteCriticalSection(&g_lock);
        return 1;
    }

    g_total_filtradas = cargar_archivo(ARCHIVO_FILTRADAS,
        g_filtradas, MAX_PASSWORDS);
    if (g_total_filtradas < 0) {
        fprintf(stderr, "Error: no se pudo cargar el dataset\n");
        free(g_filtradas);
        free(g_palabras);
        WSACleanup();
        DeleteCriticalSection(&g_lock);
        return 1;
    }

    g_total_palabras = cargar_archivo(ARCHIVO_PALABRAS,
        g_palabras, MAX_PALABRAS);
    if (g_total_palabras < 0) {
        fprintf(stderr, "Error: no se pudo cargar palabras comunes\n");
        liberar_memoria(g_filtradas, g_total_filtradas);
        free(g_filtradas);
        free(g_palabras);
        WSACleanup();
        DeleteCriticalSection(&g_lock);
        return 1;
    }


    memset(&direccion_local, 0, sizeof(direccion_local));
    direccion_local.sin_family = AF_INET;
    direccion_local.sin_port = htons(PUERTO);
    direccion_local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    g_servidor = MHD_start_daemon(
        MHD_USE_INTERNAL_POLLING_THREAD,
        PUERTO,
        NULL, NULL,
        &manejar_peticion, NULL,
        MHD_OPTION_SOCK_ADDR, &direccion_local,
        MHD_OPTION_NOTIFY_COMPLETED, &solicitud_completada, NULL,
        MHD_OPTION_END);

    if (g_servidor == NULL) {
        fprintf(stderr, "Error: no se pudo iniciar el servidor\n");
        liberar_memoria(g_filtradas, g_total_filtradas);
        liberar_memoria(g_palabras, g_total_palabras);
        free(g_filtradas);
        free(g_palabras);
        WSACleanup();
        DeleteCriticalSection(&g_lock);
        return 1;
    }

    printf("Servidor corriendo en http://127.0.0.1:%d (solo local)\n",
        PUERTO);
    printf("Presiona Enter o Ctrl+C para detener...\n");

    while (g_seguir_corriendo) {
        int tecla = getchar();
        if (tecla == '\n' || tecla == EOF) {
            break;
        }
    }

    /* Detener el servidor */
    if (g_servidor != NULL) {
        MHD_stop_daemon(g_servidor);
        g_servidor = NULL;
    }

    liberar_memoria(g_filtradas, g_total_filtradas);
    liberar_memoria(g_palabras, g_total_palabras);
    free(g_filtradas);
    free(g_palabras);

    WSACleanup();
    DeleteCriticalSection(&g_lock);

    printf("Servidor detenido. Memoria liberada.\n");
    return 0;
}