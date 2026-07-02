/* Declaraciones, constantes y prototipos */

#ifndef PASSWORD_H
#define PASSWORD_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_PASSWORD_LEN    256
#define MAX_PASSWORDS       150000
#define MAX_PALABRAS        500

#define ARCHIVO_FILTRADAS   "contrasenas_filtradas.txt"
#define ARCHIVO_PALABRAS    "palabras_comunes.txt"


/* insertion_sort.c */
int insertion_sort(char** lista, int total);

/* busqueda_binaria.c
 * Retorno: >=0 indice encontrado | -1 no encontrado | -2 error
 * (NUNCA tratar -2 como "no encontrado": es un fallo de verificacion). */
int busqueda_binaria(char** lista, int total, const char* objetivo);

/* busqueda_lineal.c
 * Retorno: >=0 indice encontrado | -1 no encontrado | -2 error */
int busqueda_lineal(char** palabras, int total, const char* contrasena);

/* fortaleza.c */
int calcular_fortaleza(const char* contrasena);
void mostrar_fortaleza(int puntaje);

#endif