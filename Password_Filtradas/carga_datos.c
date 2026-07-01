/* Funciones de carga y liberacion de memoria de los datasets */

#include "carga_datos.h"
#include "Password.h"

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

    /* Si se llego al limite 'max' pero el archivo todavia tiene mas
       lineas, avisar explicitamente en vez de truncar en silencio. */
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
