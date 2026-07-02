/* Implementación del algoritmo de Búsqueda Binaria para verificar si una contraseña exacta existe dentro del dataset ordenado de contraseñas filtradas */

#include "Password.h"

/* Función para realizar una búsqueda binaria en un arreglo de cadenas ordenadas para verificar si la contraseña exacta está presente en el dataset de contraseñas filtradas.
   Devuelve el índice de la contraseña encontrada, -1 si no se encuentra, o -2 si ocurre un error (como punteros NULL o tamaño inválido). */
int busqueda_binaria(char** lista, int total, const char* objetivo) {
    int inicio;
    int fin;
    int medio;
    int comparacion;

    if (lista == NULL) {
        fprintf(stderr, "Error: lista no puede ser NULL\n");
        return -2;
    }
    if (objetivo == NULL) {
        fprintf(stderr, "Error: objetivo no puede ser NULL\n");
        return -2;
    }
    if (total <= 0) {
        fprintf(stderr, "Error: total debe ser mayor a 0\n");
        return -2;
    }

    inicio = 0;
    fin = total - 1;

	/* Búsqueda binaria */
    while (inicio <= fin) {
        medio = inicio + (fin - inicio) / 2;

        if (lista[medio] == NULL) {
            fprintf(stderr, "Error: elemento NULL en posicion %d\n", medio);
            return -2;
        }

        comparacion = strcmp(lista[medio], objetivo);

        if (comparacion == 0) {
            return medio;
        }
        else if (comparacion < 0) {
            inicio = medio + 1;
        }
        else {
            fin = medio - 1;
        }
    }

    return -1;
}