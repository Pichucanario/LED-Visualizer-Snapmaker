# LED-Visualizer-Snapmaker
"Control visual para Snapmaker U1 con firmware Extended".
He añadido una descripción de la secuencia de arranque del ESP32 dentro de la sección **"Características adicionales"** en el punto 2. También he dejado claro que es una comprobación visual del correcto funcionamiento del sistema. Aquí tienes el informe completo, listo para publicar.

---

# 🌟 Visualizador LED para Snapmaker U1 – Informe para la Comunidad (BETA)

**Versión actual:** v11.55 – **BETA**  
**Autor:** Israel García Armas (con la asistencia de DeepSeek)  
**Licencia:** MIT (código abierto)

---

## 1. Origen y filosofía del proyecto

Este proyecto nació de una necesidad común en la impresión 3D: saber de un vistazo qué está haciendo la impresora, sin necesidad de mirar la pantalla táctil o la aplicación móvil. La idea era simple: convertir una tira de LEDs RGB en un indicador visual del estado de la máquina, con colores y efectos personalizables.

Lo que empezó como una necesidad personal se ha convertido en un sistema completo, estable y configurable, que cualquier usuario de una Snapmaker U1 con firmware Extended puede montar por menos de 25 €. Se ofrece en fase BETA para que la comunidad lo pruebe, mejore y ayude a pulir los últimos detalles.

---

## 2. ¿Cómo funciona?

El sistema se compone de tres partes:

| Componente | Función |
|------------|---------|
| **ESP32** | Lee el estado de la impresora mediante la API de Moonraker (cada 500 ms) y controla los LEDs. |
| **Tira de LEDs WS2812B (21 píxeles)** | Muestra los efectos de luz según el estado. |
| **Interfaz web** | Permite configurar colores, brillo, efectos y asignar un color a cada herramienta (extrusor). |

### Estados automáticos y sus efectos

| Estado en impresora | Efecto visual en LED | Descripción |
|---------------------|----------------------|-------------|
| `idle` (reposo) | Azul tenue con respiración | La máquina está lista, sin trabajo cargado. |
| Nueva impresión detectada | Pre‑printing: barra de progreso al 0 % durante 3 segundos | Indica que se va a iniciar el calentamiento. |
| `heating` (calentando) | Ámbar con respiración lenta | La cama está subiendo a la temperatura objetivo. |
| `calibrating` (calibraciones) | Efecto wave (onda) verde/azul | Desde que termina el calentamiento hasta que el porcentaje de impresión supera 0 %. |
| `printing` (imprimiendo) | Barra de progreso que se llena de izquierda a derecha, con el color de la herramienta activa | El color de la barra es el que hayas asignado a T0, T1, T2 o T3. |
| `paused` | Parpadeo amarillo (configurable) | La impresión se ha detenido. |
| `finished` (finalizado) | Arcoíris | La impresión ha terminado correctamente. |
| `error` | Parpadeo rojo durante 15 segundos | Error grave o cancelación. |

### Características adicionales

- Detección de cambio de herramienta: al cambiar de T0 a T1, etc., la tira parpadea 500 ms con el color de la nueva herramienta.
- Botón "Identificar herramienta" en la web: fuerza un parpadeo de 1,5 segundos para saber qué color tiene el cabezal activo.
- Persistencia: la configuración (colores, efectos, brillo) se guarda en la memoria SPIFFS del ESP32 y se mantiene tras reiniciar.
- Vista previa en tiempo real: cualquier cambio en los selectores, colores o velocidades se ve al instante en los LEDs, sin necesidad de guardar.
- **Secuencia de arranque (chequeo visual del ESP32):** al encender el sistema, los LEDs realizan una comprobación automática dividida en cuatro etapas:  
  1. **Efecto "serpiente"** – un barrido de luz que va desde los extremos hacia el centro y luego se expande, simulando una prueba de todos los píxeles.  
  2. **Validación de WiFi** – la tira muestra un color azul claro breve mientras el ESP32 se conecta a la red.  
  3. **Comprobación de Moonraker** – destello en amarillo si la comunicación con la impresora es correcta (en caso de fallo, parpadea en rojo y se detiene).  
  4. **Finalización correcta** – seis destellos verdes (o rojos si hay error) para indicar que el sistema está listo.  
  Esta secuencia permite diagnosticar rápidamente problemas de conexión o alimentación sin necesidad de mirar el monitor serie.

---

## 3. Puntos fuertes (PROS)

