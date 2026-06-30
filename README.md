# Proyecto Universitario Validación de Contraseñas Filtradas

## Requisitos
- Visual Studio 2026
- Workload: Desarrollo para escritorio con C++

## Configuración del dataset
1. Descargar contrasenas_filtradas.txt desde:
   https://github.com/danielmiessler/SecLists/blob/master/Passwords/Common-Credentials/100k-most-used-passwords-NCSC.txt
2. Descargar palabras_comunes.txt desde:
   https://github.com/danielmiessler/SecLists/blob/master/Passwords/Language-Specific/Spanish_Pwdb_common-password-list-top-150.txt
3. Colocar ambos archivos en:
   Password_Filtradas/Password_Filtradas/

## Compilación
Abrir Password_Filtradas.slnx con Visual Studio 2026 y compilar con Ctrl+B

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
    ├── contrasenas_filtradas.txt  (no está en git)
    └── palabras_comunes.txt       (no está en git)
```
