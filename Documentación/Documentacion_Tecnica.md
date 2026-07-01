# Documento de Diseño de Software (SDD)
## SDK de Validación de Contraseñas Filtradas

**Basado en:** IEEE 1016-2009 (Software Design Descriptions), adaptado al
alcance de un proyecto universitario de un solo módulo ejecutable.
**Materia:** Algoritmos — ISW131
**Versión del documento:** 1.0

---

## 1. Introducción

### 1.1 Propósito

Este documento describe el diseño técnico del SDK de Validación de
Contraseñas Filtradas: su arquitectura, las estructuras de datos que
usa, los algoritmos implementados con su análisis de complejidad, la
interfaz HTTP que expone, y las decisiones de diseño tomadas —
incluyendo alternativas consideradas y por qué se descartaron.

### 1.2 Alcance

El sistema es un ejecutable en C (estándar C11) que:

- Carga dos datasets de texto plano en memoria (contraseñas
  filtradas y palabras comunes de diccionario).
- Expone un servidor HTTP local (`127.0.0.1:8080`, no accesible
  desde la red) mediante libmicrohttpd.
- Sirve una interfaz web (HTML/CSS/JS) que consume ese servidor.
- Permite verificar contraseñas individualmente o en lote, y
  reemplazar el dataset de filtradas en tiempo de ejecución.

Fuera de alcance: persistencia en base de datos, autenticación de
usuarios, comunicación fuera de la máquina local, cifrado de
contraseñas en tránsito (el servidor es HTTP, no HTTPS, por diseño:
es una herramienta local de un solo usuario).

### 1.3 Definiciones y acrónimos

| Término | Significado |
|---|---|
| SDK | Software Development Kit — en este contexto, el conjunto de lógica de verificación expuesta vía API HTTP |
| MHD | libmicrohttpd, la librería C que implementa el servidor HTTP |
| Dataset filtradas | Lista de contraseñas conocidas por haber aparecido en brechas de seguridad reales |
| O(n), O(log n) | Notación de complejidad asintótica (peor caso, salvo que se indique lo contrario) |

### 1.4 Referencias

- SecLists — 100k-most-used-passwords-NCSC.txt (dataset de filtradas)
- SecLists — Spanish_Pwdb_common-password-list-top-150.txt (palabras comunes)
- libmicrohttpd, documentación oficial de la API (`MHD_start_daemon`, `MHD_AccessHandlerCallback`)
- IEEE 1016-2009, Standard for Information Technology — Systems Design — Software Design Descriptions

### 1.5 Resumen del documento

La Sección 2 identifica a los interesados y sus preocupaciones de
diseño. Las secciones 3 a 9 presentan el diseño desde distintas
vistas (contexto, composición, información, algoritmos, interfaz,
interacción, recursos). La Sección 10 documenta el racional detrás
de las decisiones de diseño más relevantes. La Sección 11 resume la
verificación realizada. La Sección 12 concluye.

---

## 2. Interesados y preocupaciones de diseño

| Interesado | Preocupación principal |
|---|---|
| Estudiante/equipo de desarrollo | Que el sistema demuestre correctamente los tres algoritmos exigidos por la materia (Insertion Sort, Búsqueda Binaria, Búsqueda Lineal) |
| Profesor evaluador | Correctitud algorítmica, justificación de las decisiones de diseño, complejidad computacional documentada |
| Usuario final (quien corre la demo) | Que la interfaz responda rápido y sin errores al verificar contraseñas propias |

---

## 3. Vista de Contexto

El sistema es un proceso único, autocontenido, sin dependencias
externas en tiempo de ejecución más allá de libmicrohttpd y la
librería estándar de C. No se comunica con ningún servicio externo.

```
                    ┌─────────────────────────┐
   Navegador  ───▶  │   Servidor HTTP (main.c) │
   (localhost)      │   127.0.0.1:8080          │
                    └───────────┬─────────────┘
                                │
                   ┌────────────┴────────────┐
                   │  Núcleo de algoritmos     │
                   │  (insertion_sort.c,       │
                   │   busqueda_binaria.c,     │
                   │   busqueda_lineal.c,      │
                   │   fortaleza.c)            │
                   └────────────┬────────────┘
                                │
                   ┌────────────┴────────────┐
                   │  Datasets en memoria      │
                   │  (arreglos char**, cargados│
                   │   desde .txt al arrancar) │
                   └──────────────────────────┘
```

No hay actores externos más allá del navegador local que consume la
interfaz. El sistema no expone puertos a la red (bind explícito a
loopback) ni depende de servicios de terceros.

