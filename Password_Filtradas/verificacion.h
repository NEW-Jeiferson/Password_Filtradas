#ifndef VERIFICACION_H
#define VERIFICACION_H

#include <stddef.h>

/* Verifica una contraseña y genera un mensaje de salida.
 * Retorno: 0 si la contraseña es aceptable, 1 si es débil, 2 si es filtrada,
 * 3 si contiene palabras comunes, -1 si hubo un error. */
int verificar_una(const char* contrasena, char* salida, size_t tam);

#endif

