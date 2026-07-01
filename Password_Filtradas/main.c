#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <microhttpd.h>
#include <signal.h>

#include "Password.h"
#include "estado_global.h"
#include "carga_datos.h"
#include "servidor_http.h"
#include "estado_peticion.h"

#define PUERTO 8080

char** g_filtradas = NULL;
int      g_total_filtradas = 0;
char** g_palabras = NULL;
int      g_total_palabras = 0;

struct MHD_Daemon* g_servidor = NULL;

volatile sig_atomic_t g_seguir_corriendo = 1;

CRITICAL_SECTION g_lock;


/* Manejador de la señal SIGINT (Ctrl+C) */
static void manejar_sigint(int senal) {
    (void)senal;
    g_seguir_corriendo = 0;
}


static BOOL WINAPI manejador_consola(DWORD tipo_evento) {
    if (tipo_evento == CTRL_CLOSE_EVENT
        || tipo_evento == CTRL_C_EVENT
        || tipo_evento == CTRL_BREAK_EVENT
        || tipo_evento == CTRL_LOGOFF_EVENT
        || tipo_evento == CTRL_SHUTDOWN_EVENT) {

        if (g_servidor != NULL) {
            MHD_stop_daemon(g_servidor);
            g_servidor = NULL;
        }

        EnterCriticalSection(&g_lock);
        liberar_memoria(g_filtradas, g_total_filtradas);
        liberar_memoria(g_palabras, g_total_palabras);
        free(g_filtradas);
        free(g_palabras);
        g_filtradas = NULL;
        g_palabras = NULL;
        LeaveCriticalSection(&g_lock);

        return TRUE;
    }

    return FALSE;
}


int main(void) {
    struct sockaddr_in direccion_local;
    WSADATA datos_wsa;
    int resultado_wsa;

    SetConsoleOutputCP(CP_UTF8);

    signal(SIGINT, manejar_sigint);

    SetConsoleCtrlHandler(manejador_consola, TRUE);

    InitializeCriticalSection(&g_lock);

    resultado_wsa = WSAStartup(MAKEWORD(2, 2), &datos_wsa);
    if (resultado_wsa != 0) {
        fprintf(stderr,
            "Error: WSAStartup fallo (codigo %d)\n", resultado_wsa);
        DeleteCriticalSection(&g_lock);
        return 1;
    }

    printf("=== SDK Validacion de Contrasenas ===\n");

    g_filtradas = (char**)malloc(MAX_PASSWORDS * sizeof(char*));
    if (g_filtradas == NULL) {
        fprintf(stderr, "Error: no se pudo reservar memoria\n");
        WSACleanup();
        DeleteCriticalSection(&g_lock);
        return 1;
    }

    g_palabras = (char**)malloc(MAX_PALABRAS * sizeof(char*));
    if (g_palabras == NULL) {
        fprintf(stderr, "Error: no se pudo reservar memoria\n");
        free(g_filtradas);
        WSACleanup();
        DeleteCriticalSection(&g_lock);
        return 1;
    }

    g_total_filtradas = cargar_archivo(ARCHIVO_FILTRADAS,
        g_filtradas, MAX_PASSWORDS);
    if (g_total_filtradas < 0) {
        fprintf(stderr, "Error: no se pudo cargar el dataset\n");
        free(g_filtradas);
        free(g_palabras);
        WSACleanup();
        DeleteCriticalSection(&g_lock);
        return 1;
    }

    g_total_palabras = cargar_archivo(ARCHIVO_PALABRAS,
        g_palabras, MAX_PALABRAS);
    if (g_total_palabras < 0) {
        fprintf(stderr, "Error: no se pudo cargar palabras comunes\n");
        liberar_memoria(g_filtradas, g_total_filtradas);
        free(g_filtradas);
        free(g_palabras);
        WSACleanup();
        DeleteCriticalSection(&g_lock);
        return 1;
    }

    /* Bind explicito a loopback (127.0.0.1): este servidor es una
       demo local, no hay razon para que sea alcanzable desde otras
       maquinas de la red. */
    memset(&direccion_local, 0, sizeof(direccion_local));
    direccion_local.sin_family = AF_INET;
    direccion_local.sin_port = htons(PUERTO);
    direccion_local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    g_servidor = MHD_start_daemon(
        MHD_USE_INTERNAL_POLLING_THREAD,
        PUERTO,
        NULL, NULL,
        &manejar_peticion, NULL,
        MHD_OPTION_SOCK_ADDR, &direccion_local,
        MHD_OPTION_NOTIFY_COMPLETED, &solicitud_completada, NULL,
        MHD_OPTION_END);

    if (g_servidor == NULL) {
        fprintf(stderr, "Error: no se pudo iniciar el servidor\n");
        liberar_memoria(g_filtradas, g_total_filtradas);
        liberar_memoria(g_palabras, g_total_palabras);
        free(g_filtradas);
        free(g_palabras);
        WSACleanup();
        DeleteCriticalSection(&g_lock);
        return 1;
    }

    printf("Servidor corriendo en http://127.0.0.1:%d (solo local)\n",
        PUERTO);
    printf("Presiona Enter o Ctrl+C para detener...\n");

    while (g_seguir_corriendo) {
        int tecla = getchar();
        if (tecla == '\n' || tecla == EOF) {
            break;
        }
    }

    /* Detener el servidor */
    if (g_servidor != NULL) {
        MHD_stop_daemon(g_servidor);
        g_servidor = NULL;
    }

    liberar_memoria(g_filtradas, g_total_filtradas);
    liberar_memoria(g_palabras, g_total_palabras);
    free(g_filtradas);
    free(g_palabras);

    WSACleanup();
    DeleteCriticalSection(&g_lock);

    printf("Servidor detenido. Memoria liberada.\n");
    return 0;
}