---

## 4. Vista de Composición

Descomposición en módulos y responsabilidad de cada uno:

| Módulo | Responsabilidad |
|---|---|
| `Password.h` | Constantes compartidas (límites de tamaño), prototipos de todas las funciones públicas entre módulos |
| `insertion_sort.c` | Ordenamiento incremental del dataset de filtradas mientras se carga |
| `busqueda_binaria.c` | Búsqueda de coincidencia exacta en el dataset ordenado |
| `busqueda_lineal.c` | Búsqueda de subcadena (palabra común) dentro de una contraseña |
| `fortaleza.c` | Cálculo de puntaje de fortaleza basado en composición de caracteres |
| `main.c` | Carga de archivos, servidor HTTP, enrutamiento, serialización JSON, gestión de memoria y concurrencia |
| `web/index.html` | Estructura de la interfaz (4 vistas: Verificar, Archivo, Dataset, Estado) |
| `web/style.css` | Presentación visual |
| `web/app.js` | Lógica de cliente: llamadas `fetch` a los endpoints, renderizado de resultados |

`main.c` concentra la orquestación porque es donde vive el estado
global compartido (los dos datasets en memoria) y el ciclo de vida
del servidor; los tres algoritmos quedan aislados en módulos propios
sin dependencias entre sí, lo que permite probarlos independientemente.

---

## 5. Vista de Información

### 5.1 Estructuras de datos principales

**Datasets en memoria:** ambos datasets (`g_filtradas`, `g_palabras`)
se representan como `char**` — un arreglo de punteros a cadenas
reservadas individualmente con `malloc`. Se eligió esta
representación (en vez de, por ejemplo, un único buffer contiguo con
offsets) porque **Insertion Sort necesita reordenar punteros, no
copiar el contenido de las cadenas** — intercambiar un `char*` es
O(1); mover el contenido completo de una cadena en cada inserción
sería O(longitud de la cadena) adicional, sin necesidad.

**Estado por conexión HTTP (`EstadoPeticion`):**

```c
typedef struct {
    char*   buffer;
    size_t  tam_usado;
    size_t  tam_reservado;
    int     respuesta_enviada;
} EstadoPeticion;
```

Se introdujo para resolver un problema real de diseño: libmicrohttpd
invoca el callback de una petición varias veces mientras llega el
cuerpo (`body`) de un `POST`, sin garantizar que todo llegue en una
sola llamada. Este struct acumula los bytes recibidos en un buffer
que crece dinámicamente (`realloc`, duplicando capacidad), y solo
procesa el cuerpo completo en la llamada final. El campo
`respuesta_enviada` evita encolar más de una respuesta HTTP por
conexión (ver Sección 10.1 para el racional completo).

### 5.2 Formato de intercambio (JSON)

Las respuestas usan JSON armado manualmente con `snprintf` (sin
librería externa de serialización, dado el alcance del proyecto).
Esquema de `/verificar`:

```json
{
  "segura": bool,
  "filtrada": bool,
  "palabra_comun": bool,
  "palabra": string,
  "puntaje": int (0-6),
  "nivel": "Debil" | "Media" | "Fuerte",
  "contrasena": string (con caracteres especiales escapados)
}
```

`/verificar-archivo` devuelve un arreglo de objetos con el mismo
esquema. `/estado` devuelve `{"filtradas": int, "palabras": int}`.

---

## 6. Vista de Algoritmos

Esta es la vista central para los fines de la materia: los tres
algoritmos exigidos, dónde se usan, por qué, y su complejidad.

### 6.1 Insertion Sort — ordenamiento del dataset de filtradas

**Dónde:** `insertion_sort.c`, invocado una vez por cada línea nueva
cargada desde `contrasenas_filtradas.txt` (`cargar_archivo()` en
`main.c`).

**Por qué esta ubicación:** el dataset se lee línea por línea con
`fgets` (no se puede cargar todo de una vez y ordenar al final sin
perder la propiedad de "inserción incremental" que pide la materia).
Cada línea nueva se inserta en su posición correcta dentro del
arreglo ya ordenado, desplazando los elementos mayores una posición
a la derecha — la mecánica real del algoritmo, no una simulación.

**Complejidad:**
- Una sola inserción: O(n) en el peor caso (la nueva clave es menor
  que todas las anteriores, se desplazan las n-1 existentes).
- Cargar el dataset completo de n líneas: O(n²) — se invoca una vez
  por línea, cada una potencialmente O(n).
- Espacial: O(1) adicional por inserción (in-place); el arreglo de
  punteros ya reservado no crece, solo se reordena.

