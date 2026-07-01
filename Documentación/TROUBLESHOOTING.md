# Solución de problemas

## Windows bloquea la ejecución del programa (Smart App Control)

Algunas PC tienen activado "Smart App Control" en Windows, una función
de seguridad que bloquea ejecutables nuevos sin firma digital
reconocida. Si al correr el programa (Ctrl+F5) aparece el error:

> "Una directiva de Control de aplicaciones bloqueó este archivo"

Esto es distinto del aviso de SmartScreen que puede aparecer al usar
el `.exe` distribuido en Releases (ver README) — SmartScreen solo
avisa y se puede saltar con un clic; Smart App Control bloquea
directamente la ejecución.

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

Nota: este post-build event, tal como está configurado en el repo,
solo aplica a la configuración Debug. Si compilas en Release y
Smart App Control te bloquea igual, hay que configurar el mismo
post-build event también para Release (misma ruta de Properties,
cambiando el dropdown de Configuration a "Release" antes de escribir
el Command Line).

Si Smart App Control sigue bloqueando pese a la firma (puede pasar
porque el certificado es autofirmado, sin reputación en la nube de
Microsoft), correr el `.exe` directamente con doble clic desde el
explorador de archivos suele evitar el bloqueo, en vez de correrlo
desde Visual Studio.

Nota: el certificado generado en el paso 2 es local a cada máquina,
no se sube al repositorio. Cada persona del equipo que tenga Smart
App Control activado debe generar el suyo siguiendo estos mismos
pasos.

## Error de enlazado `unresolved external symbol htons`

El proyecto necesita `ws2_32.lib` (librería de sockets de Windows)
para el bind explícito a `127.0.0.1`. Debería estar ya configurado
en el `.vcxproj`, pero si ven este error: Project Properties →
Linker → Input → Additional Dependencies → agregar `ws2_32.lib`.

## El `.exe` no encuentra `libmicrohttpd-dll.dll` al distribuirlo

Si arman un paquete portable del `.exe` compilado para dárselo a
alguien más (ver README, sección "Uso rápido"), tiene que ir
acompañado de `libmicrohttpd-dll.dll`, que se encuentra en
`vcpkg_installed\x64-windows\bin\` dentro de la carpeta del
proyecto. Sin esa DLL al lado del `.exe`, Windows va a mostrar un
error de "no se encontró tal.dll" al intentar ejecutarlo. Ojo con
usar la versión correcta: la que tiene sufijo `_d` (`..._d.dll`)
corresponde a builds Debug; la que no tiene sufijo corresponde a
Release.
