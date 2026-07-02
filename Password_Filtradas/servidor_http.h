#ifndef SERVIDOR_HTTP_H
#define SERVIDOR_HTTP_H

#include <stddef.h>
#include <microhttpd.h>

/* Prototipos de funciones del servidor HTTP. */
enum MHD_Result manejar_peticion(
    void* cls,
    struct MHD_Connection* conexion,
    const char* url,
    const char* metodo,
    const char* version,
    const char* datos,
    size_t* tam_datos,
    void** ptr);

#endif