**Costo real medido:** con el dataset completo (99 839 entradas), la
carga tarda entre 5 y 10 segundos en hardware de escritorio estándar
— consistente con el orden de magnitud esperado para O(n²) en ese
tamaño de entrada (del orden de 10¹⁰ operaciones elementales en el
peor caso, muchas menos en la práctica porque el dataset no llega
ordenado en sentido inverso).

**Por qué no Bubble Sort o Selection Sort:** el enunciado del
proyecto no genera un segundo conjunto de datos que requiera
ordenarse — solo hay un dataset que necesita orden estricto
(precondición de la búsqueda binaria), y ese ya se resuelve con
Insertion Sort de forma incremental mientras se carga. Agregar
Bubble o Selection Sort sin un caso de uso real sería forzar
algoritmos donde no correspondería.

### 6.2 Búsqueda Binaria — verificación de coincidencia exacta

**Dónde:** `busqueda_binaria.c`, invocada en cada verificación
(`/verificar` y por cada línea de `/verificar-archivo`) contra el
dataset ordenado de filtradas.

**Por qué esta ubicación:** es la operación más frecuente del
sistema (se ejecuta en cada request), y el dataset ya está ordenado
por el paso anterior — condición necesaria para que binaria sea
aplicable.

**Complejidad:**
- Temporal: O(log n) en el peor caso. Con 99 839 entradas, como
  máximo ⌈log₂(99839)⌉ = 17 comparaciones.
- Espacial: O(1) — implementación iterativa, sin recursión, sin
  memoria adicional proporcional a n.

**Contrato de retorno (documentado explícitamente, ver 10.2):**
índice ≥ 0 si encuentra coincidencia, `-1` si no encuentra, `-2` si
hay un error de verificación (parámetros inválidos o dato corrupto
en el arreglo) — distinción necesaria para no reportar una
contraseña como "segura" cuando en realidad no se pudo verificar.

### 6.3 Búsqueda Lineal — detección de palabras comunes

**Dónde:** `busqueda_lineal.c`, invocada después de la búsqueda
binaria (solo si la contraseña no está en el dataset de filtradas)
contra la lista de 150 palabras comunes de diccionario.

**Por qué lineal y no binaria:** es una búsqueda de subcadena
(¿la contraseña *contiene* alguna palabra común, en cualquier
posición?), no de coincidencia exacta. La búsqueda binaria exige
comparar la clave completa contra el punto medio del arreglo
ordenado; no hay forma de aplicar ese descarte por mitades cuando la
pregunta es "¿aparece como subcadena en cualquier lugar?" —
binaria no es aplicable a este problema por definición, independientemente de si la lista de palabras está ordenada o no.

**Complejidad:**
- Temporal: O(m · k) en el peor caso, donde m es la cantidad de
  palabras comunes (150, constante y chica) y k es la longitud de la
  contraseña a revisar (acotada a 255 caracteres). En la práctica,
  O(1) amortizado dado que ambos factores son constantes pequeños.
- Espacial: O(k) — copias locales en minúsculas de la contraseña y
  de cada palabra comparada, acotadas por `MAX_PASSWORD_LEN`.

---

## 7. Vista de Interfaz

### 7.1 Contrato de la API HTTP

| Método y ruta | Body | Respuesta | Códigos de estado |
|---|---|---|---|
| `GET /` | — | `web/index.html` | 200, 404 (si falta el archivo) |
| `GET /style.css` | — | `web/style.css` | 200, 404 |
| `GET /app.js` | — | `web/app.js` | 200, 404 |
| `GET /estado` | — | `{"filtradas":N,"palabras":N}` | 200 |
| `POST /verificar` | texto plano (contraseña) | JSON de resultado (ver 5.2) | 200, 400 (vacío), 413 (>10MB), 500 |
| `POST /verificar-archivo` | `.txt`, una contraseña por línea | arreglo JSON de resultados | 200, 400, 413, 500 |
| `POST /cargar-dataset` | `.txt`, una contraseña filtrada por línea | `{"ok":bool,"total":N,"mensaje":string}` | 200, 400, 413, 500 |

### 7.2 Decisiones de la interfaz HTTP

- **CORS abierto (`Access-Control-Allow-Origin: *`)** en los
  endpoints de solo lectura (`/estado`, `/verificar`,
  `/verificar-archivo`), pero **deliberadamente ausente** en
  `/cargar-dataset` — ese endpoint muta estado del servidor; con
  CORS abierto, cualquier página web que el usuario visitara podría
  dispararlo sin autorización desde JavaScript (ataque tipo
  "localhost CSRF"). Ver 10.3.
