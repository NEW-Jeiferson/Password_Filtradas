# Proyecto Universitario Validación de Contraseñas Filtradas

SDK en C que verifica si una contraseña fue filtrada en brechas de
seguridad conocidas y analiza su fortaleza, con una interfaz web
servida por un servidor HTTP local (libmicrohttpd).

## Requisitos

- Visual Studio 2026
- Workload: Desarrollo para escritorio con C++
- Conexión a internet **opcional**: la interfaz carga las tipografías
  desde Google Fonts; si no hay internet, cae automáticamente a una
  fuente monoespaciada del sistema, no rompe nada.

## Configuración del dataset

1. Descargar contrasenas_filtradas.txt desde:
   https://github.com/danielmiessler/SecLists/blob/master/Passwords/Common-Credentials/100k-most-used-passwords-NCSC.txt
2. Descargar palabras_comunes.txt desde:
   https://github.com/danielmiessler/SecLists/blob/master/Passwords/Language-Specific/Spanish_Pwdb_common-password-list-top-150.txt
3. Colocar ambos archivos en:
   Password_Filtradas/Password_Filtradas/

Para pruebas rápidas sin descargar el dataset completo, el repo ya
incluye `contrasenas_filtradas_10k.txt` (10 mil entradas, versionado
a propósito en git) como muestra chica.

## Compilación

Abrir Password_Filtradas.slnx con Visual Studio 2026 y compilar con
Ctrl+Shift+B.

El proyecto enlaza `ws2_32.lib` (librería de sockets de Windows,
necesaria porque el servidor se bindea explícitamente a
`127.0.0.1`). Ya está configurado en el `.vcxproj`; si por algún
motivo ven un error de enlazado tipo `unresolved external symbol
htons`, revisen Project Properties → Linker → Input → Additional
Dependencies y agreguen `ws2_32.lib` a mano.

## Ejecutar el servidor

Ctrl+F5 desde Visual Studio, o doble clic al `.exe` compilado desde
el explorador de archivos (ver sección de Smart App Control si esto
da problemas). Al arrancar, el servidor:

1. Carga `contrasenas_filtradas.txt` y `palabras_comunes.txt` con
   Insertion Sort incremental.
2. Levanta el servidor HTTP en `http://127.0.0.1:8080` (solo
   accesible desde la misma máquina, no desde la red).

Con el servidor corriendo, abrir en el navegador:

```
http://127.0.0.1:8080/
```

Ahí carga la interfaz visual completa. Presionar Enter en la
consola del servidor (o Ctrl+C) para detenerlo.

## Interfaz visual

La interfaz (`web/index.html`, `web/style.css`, `web/app.js`) tiene
cuatro secciones:

- **Verificar** — revisa una sola contraseña.
- **Archivo** — sube un `.txt` con varias contraseñas (una por
  línea) y las revisa todas.
- **Dataset** — reemplaza el dataset de contraseñas filtradas en
  memoria por uno nuevo (acción destructiva, pide confirmación).
- **Estado** — muestra cuántas entradas hay cargadas ahora mismo.

Estos archivos se sirven directamente desde disco en cada petición
(no se compilan), así que se pueden editar y solo hace falta
refrescar el navegador, sin recompilar el proyecto.

## Endpoints del servidor

```
GET  /                   → interfaz visual (web/index.html)
GET  /style.css           → hoja de estilos
GET  /app.js               → lógica de la interfaz
GET  /estado               → {"filtradas":N,"palabras":N}
POST /verificar            → verifica una contraseña (cuerpo: texto plano)
POST /verificar-archivo    → verifica varias contraseñas (cuerpo: .txt, una por línea)
POST /cargar-dataset       → reemplaza el dataset de filtradas (cuerpo: .txt)
```

Límite de tamaño de petición: 10 MB por request (`/verificar-archivo`
y `/cargar-dataset`).

### Probar con curl (sin la interfaz)

```powershell
curl.exe -X POST http://127.0.0.1:8080/verificar -d "password123"
curl.exe -X POST http://127.0.0.1:8080/verificar-archivo --data-binary "@archivo.txt"
curl.exe http://127.0.0.1:8080/estado
```

## Si Windows bloquea la ejecución del programa (Smart App Control)

Algunas PC tienen activado "Smart App Control" en Windows, una función
de seguridad que bloquea ejecutables nuevos sin firma digital. Si al
correr el programa (Ctrl+F5) aparece el error:

"Una directiva de Control de aplicaciones bloqueó este archivo"

Sigue estos pasos una sola vez en tu máquina:

### 1. Verificar si Smart App Control está activo

Configuración de Windows > Privacidad y seguridad > Seguridad de
Windows > Control de aplicaciones y del explorador > Smart App Control.
Si dice "Activado", continúa con los siguientes pasos.

### 2. Crear un certificado local de desarrollo

Abrir "Developer PowerShell for VS" (no hace falta ser administrador
para este paso) y ejecutar:

```powershell
$cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject "CN=PasswordFiltradasDev" -CertStoreLocation "Cert:\CurrentUser\My" -NotAfter (Get-Date).AddYears(3)
$cert | Export-Certificate -FilePath "$env:USERPROFILE\PasswordFiltradasDev.cer"
```

### 3. Instalar el certificado como confiable

Cerrar esa terminal y abrir "Developer PowerShell for VS" de nuevo,
esta vez con clic derecho > "Ejecutar como administrador". Ejecutar:

```powershell
Import-Certificate -FilePath "$env:USERPROFILE\PasswordFiltradasDev.cer" -CertStoreLocation Cert:\LocalMachine\Root
Import-Certificate -FilePath "$env:USERPROFILE\PasswordFiltradasDev.cer" -CertStoreLocation Cert:\LocalMachine\TrustedPublisher
```

Cada uno de estos dos comandos imprime un Thumbprint (una cadena
larga de letras y números). Copia ese Thumbprint, se usa en el
siguiente paso.

### 4. Configurar el proyecto para firmar el .exe automáticamente

En Visual Studio: clic derecho en el proyecto Password_Filtradas >
Properties > Configuration Properties > Build Events > Post-Build Event.
En el campo "Command Line" escribir, reemplazando TU_THUMBPRINT por
el valor copiado en el paso anterior:

```
signtool sign /sha1 TU_THUMBPRINT /fd SHA256 "$(TargetPath)"
```

Apply > OK, y recompilar con Ctrl+Shift+B. A partir de ahí, cada
build firma el .exe automáticamente y Windows ya no debería bloquearlo.

Si Smart App Control sigue bloqueando pese a la firma (puede pasar
porque el certificado es autofirmado, sin reputación en la nube de
Microsoft), correr el `.exe` directamente con doble clic desde el
explorador de archivos suele evitar el bloqueo, en vez de correrlo
desde Visual Studio.

Nota: el certificado generado en el paso 2 es local a cada máquina,
no se sube al repositorio. Cada persona del equipo que tenga Smart
App Control activado debe generar el suyo siguiendo estos mismos
pasos.

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

## Algoritmos utilizados

- **Insertion Sort** — ordena el dataset de contraseñas filtradas de
  forma incremental, insertando una entrada nueva por cada línea
  cargada del archivo.
- **Búsqueda Binaria** — verifica en O(log n) si una contraseña
  exacta está en el dataset ordenado.
- **Búsqueda Lineal** — detecta si la contraseña contiene una
  palabra de diccionario común, en cualquier posición (búsqueda de
  subcadena, no reemplazable por binaria).
