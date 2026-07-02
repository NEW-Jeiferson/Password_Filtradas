/* Carga y liberacion de los datasets de texto plano (contrasenas
   filtradas y palabras comunes) usados por el resto del sistema. */

#ifndef CARGA_DATOS_H
#define CARGA_DATOS_H

/* Carga un archivo de texto plano en memoria, línea por línea.
 * Parámetros:
 *   ruta: ruta del archivo a cargar.
 *   lista: puntero a un arreglo de cadenas donde se almacenarán las líneas.
 *   max: número máximo de líneas a cargar.
 * Retorno: número de líneas cargadas, o -1 si hubo un error. */
int cargar_archivo(const char* ruta, char** lista, int max);

/* Libera la memoria asignada para un arreglo de cadenas.
 * Parámetros:
 *   lista: arreglo de cadenas a liberar.
 *   total: número de cadenas en el arreglo. */
void liberar_memoria(char** lista, int total);

#endif
