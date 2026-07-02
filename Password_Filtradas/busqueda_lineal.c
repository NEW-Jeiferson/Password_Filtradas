/* Implementación del algoritmo de Búsqueda Linealpara detectar si una contraseña contiene palabras comunes de diccionario en cualquier posición */

#include "Password.h"

/* Función para realizar una búsqueda lineal en un arreglo de cadenas para verificar si alguna palabra común está contenida dentro de la contraseña proporcionada.
   Devuelve el índice de la primera palabra encontrada, -1 si no se encuentra ninguna coincidencia, o -2 si ocurre un error (como punteros NULL o tamaño inválido). */
int busqueda_lineal(char** palabras, int total, const char* contrasena) {
    int i;
    char contrasena_lower[MAX_PASSWORD_LEN];
    char palabra_lower[MAX_PASSWORD_LEN];
    int j;

    if (palabras == NULL) {
        fprintf(stderr, "Error: lista de palabras no puede ser NULL\n");
        return -2;
    }
    if (contrasena == NULL) {
        fprintf(stderr, "Error: contrasena no puede ser NULL\n");
        return -2;
    }
    if (total <= 0) {
        fprintf(stderr, "Error: total debe ser mayor a 0\n");
        return -2;
    }

    for (j = 0; contrasena[j] && j < MAX_PASSWORD_LEN - 1; j++) {
        contrasena_lower[j] = (char)tolower((unsigned char)contrasena[j]);
    }
    contrasena_lower[j] = '\0';

    for (i = 0; i < total; i++) {

        if (palabras[i] == NULL) {
            fprintf(stderr, "Error: palabra NULL en posicion %d\n", i);
            continue;
        }

        for (j = 0; palabras[i][j] && j < MAX_PASSWORD_LEN - 1; j++) {
            palabra_lower[j] = (char)tolower((unsigned char)palabras[i][j]);
        }
        palabra_lower[j] = '\0';

        if (palabra_lower[0] == '\0') {
            continue;
        }

        if (strstr(contrasena_lower, palabra_lower) != NULL) {
            return i;
        }
    }

    return -1;
}