- Código completamente abierto (Licencia MIT): cualquiera puede modificarlo, mejorarlo o adaptarlo a otras impresoras.
- Personalización total: cada estado puede tener su propio color, tipo de efecto (fijo, respiración, parpadeo, arcoíris, wave) y velocidad.
- Asignación de colores por herramienta: muy útil para saber con qué filamento estás imprimiendo sin mirar la pantalla.
- Respuesta rápida: el polling de 500 ms hace que los cambios en la impresora se reflejen casi al instante.
- Efecto "calibrating": un detalle que visualiza la espera entre el calentamiento y el inicio real de la impresión.
- Transición finished → idle manual: el LED se queda en arcoíris hasta que pulsas "Trabajo completado" en la impresora; entonces vuelve a reposo.
- **Posibilidad de alimentación interna directa desde la impresora:** la fuente de alimentación original de la Snapmaker U1 suministra 24 V estables. Usando un pequeño convertidor reductor (step‑down) de 24 V a 5 V (capaz de al menos 1 A), el ESP32 puede alimentarse desde el interior de la máquina, logrando una instalación mucho más limpia y sin fuentes de alimentación externas adicionales.  
  *Nota de responsabilidad: cualquier modificación eléctrica interna anula la garantía del fabricante y debe ser realizada únicamente por usuarios con los conocimientos técnicos necesarios. El proyecto se comparte bajo licencia MIT y el autor declina toda responsabilidad por daños derivados de estas modificaciones.*

---

## 4. Limitaciones y aspectos a tener en cuenta (CONTRAS)

- Colores claros pueden parecer blancos: los LEDs WS2812B no reproducen bien los tonos pastel, beige, gris claro o dorado suave. Para distinguir herramientas, elige colores saturados (rojo, verde, azul, magenta, cian, amarillo intenso). No es un fallo del código, es una limitación física de los LEDs.
- El porcentaje de impresión no es 100 % exacto: se basa en `print_stats.progress` (Moonraker). El desfase es pequeño (entre 8‑10 % por debajo del real) y se mantiene sincronizado. Es lo más fiable que se puede obtener sin acceder internamente a Klipper.
- Alimentación externa si no se hace la modificación interna: si no te atreves a modificar la impresora, los puertos USB de la Snapmaker U1 no son adecuados para alimentar el ESP32 (no proporcionan corriente suficiente). Deberás usar una fuente de alimentación externa (ver punto 6).
- Solo para Snapmaker U1 con firmware Extended: el código depende de objetos específicos de Moonraker y del firmware de paxx12. No funcionará en una impresora sin ese firmware.
- Estado BETA: aunque funciona correctamente, pueden existir pequeños errores o comportamientos no previstos en algunos escenarios. Se agradecen los reportes.

---

## 5. Mejoras futuras (invitación a la comunidad)

El proyecto está vivo y abierto a contribuciones. Algunas ideas que se podrían explorar:

### 5.1. Desglosar el estado de calibración en múltiples fases

Actualmente, el visualizador muestra un estado genérico de "calibrando" durante toda la secuencia de ajustes previa a la impresión. Sin embargo, la Snapmaker U1 ejecuta una serie de calibraciones muy concretas que podrían representarse con efectos visuales específicos. La comunidad puede ayudar a investigar cómo detectar cada una de estas fases a través de Moonraker o del G‑code, y asignarles un efecto propio.

Estas son las calibraciones más relevantes que realiza la U1 antes de cada impresión (documentadas en sus especificaciones oficiales):

| Calibración | ¿Qué hace la máquina? | Sugerencia de efecto visual |
|-------------|----------------------|-----------------------------|
| **Nivelación automática de la cama (Mesh Bed Leveling)** | Construye un mapa de la superficie de la cama para compensar desniveles y garantizar una primera capa perfecta. | Un barrido de luz que recorre los LEDs de un extremo a otro, simulando el escaneo de la cama. |
| **Calibración del offset de herramientas** | Alinea automáticamente las boquillas (T0, T1, T2, T3) en los ejes X, Y y Z con precisión de 0.04 mm. | Parpadeos rápidos con el color de la herramienta que se está calibrando (ej. verde para T0, luego cian para T1, etc.). |
| **Limpieza automática de la boquilla** | Realiza una rutina de cepillado para eliminar residuos de filamento de la punta antes de imprimir. | Un breve efecto de "parpadeo secuencial" o "destello" corto. |
| **Calibración de compensación de flujo (Pressure Advance)** | Extruye una cantidad de filamento, la mide con un sensor y calcula la compensación óptima para evitar exceso o falta de material. | Efecto "respiración" (intensidad creciente y decreciente) sincronizado con los ciclos de extrusión. |
| **Calibración de compensación de vibraciones (Input Shaping)** | Realiza un barrido de frecuencias para medir la resonancia de la máquina y calcular un filtro que suaviza marcas en la superficie. | Efecto de "onda estacionaria" o patrón de ondas superpuestas. |

Para lograr este desglose, la comunidad puede investigar:
- **Detectar la ejecución de comandos G‑code específicos** como `BED_MESH_CALIBRATE`, `G28`, o comandos propios del firmware Snapmaker.
- **Consultar objetos del sistema Klipper a través de Moonraker** (por ejemplo, `bed_mesh`, `probe`, `input_shaper`) para conocer en cada momento la fase de calibración activa.

Cualquier avance en esta dirección permitiría sustituir el genérico estado "calibrando" por una secuencia mucho más informativa y visualmente atractiva.

### 5.2. Otras mejoras posibles

