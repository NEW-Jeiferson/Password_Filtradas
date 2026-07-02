/* Verificacion end-to-end de una contrasena */

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>

#include "verificacion.h"
#include "Password.h"
#include "estado_global.h"

/* Función para escapar caracteres especiales en una cadena JSON.
   Se reemplazan los caracteres especiales por sus secuencias de escape correspondientes.
   La cadena resultante se almacena en 'destino', que debe tener suficiente espacio.
   Se asegura de no exceder el tamaño máximo 'tam' del destino. */
static void escapar_json(const char* origen, char* destino, size_t tam) {
    size_t i = 0;
    size_t j = 0;

    if (origen == NULL || destino == NULL || tam == 0) return;

	/* Escapar caracteres especiales para JSON */
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

/* Función principal para verificar la seguridad de una contraseña.
   Se realiza una búsqueda binaria en el dataset de contraseñas filtradas y una búsqueda lineal en el dataset de palabras comunes.
   Se calcula un puntaje de fortaleza y se genera un JSON con los resultados.
   Devuelve 0 si la verificación fue exitosa, -1 si hubo un error. */
int verificar_una(const char* contrasena, char* salida, size_t tam) {
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