- **Bind explícito a `127.0.0.1`** vía `MHD_OPTION_SOCK_ADDR`, no a
  todas las interfaces — el servidor es una herramienta local de un
  solo usuario, no un servicio de red.

---

## 8. Vista de Interacción

Secuencia de una petición `POST /verificar-archivo` con un archivo
grande (varios MB), el caso que ejercita más lógica del sistema:

```
Navegador                    main.c (manejar_peticion)         Algoritmos
    │                                  │                              │
    │──POST /verificar-archivo───────▶│                              │
    │        (*ptr == NULL)           │ crea EstadoPeticion          │
    │◀─────────── 100 Continue ───────│ (*ptr = estado)              │
    │                                  │                              │
    │──chunk 1 del body───────────────▶│ acumular_datos(chunk 1)     │
    │                                  │ (realloc si hace falta)      │
    │──chunk 2 del body───────────────▶│ acumular_datos(chunk 2)     │
    │            ...                  │            ...               │
    │──chunk N (último)───────────────▶│ acumular_datos(chunk N)     │
    │                                  │                              │
    │──llamada final (tam=0)──────────▶│ estado->tam_usado > 0 →     │
    │                                  │ procesar buffer completo:   │
    │                                  │   por cada línea:            │
    │                                  │──────────────────────────▶ │ busqueda_binaria()
    │                                  │◀────────────────────────── │ (si no encontrada)
    │                                  │──────────────────────────▶ │ busqueda_lineal()
    │                                  │◀────────────────────────── │ calcular_fortaleza()
    │                                  │ arma JSON (buffer dinámico) │
    │                                  │ estado->respuesta_enviada=1 │
    │◀──────── 200 + JSON completo ───│                              │
```

Este diagrama documenta explícitamente el punto que motivó el
rediseño del manejo de peticiones (Sección 10.1): antes, el sistema
asumía que el body completo llegaba en la "llamada 1", procesaba y
respondía ahí mismo, y luego fallaba al recibir la llamada final del
protocolo de MHD (intentando encolar una segunda respuesta sobre la
misma conexión).

---

## 9. Vista de Recursos

### 9.1 Gestión de memoria

- **Datasets globales** (`g_filtradas`, `g_palabras`): reservados
  una vez al arrancar (`malloc` de `MAX_PASSWORDS`/`MAX_PALABRAS`
  punteros), cada entrada individual reservada por `cargar_archivo`.
  Liberados explícitamente al cerrar el servidor (`Ctrl+C`, cierre
  de consola, o fin normal de `main`), con `liberar_memoria()`
  idempotente.
- **Estado por conexión** (`EstadoPeticion`): reservado en la
  primera llamada del callback de MHD, liberado automáticamente por
  `solicitud_completada()` (registrado vía
  `MHD_OPTION_NOTIFY_COMPLETED`) sin importar si la conexión terminó
  bien o con error — evita fugas por conexión.
- **Buffer de resultados de `/verificar-archivo`**: dinámico
  (arranca en 64KB, `realloc` duplicando capacidad), en vez de un
  tamaño fijo que truncara resultados silenciosamente en datasets
  grandes (ver 10.4).

### 9.2 Concurrencia

El servidor corre con `MHD_USE_INTERNAL_POLLING_THREAD` (un único
hilo reactor interno de MHD, sin verdadero paralelismo entre
conexiones). Aun así, el acceso a `g_filtradas`/`g_palabras` está
protegido con una `CRITICAL_SECTION` (`g_lock`) alrededor de toda
lectura y escritura — no porque el modo actual lo requiera
estrictamente, sino para que el sistema sea seguro también si en el
futuro se cambia a un modelo de verdadera concurrencia
(`MHD_USE_THREAD_PER_CONNECTION`), donde una petición podría estar
leyendo el dataset mientras otra lo reemplaza vía
`/cargar-dataset` — sin el mutex, eso sería una condición de carrera
con acceso a memoria liberada.

---

## 10. Racional de diseño

Esta sección documenta decisiones de diseño no triviales, las
alternativas consideradas, y por qué se eligió lo que se eligió.

### 10.1 Acumulación de body por conexión (vs. procesar en la primera llamada)

**Problema:** la implementación inicial asumía que el cuerpo de un
`POST` llegaba completo en una sola invocación del callback de MHD.
Esto funcionaba para contraseñas individuales (pocos bytes) pero
fallaba de dos formas con payloads grandes: truncaba archivos a solo
el primer fragmento recibido, y generaba una segunda llamada a
`MHD_queue_response()` sobre la misma conexión en la invocación
final del protocolo de MHD — comportamiento indefinido que producía
`Empty reply from server` del lado del cliente.

