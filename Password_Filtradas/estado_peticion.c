#include "estado_peticion.h"
#include <stdlib.h>
#include <string.h>


#define TAM_INICIAL_ACUMULADOR  4096

EstadoPeticion* crear_estado_peticion(void) {
    EstadoPeticion* estado = (EstadoPeticion*)calloc(1, sizeof(EstadoPeticion));
    return estado;
}

void destruir_estado_peticion(EstadoPeticion* estado) {
    if (estado == NULL) return;
    free(estado->buffer);
    free(estado);
}

int acumular_datos(EstadoPeticion* estado, const char* datos,
    size_t tam, size_t limite) {

    if (estado->tam_usado + tam > limite) {
        return 0;
    }

    if (estado->tam_usado + tam > estado->tam_reservado) {
        size_t nuevo_tam = (estado->tam_reservado == 0)
            ? TAM_INICIAL_ACUMULADOR : estado->tam_reservado * 2;
        char* nuevo_buffer;

        while (nuevo_tam < estado->tam_usado + tam) {
            nuevo_tam *= 2;
        }

        nuevo_buffer = (char*)realloc(estado->buffer, nuevo_tam);
        if (nuevo_buffer == NULL) {
            return -1;
        }

        estado->buffer = nuevo_buffer;
        estado->tam_reservado = nuevo_tam;
    }

    memcpy(estado->buffer + estado->tam_usado, datos, tam);
    estado->tam_usado += tam;
    return 1;
}


void solicitud_completada(void* cls,
    struct MHD_Connection* conexion,
    void** con_cls,
    enum MHD_RequestTerminationCode motivo) {
    EstadoPeticion* estado;

    (void)cls;
    (void)conexion;
    (void)motivo;

    estado = (EstadoPeticion*)(*con_cls);
    destruir_estado_peticion(estado);
    *con_cls = NULL;
}
