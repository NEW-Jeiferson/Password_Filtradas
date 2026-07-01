/* Servidor HTTP: ruteo y manejadores de endpoints */

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>

#include "servidor_http.h"
#include "Password.h"
#include "estado_global.h"
#include "estado_peticion.h"
#include "carga_datos.h"
#include "verificacion.h"

#define TAM_RESULTADO_UNO  1024
#define TAM_RESULTADOS_ARCHIVO  65536
#define TAM_MAXIMO_PETICION  (10 * 1024 * 1024)


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


/* ---- GET /estado → info del dataset actual ---- */
static enum MHD_Result manejar_get_estado(struct MHD_Connection* conexion) {
    struct MHD_Response* respuesta;
    enum MHD_Result      ret;
    char                 salida[4096];

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
static enum MHD_Result manejar_post_verificar(
    struct MHD_Connection* conexion,
    const char* datos,
    size_t* tam_datos,
    void** ptr) {
    struct MHD_Response* respuesta;
    enum MHD_Result      ret;
    char                 salida[4096];
    EstadoPeticion* estado;

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
static enum MHD_Result manejar_post_verificar_archivo(
    struct MHD_Connection* conexion,
    const char* datos,
    size_t* tam_datos,
    void** ptr) {
    struct MHD_Response* respuesta;
    enum MHD_Result      ret;
    EstadoPeticion* estado;

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
static enum MHD_Result manejar_post_cargar_dataset(
    struct MHD_Connection* conexion,
    const char* datos,
    size_t* tam_datos,
    void** ptr) {
    struct MHD_Response* respuesta;
    enum MHD_Result      ret;
    char                 salida[4096];
    EstadoPeticion* estado;

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
static enum MHD_Result manejar_no_encontrado(
    struct MHD_Connection* conexion) {
    struct MHD_Response* respuesta;
    enum MHD_Result      ret;
    const char* salida = "{\"error\":\"Ruta no encontrada\"}";

    respuesta = MHD_create_response_from_buffer(
        strlen(salida), (void*)salida, MHD_RESPMEM_PERSISTENT);
    if (respuesta == NULL) return MHD_NO;
    MHD_add_response_header(respuesta,
        "Content-Type", "application/json");
    ret = MHD_queue_response(conexion, MHD_HTTP_NOT_FOUND, respuesta);
    MHD_destroy_response(respuesta);
    return ret;
}



enum MHD_Result manejar_peticion(
    void* cls,
    struct MHD_Connection* conexion,
    const char* url,
    const char* metodo,
    const char* version,
    const char* datos,
    size_t* tam_datos,
    void** ptr)
{
    (void)cls;
    (void)version;

    if (strcmp(metodo, "GET") == 0) {
        if (strcmp(url, "/") == 0) {
            return servir_archivo_estatico(conexion,
                "web/index.html", "text/html; charset=utf-8");
        }
        if (strcmp(url, "/style.css") == 0) {
            return servir_archivo_estatico(conexion,
                "web/style.css", "text/css; charset=utf-8");
        }
        if (strcmp(url, "/app.js") == 0) {
            return servir_archivo_estatico(conexion,
                "web/app.js", "application/javascript; charset=utf-8");
        }
        if (strcmp(url, "/estado") == 0) {
            return manejar_get_estado(conexion);
        }
    }

    if (strcmp(metodo, "POST") == 0) {
        if (strcmp(url, "/verificar") == 0) {
            return manejar_post_verificar(conexion, datos, tam_datos, ptr);
        }
        if (strcmp(url, "/verificar-archivo") == 0) {
            return manejar_post_verificar_archivo(conexion, datos,
                tam_datos, ptr);
        }
        if (strcmp(url, "/cargar-dataset") == 0) {
            return manejar_post_cargar_dataset(conexion, datos,
                tam_datos, ptr);
        }
    }

    return manejar_no_encontrado(conexion);
}
