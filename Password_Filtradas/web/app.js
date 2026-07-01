(function () {
  "use strict";

  /* ============================================================
     Utilidades generales
     ============================================================ */

  function $(selector, contexto) {
    return (contexto || document).querySelector(selector);
  }

  function crearElemento(etiqueta, clases, texto) {
    var el = document.createElement(etiqueta);
    if (clases) el.className = clases;
    if (texto !== undefined) el.textContent = texto;
    return el;
  }

  function mostrar(el) { if (el) el.hidden = false; }
  function ocultar(el) { if (el) el.hidden = true; }

  /* ============================================================
     Efecto ripple en botones (.boton) — puramente visual, no
     afecta el submit/click real del formulario: solo agrega y
     limpia un <span> decorativo.
     ============================================================ */

  function agregarRipple(evento) {
    var boton = evento.currentTarget;
    if (boton.disabled) return;

    var rect = boton.getBoundingClientRect();
    var diametro = Math.max(rect.width, rect.height);
    var radio = diametro / 2;

    var span = document.createElement("span");
    span.className = "ripple";
    span.style.width = span.style.height = diametro + "px";
    span.style.left = (evento.clientX - rect.left - radio) + "px";
    span.style.top = (evento.clientY - rect.top - radio) + "px";

    boton.appendChild(span);
    span.addEventListener("animationend", function () {
      span.remove();
    });
  }

  document.querySelectorAll(".boton").forEach(function (boton) {
    boton.addEventListener("click", agregarRipple);
  });

  /* ============================================================
     Navegación entre vistas
     ============================================================ */

  var botonesNav = document.querySelectorAll(".nav-item");
  var vistas = document.querySelectorAll(".vista");

  botonesNav.forEach(function (boton) {
    boton.addEventListener("click", function () {
      var destino = boton.getAttribute("data-vista");

      botonesNav.forEach(function (b) { b.classList.remove("es-activo"); });
      boton.classList.add("es-activo");

      vistas.forEach(function (v) {
        if (v.getAttribute("data-vista") === destino) {
          /* Se quita y se vuelve a agregar la clase para que la
             animacion de entrada (definida en CSS sobre
             .vista.es-activa) se reinicie cada vez que se cambia
             de pestaña, no solo la primera vez. */
          v.classList.remove("es-activa");
          void v.offsetWidth; /* fuerza reflow */
          v.classList.add("es-activa");
        } else {
          v.classList.remove("es-activa");
        }
      });

      if (destino === "estado") {
        actualizarEstado();
      }
    });
  });

  /* ============================================================
     Estado del servidor (barra superior + vista Estado)
     ============================================================ */

  var numeroFiltradas = $("#numero-filtradas");
  var numeroPalabras = $("#numero-palabras");
  var errorEstado = $("#error-estado");

  function actualizarEstado() {
    ocultar(errorEstado);

    fetch("/estado")
      .then(function (res) {
        if (!res.ok) throw new Error("El servidor respondio " + res.status);
        return res.json();
      })
      .then(function (datos) {
        if (numeroFiltradas) numeroFiltradas.textContent = datos.filtradas;
        if (numeroPalabras) numeroPalabras.textContent = datos.palabras;
      })
      .catch(function (err) {
        if (errorEstado) {
          errorEstado.textContent =
            "No se pudo consultar /estado: " + err.message;
          mostrar(errorEstado);
        }
      });
  }

  $("#boton-refrescar-estado").addEventListener("click", actualizarEstado);

  /* ============================================================
     Vista: Verificar una contraseña
     ============================================================ */

  var formVerificar = $("#form-verificar");
  var inputPassword = $("#input-password");
  var botonMostrar = $("#boton-mostrar");
  var lineaEscaneo = $("#linea-escaneo");
  var resultadoVerificar = $("#resultado-verificar");
  var etiquetaNivel = $("#etiqueta-nivel");
  var resultadoTitulo = $("#resultado-titulo");
  var medidorBloques = $("#medidor-bloques");
  var medidorTexto = $("#medidor-texto");
  var listaHallazgos = $("#lista-hallazgos");
  var errorVerificar = $("#error-verificar");

  botonMostrar.addEventListener("click", function () {
    var esTexto = inputPassword.type === "text";
    inputPassword.type = esTexto ? "password" : "text";
    botonMostrar.textContent = esTexto ? "ver" : "ocultar";
  });

  function claseNivel(nivel) {
    if (nivel === "Debil") return "es-debil";
    if (nivel === "Media") return "es-media";
    return "es-fuerte";
  }

  function pintarMedidor(puntaje, nivel) {
    medidorBloques.innerHTML = "";
    var total = 6;
    for (var i = 0; i < total; i++) {
      var bloque = crearElemento("span");
      if (i < puntaje) {
        bloque.classList.add("es-llena");
        if (nivel === "Debil") bloque.classList.add("es-debil");
        if (nivel === "Fuerte") bloque.classList.add("es-fuerte");
      }
      medidorBloques.appendChild(bloque);
    }
    medidorTexto.textContent = puntaje + "/" + total;
  }

  function pintarResultadoIndividual(datos) {
    var nivel = datos.nivel || "Debil";

    etiquetaNivel.textContent = nivel;
    etiquetaNivel.className = "etiqueta-nivel " + claseNivel(nivel);

    if (datos.filtrada) {
      resultadoTitulo.textContent = "Esta contraseña está en un dataset filtrado";
    } else if (datos.palabra_comun) {
      resultadoTitulo.textContent = "Contiene una palabra común de diccionario";
    } else if (datos.segura) {
      resultadoTitulo.textContent = "No se encontró en ningún dataset conocido";
    } else {
      resultadoTitulo.textContent = "No está filtrada, pero es débil";
    }

    pintarMedidor(datos.puntaje || 0, nivel);

    listaHallazgos.innerHTML = "";
    var hallazgos = [];
    if (datos.filtrada) {
      hallazgos.push("Aparece tal cual en el dataset de contraseñas filtradas.");
    }
    if (datos.palabra_comun) {
      hallazgos.push("Contiene la palabra común \"" + datos.palabra + "\".");
    }
    if (!datos.filtrada && !datos.palabra_comun) {
      hallazgos.push(
        datos.segura
          ? "Cumple longitud y variedad de caracteres suficientes."
          : "No está filtrada, pero le falta longitud o variedad de caracteres."
      );
    }
    hallazgos.forEach(function (texto) {
      listaHallazgos.appendChild(crearElemento("li", null, texto));
    });

    /* Reinicia la animacion de entrada del panel de resultado. */
    resultadoVerificar.classList.remove("es-repintado");
    void resultadoVerificar.offsetWidth;
    resultadoVerificar.style.animation = "none";
    void resultadoVerificar.offsetWidth;
    resultadoVerificar.style.animation = "";

    mostrar(resultadoVerificar);
  }

  formVerificar.addEventListener("submit", function (evento) {
    evento.preventDefault();

    var password = inputPassword.value;
    if (!password) return;

    ocultar(errorVerificar);
    ocultar(resultadoVerificar);
    mostrar(lineaEscaneo);

    fetch("/verificar", {
      method: "POST",
      body: password
    })
      .then(function (res) {
        return res.json().then(function (datos) {
          if (!res.ok) {
            throw new Error(datos.error || ("El servidor respondio " + res.status));
          }
          return datos;
        });
      })
      .then(function (datos) {
        pintarResultadoIndividual(datos);
      })
      .catch(function (err) {
        errorVerificar.textContent = "No se pudo verificar: " + err.message;
        mostrar(errorVerificar);
      })
      .finally(function () {
        ocultar(lineaEscaneo);
      });
  });

  /* ============================================================
     Vista: Verificar archivo
     ============================================================ */

  var formArchivo = $("#form-archivo");
  var zonaArrastre = $("#zona-arrastre");
  var inputArchivo = $("#input-archivo");
  var textoArchivo = $("#texto-archivo");
  var botonSubirArchivo = $("#boton-subir-archivo");
  var barraProgresoArchivo = $("#barra-progreso-archivo");
  var skeletonTabla = $("#skeleton-tabla");
  var resumenArchivo = $("#resumen-archivo");
  var tablaContenedor = $("#tabla-resultados-contenedor");
  var cuerpoTabla = $("#cuerpo-tabla-resultados");
  var errorArchivo = $("#error-archivo");

  function configurarZonaArchivo(zona, input, textoEl, boton, textoBase) {
    zona.addEventListener("click", function () { input.click(); });

    zona.addEventListener("dragover", function (e) {
      e.preventDefault();
      zona.classList.add("es-arrastrando");
    });
    zona.addEventListener("dragleave", function () {
      zona.classList.remove("es-arrastrando");
    });
    zona.addEventListener("drop", function (e) {
      e.preventDefault();
      zona.classList.remove("es-arrastrando");
      if (e.dataTransfer.files.length > 0) {
        input.files = e.dataTransfer.files;
        actualizarNombreArchivo();
      }
    });

    input.addEventListener("change", actualizarNombreArchivo);

    function actualizarNombreArchivo() {
      if (input.files.length > 0) {
        textoEl.textContent = input.files[0].name;
        boton.disabled = false;
      } else {
        textoEl.textContent = textoBase;
        boton.disabled = true;
      }
    }
  }

  configurarZonaArchivo(
    zonaArrastre, inputArchivo, textoArchivo, botonSubirArchivo,
    "Arrastra un .txt o haz clic"
  );

  function chip(esPositivo, textoSi, textoNo) {
    var span = crearElemento("span",
      "chip " + (esPositivo ? "es-si" : "es-no"),
      esPositivo ? textoSi : textoNo);
    return span;
  }

  formArchivo.addEventListener("submit", function (evento) {
    evento.preventDefault();
    if (inputArchivo.files.length === 0) return;

    var archivo = inputArchivo.files[0];

    ocultar(errorArchivo);
    ocultar(resumenArchivo);
    ocultar(tablaContenedor);
    mostrar(barraProgresoArchivo);
    mostrar(skeletonTabla);
    botonSubirArchivo.disabled = true;

    archivo.arrayBuffer()
      .then(function (buffer) {
        return fetch("/verificar-archivo", {
          method: "POST",
          body: buffer
        });
      })
      .then(function (res) {
        return res.json().then(function (datos) {
          if (!res.ok) {
            var mensaje = (datos && datos.error) || ("El servidor respondio " + res.status);
            throw new Error(mensaje);
          }
          return datos;
        });
      })
      .then(function (resultados) {
        pintarResultadosArchivo(resultados);
      })
      .catch(function (err) {
        errorArchivo.textContent = "No se pudo procesar el archivo: " + err.message;
        mostrar(errorArchivo);
      })
      .finally(function () {
        ocultar(barraProgresoArchivo);
        ocultar(skeletonTabla);
        botonSubirArchivo.disabled = false;
      });
  });

  function pintarResultadosArchivo(resultados) {
    var total = resultados.length;
    var filtradas = 0;
    var comunes = 0;
    var fuertes = 0;

    cuerpoTabla.innerHTML = "";

    resultados.forEach(function (r) {
      if (r.error) return;
      if (r.filtrada) filtradas++;
      if (r.palabra_comun) comunes++;
      if (r.nivel === "Fuerte") fuertes++;

      var fila = document.createElement("tr");

      var tdPass = crearElemento("td");
      tdPass.textContent = r.contrasena || "";
      fila.appendChild(tdPass);

      var tdNivel = crearElemento("td");
      var etiqueta = crearElemento("span",
        "etiqueta-nivel " + claseNivel(r.nivel), r.nivel || "—");
      tdNivel.appendChild(etiqueta);
      fila.appendChild(tdNivel);

      var tdFiltrada = crearElemento("td");
      tdFiltrada.appendChild(chip(!!r.filtrada, "Sí", "No"));
      fila.appendChild(tdFiltrada);

      var tdComun = crearElemento("td");
      tdComun.appendChild(chip(!!r.palabra_comun, "Sí", "No"));
      fila.appendChild(tdComun);

      cuerpoTabla.appendChild(fila);
    });

    resumenArchivo.innerHTML = "";
    var resumenes = [
      [total, "procesadas"],
      [filtradas, "filtradas"],
      [comunes, "con palabra común"],
      [fuertes, "fuertes"]
    ];
    resumenes.forEach(function (par) {
      var span = document.createElement("span");
      var fuerte = crearElemento("strong", null, String(par[0]));
      span.appendChild(fuerte);
      span.appendChild(document.createTextNode(" " + par[1]));
      resumenArchivo.appendChild(span);
    });

    mostrar(resumenArchivo);
    mostrar(tablaContenedor);
  }

  /* ============================================================
     Vista: Cargar dataset nuevo
     ============================================================ */

  var formDataset = $("#form-dataset");
  var zonaArrastreDataset = $("#zona-arrastre-dataset");
  var inputDataset = $("#input-dataset");
  var textoDataset = $("#texto-dataset");
  var botonCargarDataset = $("#boton-cargar-dataset");
  var exitoDataset = $("#exito-dataset");
  var errorDataset = $("#error-dataset");

  configurarZonaArchivo(
    zonaArrastreDataset, inputDataset, textoDataset, botonCargarDataset,
    "Arrastra el nuevo dataset .txt o haz clic"
  );

  formDataset.addEventListener("submit", function (evento) {
    evento.preventDefault();
    if (inputDataset.files.length === 0) return;

    var archivo = inputDataset.files[0];
    var confirmado = window.confirm(
      "Esto va a reemplazar por completo el dataset de contraseñas " +
      "filtradas que el servidor tiene en memoria. ¿Continuar?"
    );
    if (!confirmado) return;

    ocultar(exitoDataset);
    ocultar(errorDataset);
    botonCargarDataset.disabled = true;
    botonCargarDataset.textContent = "Cargando…";

    archivo.arrayBuffer()
      .then(function (buffer) {
        return fetch("/cargar-dataset", {
          method: "POST",
          body: buffer
        });
      })
      .then(function (res) {
        return res.json().then(function (datos) {
          if (!res.ok || !datos.ok) {
            throw new Error(datos.mensaje || ("El servidor respondio " + res.status));
          }
          return datos;
        });
      })
      .then(function (datos) {
        exitoDataset.textContent =
          datos.mensaje + " (" + datos.total + " entradas cargadas)";
        mostrar(exitoDataset);
        actualizarEstado();
      })
      .catch(function (err) {
        errorDataset.textContent = "No se pudo cargar el dataset: " + err.message;
        mostrar(errorDataset);
      })
      .finally(function () {
        botonCargarDataset.disabled = false;
        botonCargarDataset.textContent = "Reemplazar dataset";
      });
  });

  /* ============================================================
     Arranque
     ============================================================ */

  actualizarEstado();
})();
