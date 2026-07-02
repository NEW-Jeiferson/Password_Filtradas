#ifndef ESTADO_PETICION_H
#define ESTADO_PETICION_H

#include <stddef.h>
#include <microhttpd.h>

/* Estructura para mantener el estado de una petición HTTP. */
typedef struct {
    char* buffer;
    size_t  tam_usado;
    size_t  tam_reservado;
    int     respuesta_enviada;
} EstadoPeticion;

EstadoPeticion* crear_estado_peticion(void);
void destruir_estado_peticion(EstadoPeticion* estado);

/* Función para acumular datos recibidos en la petición HTTP. */
int acumular_datos(EstadoPeticion* estado, const char* datos,
    size_t tam, size_t limite);

/* Función de callback para manejar la finalización de la petición HTTP. */
void solicitud_completada(void* cls,
    struct MHD_Connection* conexion,
    void** con_cls,
    enum MHD_RequestTerminationCode motivo);

#endif