#ifndef ESTADO_GLOBAL_H
#define ESTADO_GLOBAL_H

#include <signal.h>

/* Declaraciones de variables globales compartidas entre los módulos. */
struct MHD_Daemon;

/* Variables globales para las contraseñas filtradas y palabras comunes. */
extern char** g_filtradas;
extern int      g_total_filtradas;
extern char** g_palabras;
extern int      g_total_palabras;

extern struct MHD_Daemon* g_servidor;

extern volatile sig_atomic_t g_seguir_corriendo;

extern CRITICAL_SECTION g_lock;

#endif
