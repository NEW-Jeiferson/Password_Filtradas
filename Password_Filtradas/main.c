/* Servidor HTTP local usando libmicrohttpd, carga los datasets al iniciar y expone endpoints para verificar contraseñas desde el navegador */

#include "Password.h"
#include <microhttpd.h>
#include <windows.h>

#define PUERTO 8080

static char** g_filtradas = NULL;
static int      g_total_filtradas = 0;
static char** g_palabras = NULL;
static int      g_total_palabras = 0;

/* Funciones de Carga y Memoria */
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

        /* Insertion Sort en cada inserción */
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


/* Funciones de Verificacion */

static void verificar_una(const char* contrasena,
    char* salida, size_t tam) {
    int resultado_binaria;
    int resultado_lineal;
    int puntaje;
    const char* nivel;
    const char* segura;

    if (contrasena == NULL || salida == NULL) return;

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
            contrasena);
        return;
    }

    resultado_lineal = busqueda_lineal(g_palabras,
        g_total_palabras,
        contrasena);
    if (resultado_lineal >= 0) {
        snprintf(salida, tam,
            "{\"segura\":false,"
            "\"filtrada\":false,"
            "\"palabra_comun\":true,"
            "\"palabra\":\"%s\","
            "\"puntaje\":0,"
            "\"nivel\":\"Debil\","
            "\"contrasena\":\"%s\"}",
            g_palabras[resultado_lineal],
            contrasena);
        return;
    }

    puntaje = calcular_fortaleza(contrasena);
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
        segura, puntaje, nivel, contrasena);
}


/* Manejo de peticiones Http */

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
    enum MHD_Result     ret;
    char                salida[4096];
    const char* tipo_contenido;

    (void)cls;
    (void)version;


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
        MHD_add_response_header(respuesta,
            "Content-Type", "text/html; charset=utf-8");
        ret = MHD_queue_response(conexion, MHD_HTTP_OK, respuesta);
        MHD_destroy_response(respuesta);
        return ret;
    }


    if (strcmp(metodo, "GET") == 0 && strcmp(url, "/estado") == 0) {
        snprintf(salida, sizeof(salida),
            "{\"filtradas\":%d,\"palabras\":%d}",
            g_total_filtradas, g_total_palabras);

        respuesta = MHD_create_response_from_buffer(
            strlen(salida), salida, MHD_RESPMEM_MUST_COPY);
        MHD_add_response_header(respuesta,
            "Content-Type", "application/json");
        MHD_add_response_header(respuesta,
            "Access-Control-Allow-Origin", "*");
        ret = MHD_queue_response(conexion, MHD_HTTP_OK, respuesta);
        MHD_destroy_response(respuesta);
        return ret;
    }


    if (strcmp(metodo, "POST") == 0 &&
        strcmp(url, "/verificar") == 0) {

        if (*tam_datos > 0) {
            char contrasena[MAX_PASSWORD_LEN];
            size_t len = *tam_datos < MAX_PASSWORD_LEN - 1
                ? *tam_datos : MAX_PASSWORD_LEN - 1;
            strncpy(contrasena, datos, len);
            contrasena[len] = '\0';

            contrasena[strcspn(contrasena, "\r\n")] = '\0';

            verificar_una(contrasena, salida, sizeof(salida));
            *tam_datos = 0;

            respuesta = MHD_create_response_from_buffer(
                strlen(salida), salida, MHD_RESPMEM_MUST_COPY);
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


    if (strcmp(metodo, "POST") == 0 &&
        strcmp(url, "/verificar-archivo") == 0) {

        if (*tam_datos > 0) {
            char   resultados[65536];
            char   linea[MAX_PASSWORD_LEN];
            char   resultado_uno[1024];
            size_t pos_datos = 0;
            size_t pos_res = 0;
            size_t i = 0;
            int    primero = 1;

            pos_res += snprintf(resultados + pos_res,
                sizeof(resultados) - pos_res, "[");

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

                if (!primero) {
                    pos_res += snprintf(resultados + pos_res,
                        sizeof(resultados) - pos_res, ",");
                }
                primero = 0;

                verificar_una(linea, resultado_uno,
                    sizeof(resultado_uno));
                pos_res += snprintf(resultados + pos_res,
                    sizeof(resultados) - pos_res,
                    "%s", resultado_uno);
            }

            pos_res += snprintf(resultados + pos_res,
                sizeof(resultados) - pos_res, "]");

            *tam_datos = 0;

            respuesta = MHD_create_response_from_buffer(
                strlen(resultados), resultados,
                MHD_RESPMEM_MUST_COPY);
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


    if (strcmp(metodo, "POST") == 0 &&
        strcmp(url, "/cargar-dataset") == 0) {

        if (*tam_datos > 0) {
            FILE* tmp = NULL;
            char** nueva = NULL;
            int    total_nueva = 0;

            /* Guardar el contenido recibido en un archivo temporal */
            tmp = fopen("dataset_nuevo.txt", "wb");
            if (tmp == NULL) {
                const char* err = "{\"ok\":false,"
                    "\"mensaje\":\"Error al guardar\"}";
                respuesta = MHD_create_response_from_buffer(
                    strlen(err), (void*)err,
                    MHD_RESPMEM_PERSISTENT);
                MHD_add_response_header(respuesta,
                    "Content-Type", "application/json");
                ret = MHD_queue_response(conexion,
                    MHD_HTTP_INTERNAL_SERVER_ERROR, respuesta);
                MHD_destroy_response(respuesta);
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
                    free(nueva);
                    snprintf(salida, sizeof(salida),
                        "{\"ok\":false,"
                        "\"mensaje\":\"No se pudo cargar el dataset\"}");
                }
            }

            *tam_datos = 0;

            respuesta = MHD_create_response_from_buffer(
                strlen(salida), salida, MHD_RESPMEM_MUST_COPY);
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


    tipo_contenido = "application/json";
    snprintf(salida, sizeof(salida),
        "{\"error\":\"Ruta no encontrada\"}");
    respuesta = MHD_create_response_from_buffer(
        strlen(salida), salida, MHD_RESPMEM_MUST_COPY);
    MHD_add_response_header(respuesta,
        "Content-Type", tipo_contenido);
    ret = MHD_queue_response(conexion, MHD_HTTP_NOT_FOUND, respuesta);
    MHD_destroy_response(respuesta);
    return ret;
}

/* Main */

int main(void) {
    struct MHD_Daemon* servidor = NULL;

    SetConsoleOutputCP(CP_UTF8);

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

    servidor = MHD_start_daemon(
        MHD_USE_INTERNAL_POLLING_THREAD,
        PUERTO,
        NULL, NULL,
        &manejar_peticion, NULL,
        MHD_OPTION_END);

    if (servidor == NULL) {
        fprintf(stderr, "Error: no se pudo iniciar el servidor\n");
        liberar_memoria(g_filtradas, g_total_filtradas);
        liberar_memoria(g_palabras, g_total_palabras);
        free(g_filtradas);
        free(g_palabras);
        return 1;
    }

    printf("Servidor corriendo en http://localhost:%d\n", PUERTO);
    printf("Presiona Enter para detener...\n");

    getchar();

    /* Detener el servidor */
    MHD_stop_daemon(servidor);

    liberar_memoria(g_filtradas, g_total_filtradas);
    liberar_memoria(g_palabras, g_total_palabras);
    free(g_filtradas);
    free(g_palabras);

    printf("Servidor detenido. Memoria liberada.\n");
    return 0;
}