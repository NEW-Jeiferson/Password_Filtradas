/* Módulo de análisis de fortaleza de contraseñas, evalúa longitud, mayúsculas, minúsculas,
números y símbolos para generar un puntaje de seguridad */

#include "Password.h"

int calcular_fortaleza(const char* contrasena) {
    int puntaje = 0;
    int longitud;
    int tiene_mayuscula = 0;
    int tiene_minuscula = 0;
    int tiene_numero = 0;
    int tiene_simbolo = 0;
    int i;

    if (contrasena == NULL) {
        fprintf(stderr, "Error: contrasena no puede ser NULL\n");
        return -1;
    }

    longitud = (int)strlen(contrasena);

    if (longitud == 0) {
        fprintf(stderr, "Error: contrasena no puede estar vacia\n");
        return -1;
    }

    for (i = 0; i < longitud; i++) {
        unsigned char c = (unsigned char)contrasena[i];

        if (isupper(c))  tiene_mayuscula = 1;
        if (islower(c))  tiene_minuscula = 1;
        if (isdigit(c))  tiene_numero = 1;
        if (ispunct(c))  tiene_simbolo = 1;
    }

    if (longitud >= 8)  puntaje++;
    if (longitud >= 12) puntaje++;

    /* Puntaje por variedad de caracteres */
    if (tiene_mayuscula) puntaje++;
    if (tiene_minuscula) puntaje++;
    if (tiene_numero)    puntaje++;
    if (tiene_simbolo)   puntaje++;

    return puntaje;
}


void mostrar_fortaleza(int puntaje) {
    if (puntaje < 0) {
        fprintf(stderr, "Error: puntaje invalido\n");
        return;
    }

    printf("Puntaje de fortaleza: %d/6\n", puntaje);

    if (puntaje <= 2) {
        printf("Nivel: DEBIL\n");
    }
    else if (puntaje <= 4) {
        printf("Nivel: MEDIA\n");
    }
    else {
        printf("Nivel: FUERTE\n");
    }
}