**Alternativas consideradas:**
1. Aumentar el tamaño del buffer de lectura de MHD para que "casi
   siempre" quepa todo en un chunk — descartado: no elimina el
   problema, solo lo hace menos frecuente; sigue siendo incorrecto
   para archivos de varios MB.
2. Acumular todo el body en un buffer por conexión y procesar solo
   en la llamada final (`*tam_datos == 0`) — **elegida**: es el
   patrón correcto según el contrato de la API de MHD, y generaliza
   a cualquier tamaño de payload dentro del límite de 10MB.

### 10.2 Separar "no encontrado" de "error" en las funciones de búsqueda

**Problema:** `busqueda_binaria` usaba `-1` tanto para "la
contraseña no está filtrada" como para "hubo un error interno"
(dataset corrupto, parámetros inválidos). Un llamador que solo
revisa `if (resultado >= 0)` no puede distinguir ambos casos —
riesgo de reportar una contraseña como segura cuando en realidad no
se pudo verificar (fail-open en vez de fail-closed).

**Decisión:** `-1` para "no encontrado" (resultado válido), `-2`
para "error" (resultado no confiable). El llamador (`verificar_una`
en `main.c`) trata `-2` como fallo de verificación explícito
(responde 500), nunca como "no filtrada".

### 10.3 CORS restringido en el endpoint mutante

**Problema:** CORS abierto (`Access-Control-Allow-Origin: *`) en
`/cargar-dataset` permitiría que cualquier página web visitada en el
mismo navegador disparara el reemplazo del dataset sin que el
usuario lo pidiera, mediante una petición `fetch` de JavaScript.

**Decisión:** ese único endpoint no incluye el header CORS. Los
endpoints de solo lectura sí lo mantienen, porque no hay riesgo de
efectos secundarios no solicitados.

### 10.4 Buffer de resultados dinámico (vs. tamaño fijo)

**Problema:** el buffer de salida de `/verificar-archivo` tenía un
tamaño fijo (64KB). Con datasets grandes, el bucle que arma el
arreglo JSON llegaba al límite y cortaba el arreglo silenciosamente
— el JSON resultante seguía siendo sintácticamente válido, pero
incompleto, sin ningún error visible para el cliente. Se detectó
empíricamente probando con un archivo de 5000 líneas: solo 550
resultados llegaban al cliente.

**Decisión:** buffer que crece con `realloc` (duplicando capacidad)
cada vez que no alcanza el espacio restante, acotado únicamente por
el límite de 10MB ya existente para el tamaño de la petición
completa.

---

## 11. Verificación y pruebas

Resumen de las pruebas realizadas sobre la implementación final
(detalle completo de comandos en el README):

| Prueba | Resultado |
|---|---|
| `GET /estado` refleja el dataset cargado | Coincide con lo cargado al arrancar (99 839 / 150) |
| `POST /verificar` con contraseña filtrada | `filtrada:true`, respuesta inmediata |
| `POST /verificar` con contraseña no filtrada | `segura:true`, puntaje 6/6 |
| `POST /verificar-archivo` con 5000 líneas | 4999/5000 procesadas (la diferencia es una línea vacía descartada por diseño) |
| `POST /verificar-archivo` con el dataset completo (99 839 líneas) | 99839/99839 procesadas, sin truncamiento |
| `POST /cargar-dataset` reemplaza el dataset en memoria | Confirmado cruzando `/estado` antes/después y una verificación posterior contra el dataset nuevo |
| Interfaz visual, las 4 vistas | Verificadas manualmente en navegador |

---

## 12. Conclusiones

El sistema cumple el requisito académico central — demostrar
Insertion Sort, Búsqueda Binaria y Búsqueda Lineal aplicados a un
problema real donde cada uno es la elección correcta, no forzada.
Las decisiones de diseño documentadas en la Sección 10 surgieron de
problemas reales encontrados durante el desarrollo y las pruebas
(no hipotéticos), y quedaron resueltas y verificadas empíricamente
según el detalle de la Sección 11.

Limitaciones conocidas, reconocidas explícitamente: Insertion Sort
es O(n²) por diseño y no escalaría a datasets varios órdenes de
magnitud más grandes que el actual; el modelo de fortaleza de
contraseñas es una heurística simple basada en reglas de
composición, no un estimador de entropía real.
