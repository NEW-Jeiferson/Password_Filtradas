#ifndef ESTADO_PETICION_H
#define ESTADO_PETICION_H

#include <stddef.h>
#include <microhttpd.h>

typedef struct {
    char* buffer;
    size_t  tam_usado;
    size_t  tam_reservado;
    int     respuesta_enviada;
} EstadoPeticion;

EstadoPeticion* crear_estado_peticion(void);
void destruir_estado_peticion(EstadoPeticion* estado);


int acumular_datos(EstadoPeticion* estado, const char* datos,
    size_t tam, size_t limite);

void solicitud_completada(void* cls,
    struct MHD_Connection* conexion,
    void** con_cls,
    enum MHD_RequestTerminationCode motivo);

#endif