- Más efectos de luz: añadir nuevos patrones (por ejemplo, "fuego", "relámpago", "gradiente fijo").
- Investigar máquinas con firmware cerrado: desarrollar "puentes" personalizados para obtener información de estado en impresoras cuyo firmware no está pensado para ser abierto.
- Integración con Home Assistant: enviar el estado de la impresora a un sistema de domótica.
- Botón físico externo: para forzar la identificación de herramienta o cambiar entre modos manual/automático.
- Modo noche: atenuación automática del brillo en horarios programados.

Si te animas a mejorar el código, envía un pull request o comparte tus cambios en los foros de Snapmaker. Todas las aportaciones son bienvenidas.

---

## 6. Construcción y costes estimados (muy económico)

| Componente | Precio aproximado (€) | Notas |
|------------|----------------------|-------|
| ESP32 (cualquier placa genérica, p.ej. NodeMCU‑32S) | 10 € – 15 € | Asegúrate de que tenga conector USB‑C o micro‑USB. |
| Tira de LEDs WS2812B (21 LEDs) | 4 € – 6 € | Se puede cortar a medida. |
| Fuente de alimentación USB de 5V / 1A (mínimo) | 5 € – 8 € | Un cargador de móvil viejo sirve perfectamente **si no usas alimentación interna**. |
| Convertidor step‑down 24V → 5V (opcional) | 2 € – 4 € | Solo para la modificación de alimentación interna desde la impresora (ver PROS). |
| Cable USB corto y de calidad | 2 € | Evita cables largos o de mala calidad que causen caídas de tensión. |
| Carcasa impresa en 3D (opcional) | ~1 € (material) | Archivos disponibles en Printables (usuario **DeepMaker**, modelo "Snapmaker LED Visualizer Housing"). |
| **TOTAL ESTIMADO** | **~22 € – 36 €** (según opciones) | |

### Alimentación: dos opciones claras

- **Opción externa (recomendada para principiantes)**: usa un cargador USB de pared de 5V y al menos 1A. No conectes el ESP32 a ningún puerto USB de la impresora, porque no suministran corriente suficiente.
- **Opción interna (solo usuarios avanzados)**: aprovecha los 24V internos de la fuente de la Snapmaker U1 mediante un convertidor step‑down a 5V (1A mínimo). Así obtienes una instalación limpia y sin enchufes adicionales.  
  *El autor no se hace responsable de daños por malas conexiones. Si no tienes experiencia en electrónica, elige la opción externa.*

---

## 7. Instalación paso a paso (resumen)

1. Compila y sube el código (proporcionado en este hilo) a tu ESP32 usando el IDE de Arduino (con las bibliotecas: `WiFiManager`, `ArduinoJson`, `Adafruit_NeoPixel`, `WebServer`, `SPIFFS`).
2. Conecta los LEDs al pin 21 del ESP32 (configurable en el código) y la alimentación (externa o interna).
3. Enciende el ESP32. Creará una red WiFi llamada `Snapmaker-Lights`. Conéctate a ella desde tu móvil o PC y sigue los pasos del portal cautivo para configurar tu red doméstica.
4. Averigua la IP que el ESP32 obtiene en tu red (puedes verla en el monitor serie o en la pantalla de tu router).
5. Accede a la interfaz web escribiendo esa IP en tu navegador.
6. Cambia la IP de la impresora en el código (línea `const char* printerIP = "192.168.1.54";`) por la IP real de tu Snapmaker. Vuelve a compilar y subir.
7. Ajusta colores, brillo, efectos a tu gusto. Todo se guarda automáticamente.

---

## 8. Agradecimientos y créditos

Este proyecto no habría sido posible sin el trabajo de personas que compartieron su conocimiento de forma abierta.

- **paxx12** – Creador del firmware Extended para Snapmaker U1, que implementa Klipper y Moonraker en esta máquina. Sin su trabajo, este visualizador no existiría.  
  Puedes encontrar su firmware y documentación en los foros oficiales de Snapmaker y en GitHub.

- **DeepSeek** – Por la asistencia constante durante todo el desarrollo, ayudando a depurar, optimizar y ampliar las funcionalidades.

- **La comunidad maker** – Por su espíritu de colaboración y por compartir sus ideas y pruebas.

---

## 9. Código fuente y licencia

El código completo de la versión **v11.55 BETA** está disponible en este mismo hilo. También se publicará en un repositorio de GitHub en los próximos días.

**Licencia: MIT**  
Puedes usar, copiar, modificar, fusionar, publicar, distribuir, sublicenciar y vender copias del software sin ninguna restricción, siempre que incluyas el aviso de copyright original en todas las copias o partes sustanciales.

> **Repositorio oficial:** https://github.com/israaarmas/snapmaker-led-visualizer (enlace activo próximamente).

---

## 10. Invitación final a la comunidad

Este proyecto es vuestro. Si lo montas, comparte fotos y vídeos en los foros. Si encuentras un error o se te ocurre una mejora, abre un issue en GitHub o escribe en este hilo. Entre todos podemos hacer que la Snapmaker U1 sea aún más expresiva y fácil de monitorizar.

**¡Disfruta de tus impresiones a todo color!**

---

*Israel García Armas – 2025*  
*Con la inestimable ayuda de DeepSeek*
