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


int insertion_sort(char** lista, int total);


int busqueda_binaria(char** lista, int total, const char* objetivo);


int busqueda_lineal(char** palabras, int total, const char* contrasena);


int calcular_fortaleza(const char* contrasena);
void mostrar_fortaleza(int puntaje);


int cargar_archivo(const char* ruta, char** lista, int max);
void liberar_memoria(char** lista, int total);


#endif