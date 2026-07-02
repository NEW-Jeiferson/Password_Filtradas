/* Descripción: Implementación del algoritmo Insertion Sort para ordenar el dataset de contraseñas filtradas mientras se carga línea por línea desde el archivo */

#include "Password.h"

/* Función para ordenar un arreglo de cadenas usando el algoritmo Insertion Sort.
   Se asume que el arreglo ya contiene elementos ordenados hasta la posición total - 2,
   y se inserta el elemento en la posición total - 1 en su lugar correcto.
   Devuelve 0 si la operación fue exitosa, o -1 si hubo un error. */
int insertion_sort(char** lista, int total) {
    int   j;
    char* clave;

    if (lista == NULL) {
        fprintf(stderr, "Error: lista no puede ser NULL\n");
        return -1;
    }
    if (total <= 0) {
        fprintf(stderr, "Error: total debe ser mayor a 0\n");
        return -1;
    }
    if (lista[total - 1] == NULL) {
        fprintf(stderr, "Error: el nuevo elemento no puede ser NULL\n");
        return -1;
    }

    if (total == 1) {
        return 0;
    }

	/* Tomar el último elemento como clave y encontrar su posición correcta */
    clave = lista[total - 1];
    j = total - 2;

	/* Mover los elementos mayores que clave una posición adelante */
    while (j >= 0 && lista[j] != NULL && strcmp(lista[j], clave) > 0) {
        lista[j + 1] = lista[j];
        j--;
    }

    lista[j + 1] = clave;

    return 0;
}