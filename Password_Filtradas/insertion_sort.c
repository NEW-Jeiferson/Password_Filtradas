/* Implementación del algoritmo Insertion Sort para ordenar el dataset de contraseñas filtradas mientras se carga línea por línea desde el archivo */

#include "Password.h"

int insertion_sort(char** lista, int total) {
    int i, j;
    char* clave;

    if (lista == NULL) {
        fprintf(stderr, "Error: lista no puede ser NULL\n");
        return -1;
    }
    if (total <= 0) {
        fprintf(stderr, "Error: total debe ser mayor a 0\n");
        return -1;
    }

    if (total == 1) {
        return 0;
    }

    for (i = 1; i < total; i++) {

        if (lista[i] == NULL) {
            fprintf(stderr, "Error: elemento NULL en posicion %d\n", i);
            return -1;
        }

        clave = lista[i];
        j = i - 1;

        while (j >= 0 && lista[j] != NULL && strcmp(lista[j], clave) > 0) {
            lista[j + 1] = lista[j];
            j--;
        }

        lista[j + 1] = clave;
    }

    return 0;
}