# Proyecto Universitario Validación de Contraseñas Filtradas

SDK en C que verifica si una contraseña fue filtrada en brechas de
seguridad conocidas y analiza su fortaleza, con una interfaz web
servida por un servidor HTTP local (libmicrohttpd). Corre 100% en
tu máquina, no depende de ningún servicio externo.

📄 Documentación completa: [`Documentacion_Tecnica.md`](Documentación/Documentacion_Tecnica.md)
(arquitectura, algoritmos, complejidad) · [`TROUBLESHOOTING.md`](Documentación/TROUBLESHOOTING.md)
(problemas comunes al compilar o ejecutar)

## Uso rápido (sin compilar nada)

1. Ve a la sección [Releases](../../releases) y descarga el `.zip`
   más reciente (`PasswordFiltradas-vX.X-win64.zip`).
2. Descomprime en cualquier carpeta y ejecuta `Password_Filtradas.exe`.
   > Si Windows muestra un aviso de SmartScreen ("Windows protegió
   > su PC"): clic en "Más información" → "Ejecutar de todas
   > formas". Es normal en software independiente sin firma de
   > editor verificado.
3. Abre `http://127.0.0.1:8080` en tu navegador.

No necesitas internet para que funcione — el servidor es 100%
local. Presiona Enter en la consola para detenerlo.

## Desarrollo

### Requisitos

- Visual Studio 2026, workload "Desarrollo para escritorio con C++"

### Configuración del dataset

Descargar y colocar en `Password_Filtradas/Password_Filtradas/`:

- [contrasenas_filtradas.txt](https://github.com/danielmiessler/SecLists/blob/master/Passwords/Common-Credentials/100k-most-used-passwords-NCSC.txt)
- [palabras_comunes.txt](https://github.com/danielmiessler/SecLists/blob/master/Passwords/Language-Specific/Spanish_Pwdb_common-password-list-top-150.txt)

El repo ya incluye `contrasenas_filtradas_10k.txt` como muestra
chica para pruebas rápidas, sin necesidad de descargar el dataset
completo.

### Compilar y correr

Abrir `Password_Filtradas.slnx` en Visual Studio 2026,
`Ctrl+Shift+B` para compilar, `Ctrl+F5` para correr. Con el servidor
arriba, abrir `http://127.0.0.1:8080` en el navegador — ahí carga
la interfaz completa (`web/index.html`, `web/style.css`,
`web/app.js`, servidos directo desde disco).

¿Algo falla al compilar o ejecutar? Ver
[`TROUBLESHOOTING.md`](Documentación/TROUBLESHOOTING.md)

## Estructura del proyecto

```
Password_Filtradas/
└── Password_Filtradas/
    ├── Password.h
    ├── main.c
    ├── insertion_sort.c
    ├── busqueda_binaria.c
    ├── busqueda_lineal.c
    ├── fortaleza.c
    ├── contrasenas_filtradas.txt      (no está en git)
    ├── contrasenas_filtradas_10k.txt  (sí está en git, dataset de prueba)
    ├── palabras_comunes.txt           (no está en git)
    └── web/
        ├── index.html
        ├── style.css
        └── app.js
```

Detalle de algoritmos usados (Insertion Sort, Búsqueda Binaria,
Búsqueda Lineal), complejidad, y el contrato completo de la API
HTTP: ver [`Documentacion_Tecnica.md`](Documentación/Documentacion_Tecnica.md).
