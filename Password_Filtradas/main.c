/* Servidor HTTP local usando libmicrohttpd, carga los datasets al iniciar y expone endpoints para verificar contraseñas desde el navegador */

#include "Password.h"
#include <microhttpd.h>
#include <windows.h>
#include <signal.h>

#define PUERTO 8080

#define TAM_RESULTADO_UNO  1024

#define TAM_RESULTADOS_ARCHIVO  65536

#define TAM_MAXIMO_PETICION  (10 * 1024 * 1024)

static char** g_filtradas = NULL;
static int      g_total_filtradas = 0;
static char** g_palabras = NULL;
static int      g_total_palabras = 0;

/* Puntero global al servidor, necesario para poder detenerlo desde
   el manejador de la señal SIGINT (Ctrl+C). Sin esto, cerrar el
   programa de golpe deja el servidor y la memoria sin liberar. */
static struct MHD_Daemon* g_servidor = NULL;

/* Bandera que indica si el programa debe seguir corriendo. Se pone
   en 0 desde el manejador de SIGINT para salir del bucle principal
   de forma ordenada en vez de terminar el proceso abruptamente. */
static volatile sig_atomic_t g_seguir_corriendo = 1;


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

    while (fgets(buffer, MAX_PASSWORD_LEN, archivo) != NULL
        && total < max) {

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

        insertion_sort(lista, total);
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

static void verificar_una(const char* contrasena,
    char* salida, size_t tam) {
    int          resultado_binaria;
    int          resultado_lineal;
    int          puntaje;
    const char* nivel;
    const char* segura;
    char         contrasena_escapada[MAX_PASSWORD_LEN * 2];
    char         palabra_escapada[MAX_PASSWORD_LEN * 2];

    if (contrasena == NULL || salida == NULL || tam == 0) return;

    escapar_json(contrasena, contrasena_escapada,
        sizeof(contrasena_escapada));

    resultado_binaria = busqueda_binaria(g_filtradas,
        g_total_filtradas,
        contrasena);
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
        return;
    }

    resultado_lineal = busqueda_lineal(g_palabras,
        g_total_palabras,
        contrasena);
    if (resultado_lineal >= 0) {
        escapar_json(g_palabras[resultado_lineal], palabra_escapada,
            sizeof(palabra_escapada));
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
        return;
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
}


/* Manejador de la señal SIGINT (Ctrl+C) */

static void manejar_sigint(int senal) {
    (void)senal;
    g_seguir_corriendo = 0;
}


/* Manejador de Peticiones http */

static int revisar_tamano_peticion(size_t tam_datos) {
    return tam_datos <= TAM_MAXIMO_PETICION;
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

    (void)cls;
    (void)version;
    (void)ptr;

    /* ---- GET / → sirve el HTML principal ---- */
    if (strcmp(metodo, "GET") == 0 && strcmp(url, "/") == 0) {
        const char* html =
            "<!DOCTYPE html><html lang='es'><head>"
            "<meta charset='UTF-8'>"
            "<meta name='viewport' content='width=device-width'>"
            "<title>SDK Validacion</title>"
            "<link rel='stylesheet' href='style.css'>"
            "</head><body>"
            "<div id='app'></div>"
            "<script src='app.js'></script>"
            "</body></html>";

        respuesta = MHD_create_response_from_buffer(
            strlen(html), (void*)html, MHD_RESPMEM_PERSISTENT);
        if (respuesta == NULL) return MHD_NO;
        MHD_add_response_header(respuesta,
            "Content-Type", "text/html; charset=utf-8");
        ret = MHD_queue_response(conexion, MHD_HTTP_OK, respuesta);
        MHD_destroy_response(respuesta);
        return ret;
    }

    /* ---- GET /estado → info del dataset actual ---- */
    if (strcmp(metodo, "GET") == 0 && strcmp(url, "/estado") == 0) {
        snprintf(salida, sizeof(salida),
            "{\"filtradas\":%d,\"palabras\":%d}",
            g_total_filtradas, g_total_palabras);

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

        if (*tam_datos > 0) {

            /* Gestor de emergencia: si el cuerpo de la petición es
               anormalmente grande, se rechaza antes de procesarlo. */
            if (!revisar_tamano_peticion(*tam_datos)) {
                const char* err =
                    "{\"error\":\"Peticion demasiado grande\"}";
                respuesta = MHD_create_response_from_buffer(
                    strlen(err), (void*)err, MHD_RESPMEM_PERSISTENT);
                if (respuesta == NULL) return MHD_NO;
                MHD_add_response_header(respuesta,
                    "Content-Type", "application/json");
                ret = MHD_queue_response(conexion,
                    MHD_HTTP_CONTENT_TOO_LARGE, respuesta);
                MHD_destroy_response(respuesta);
                *tam_datos = 0;
                return ret;
            }

            char    contrasena[MAX_PASSWORD_LEN];
            size_t  len = *tam_datos < MAX_PASSWORD_LEN - 1
                ? *tam_datos : MAX_PASSWORD_LEN - 1;

            strncpy(contrasena, datos, len);
            contrasena[len] = '\0';
            contrasena[strcspn(contrasena, "\r\n")] = '\0';

            verificar_una(contrasena, salida, sizeof(salida));
            *tam_datos = 0;

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
        return MHD_YES;
    }

    /* ---- POST /verificar-archivo → verifica varias contraseñas ---- */
    if (strcmp(metodo, "POST") == 0 &&
        strcmp(url, "/verificar-archivo") == 0) {

        if (*tam_datos > 0) {

            /* Gestor de emergencia: mismo límite que en /verificar,
               aquí es aún más importante porque este endpoint es
               justo el que procesa archivos grandes (el caso que
               mencionaron como riesgo de "explotar" con muchos
               datos). Se corta aquí antes de reservar memoria. */
            if (!revisar_tamano_peticion(*tam_datos)) {
                const char* err =
                    "{\"error\":\"Archivo demasiado grande\"}";
                respuesta = MHD_create_response_from_buffer(
                    strlen(err), (void*)err, MHD_RESPMEM_PERSISTENT);
                if (respuesta == NULL) return MHD_NO;
                MHD_add_response_header(respuesta,
                    "Content-Type", "application/json");
                ret = MHD_queue_response(conexion,
                    MHD_HTTP_CONTENT_TOO_LARGE, respuesta);
                MHD_destroy_response(respuesta);
                *tam_datos = 0;
                return ret;
            }

            /* Reservado en el heap, no en el stack: 65KB en el
               stack es riesgoso (advertencia C6262 del compilador,
               posible stack overflow). malloc lo evita. */
            char* resultados = (char*)malloc(TAM_RESULTADOS_ARCHIVO);
            char    linea[MAX_PASSWORD_LEN];
            char    resultado_uno[TAM_RESULTADO_UNO];
            size_t  pos_datos = 0;
            size_t  pos_res = 0;
            size_t  i = 0;
            int     primero = 1;
            int     escrito;

            if (resultados == NULL) {
                const char* err =
                    "{\"error\":\"Memoria insuficiente para procesar\"}";
                respuesta = MHD_create_response_from_buffer(
                    strlen(err), (void*)err, MHD_RESPMEM_PERSISTENT);
                if (respuesta == NULL) return MHD_NO;
                MHD_add_response_header(respuesta,
                    "Content-Type", "application/json");
                ret = MHD_queue_response(conexion,
                    MHD_HTTP_INTERNAL_SERVER_ERROR, respuesta);
                MHD_destroy_response(respuesta);
                *tam_datos = 0;
                return ret;
            }

            resultados[0] = '[';
            pos_res = 1;

            /* Procesar línea por línea */
            while (pos_datos < *tam_datos) {
                i = 0;
                while (pos_datos < *tam_datos
                    && datos[pos_datos] != '\n'
                    && i < MAX_PASSWORD_LEN - 1) {
                    linea[i++] = datos[pos_datos++];
                }
                if (pos_datos < *tam_datos) pos_datos++;
                linea[i] = '\0';

                /* Quitar \r si viene de Windows */
                linea[strcspn(linea, "\r")] = '\0';

                if (strlen(linea) == 0) continue;

                verificar_una(linea, resultado_uno,
                    sizeof(resultado_uno));

                /* Calcular cuánto espacio queda ANTES de escribir,
                   y detener el procesamiento si ya no entra más,
                   en vez de dejar que pos_res supere sizeof(resultados)
                   (eso causaría un underflow en la resta de tamaños
                   sin signo, y un desborde real en el buffer). */
                {
                    size_t espacio_restante =
                        TAM_RESULTADOS_ARCHIVO - pos_res;
                    size_t necesario =
                        strlen(resultado_uno) + (primero ? 0 : 1) + 2;
                    /* +1 por la coma si no es el primero,
                       +2 de margen por el cierre "]" y el nulo */

                    if (necesario >= espacio_restante) {
                        /* Ya no hay espacio: cerrar el JSON aquí
                           mismo y detener el procesamiento, en vez
                           de seguir escribiendo fuera de límites. */
                        break;
                    }
                }

                if (!primero) {
                    resultados[pos_res++] = ',';
                }
                primero = 0;

                escrito = snprintf(resultados + pos_res,
                    TAM_RESULTADOS_ARCHIVO - pos_res,
                    "%s", resultado_uno);
                if (escrito > 0) {
                    pos_res += (size_t)escrito;
                }
            }

            resultados[pos_res++] = ']';
            resultados[pos_res] = '\0';

            *tam_datos = 0;

            respuesta = MHD_create_response_from_buffer(
                pos_res, resultados, MHD_RESPMEM_MUST_FREE);
            /* MHD_RESPMEM_MUST_FREE: libmicrohttpd se encarga de
               hacer free() sobre 'resultados' cuando ya no lo
               necesite. Evita que nosotros tengamos que liberarlo
               aquí y nos arriesguemos a un use-after-free o a
               olvidarlo (memory leak). */
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
            return ret;
        }
        return MHD_YES;
    }

    /* ---- POST /cargar-dataset → reemplaza el dataset filtradas ---- */
    if (strcmp(metodo, "POST") == 0 &&
        strcmp(url, "/cargar-dataset") == 0) {

        if (*tam_datos > 0) {

            /* Gestor de emergencia: mismo límite aquí también,
               porque este endpoint escribe el contenido recibido
               directo a disco con fwrite antes de procesarlo. */
            if (!revisar_tamano_peticion(*tam_datos)) {
                const char* err =
                    "{\"ok\":false,"
                    "\"mensaje\":\"Dataset demasiado grande\"}";
                respuesta = MHD_create_response_from_buffer(
                    strlen(err), (void*)err, MHD_RESPMEM_PERSISTENT);
                if (respuesta == NULL) return MHD_NO;
                MHD_add_response_header(respuesta,
                    "Content-Type", "application/json");
                ret = MHD_queue_response(conexion,
                    MHD_HTTP_CONTENT_TOO_LARGE, respuesta);
                MHD_destroy_response(respuesta);
                *tam_datos = 0;
                return ret;
            }

            FILE* tmp = NULL;
            char** nueva = NULL;
            int     total_nueva = 0;

            tmp = fopen("dataset_nuevo.txt", "wb");
            if (tmp == NULL) {
                const char* err = "{\"ok\":false,"
                    "\"mensaje\":\"Error al guardar\"}";
                respuesta = MHD_create_response_from_buffer(
                    strlen(err), (void*)err, MHD_RESPMEM_PERSISTENT);
                if (respuesta == NULL) return MHD_NO;
                MHD_add_response_header(respuesta,
                    "Content-Type", "application/json");
                ret = MHD_queue_response(conexion,
                    MHD_HTTP_INTERNAL_SERVER_ERROR, respuesta);
                MHD_destroy_response(respuesta);
                *tam_datos = 0;
                return ret;
            }
            fwrite(datos, 1, *tam_datos, tmp);
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
                    /* Liberar el dataset anterior y reemplazar */
                    liberar_memoria(g_filtradas, g_total_filtradas);
                    free(g_filtradas);
                    g_filtradas = nueva;
                    g_total_filtradas = total_nueva;
                    snprintf(salida, sizeof(salida),
                        "{\"ok\":true,\"total\":%d,"
                        "\"mensaje\":\"Dataset cargado correctamente\"}",
                        total_nueva);
                }
                else {
                    /* total_nueva puede ser 0 (archivo vacio) o
                       el numero de lineas cargadas antes de un
                       fallo de memoria. En ambos casos hay que
                       liberar lo que sí llegó a reservarse con
                       malloc antes de descartar 'nueva', si no
                       se queda esa memoria sin liberar. */
                    if (total_nueva > 0) {
                        liberar_memoria(nueva, total_nueva);
                    }
                    free(nueva);
                    snprintf(salida, sizeof(salida),
                        "{\"ok\":false,"
                        "\"mensaje\":\"No se pudo cargar el dataset\"}");
                }
            }

            *tam_datos = 0;

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
        return MHD_YES;
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
    SetConsoleOutputCP(CP_UTF8);

    /* Gestor de emergencia: registra manejar_sigint para que al
       presionar Ctrl+C el programa no se cierre de golpe, sino que
       salga del bucle principal y libere memoria y el servidor
       correctamente antes de terminar. */
    signal(SIGINT, manejar_sigint);

    printf("=== SDK Validacion de Contrasenas ===\n");

    g_filtradas = (char**)malloc(MAX_PASSWORDS * sizeof(char*));
    if (g_filtradas == NULL) {
        fprintf(stderr, "Error: no se pudo reservar memoria\n");
        return 1;
    }

    g_palabras = (char**)malloc(MAX_PALABRAS * sizeof(char*));
    if (g_palabras == NULL) {
        fprintf(stderr, "Error: no se pudo reservar memoria\n");
        free(g_filtradas);
        return 1;
    }

    g_total_filtradas = cargar_archivo(ARCHIVO_FILTRADAS,
        g_filtradas, MAX_PASSWORDS);
    if (g_total_filtradas < 0) {
        fprintf(stderr, "Error: no se pudo cargar el dataset\n");
        free(g_filtradas);
        free(g_palabras);
        return 1;
    }

    g_total_palabras = cargar_archivo(ARCHIVO_PALABRAS,
        g_palabras, MAX_PALABRAS);
    if (g_total_palabras < 0) {
        fprintf(stderr, "Error: no se pudo cargar palabras comunes\n");
        liberar_memoria(g_filtradas, g_total_filtradas);
        free(g_filtradas);
        free(g_palabras);
        return 1;
    }

    g_servidor = MHD_start_daemon(
        MHD_USE_INTERNAL_POLLING_THREAD,
        PUERTO,
        NULL, NULL,
        &manejar_peticion, NULL,
        MHD_OPTION_END);

    if (g_servidor == NULL) {
        fprintf(stderr, "Error: no se pudo iniciar el servidor\n");
        liberar_memoria(g_filtradas, g_total_filtradas);
        liberar_memoria(g_palabras, g_total_palabras);
        free(g_filtradas);
        free(g_palabras);
        return 1;
    }

    printf("Servidor corriendo en http://localhost:%d\n", PUERTO);
    printf("Presiona Enter o Ctrl+C para detener...\n");

    /* Bucle de espera: se mantiene corriendo hasta que el usuario
       presione Enter (getchar) o Ctrl+C (que pone en 0 la bandera
       g_seguir_corriendo desde manejar_sigint). Se revisa la
       bandera en cada vuelta para no quedar atascado si la señal
       llega mientras getchar() está esperando. */
    while (g_seguir_corriendo) {
        int tecla = getchar();
        if (tecla == '\n' || tecla == EOF) {
            break;
        }
    }

    /* Detener el servidor */
    MHD_stop_daemon(g_servidor);

    liberar_memoria(g_filtradas, g_total_filtradas);
    liberar_memoria(g_palabras, g_total_palabras);
    free(g_filtradas);
    free(g_palabras);

    printf("Servidor detenido. Memoria liberada.\n");
    return 0;
}