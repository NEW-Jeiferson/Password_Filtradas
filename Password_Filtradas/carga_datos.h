/* Carga y liberacion de los datasets de texto plano (contrasenas
   filtradas y palabras comunes) usados por el resto del sistema. */

#ifndef CARGA_DATOS_H
#define CARGA_DATOS_H

int cargar_archivo(const char* ruta, char** lista, int max);

void liberar_memoria(char** lista, int total);

#endif
