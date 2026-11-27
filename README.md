# GuitarTabStudio

# GuitarTabCreator

**GuitarTabCreator** es una aplicación de escritorio para Windows que convierte archivos MIDI en tablaturas de guitarra y las reproduce en tiempo real mediante síntesis de audio y efectos digitales.

El proyecto combina tres áreas principales: **teoría musical** (análisis armónico y posicionamiento en el diapasón), **optimización algorítmica** (Viterbi) y **procesamiento digital de señales (DSP)**.

---

## Tabla de Contenidos
1. [Características Principales](#características-principales)
2. [Arquitectura del Proyecto](#arquitectura-del-proyecto)
3. [Pipeline de Datos](#pipeline-de-datos)
4. [El Motor de Audio (DSP)](#el-motor-de-audio-dsp)
5. [Tecnologías Utilizadas](#tecnologías-utilizadas)
6. [Requisitos y Ejecución](#requisitos-y-ejecución)
7. [Decisiones de Diseño](#decisiones-de-diseño)
8. [Estado de los Módulos](#estado-de-los-módulos)
9. [Limitaciones y Trabajo Futuro](#limitaciones-y-trabajo-futuro)
10. [Glosario musical y técnico](docs/glosario.md)

---

## Características Principales

- **Mapeo inteligente:** Usa Viterbi para elegir trastes y cuerdas que reduzcan los movimientos de la mano izquierda.
- **Biblioteca de formas:** Contiene formas abiertas, cejillas y otras posiciones conocidas que sirven de referencia al generar los voicings.
- **Análisis armónico:** Agrupa notas cercanas en el tiempo, identifica acordes con varias cualidades e inversiones y estima la tonalidad mediante perfiles tonales.
- **Dos motores de síntesis:** Permite alternar entre **Karplus-Strong**, basado en una línea de retardo, y **síntesis aditiva**, con siete armónicos y envolventes independientes.
- **Efectos en tiempo real:** Incluye un ecualizador de tres bandas con filtros Biquad, overdrive con saturación suave y chorus basado en un LFO y una línea de retardo.
- **Modelado de pastillas:** Adapta parte del timbre al modo Single Coil o Humbucker.
- **Análisis de patrones:** Detecta arpegios básicos y usa esa información para favorecer posiciones conocidas dentro de la parte melódica.
- **Sincronización visual y exportación:** El desplazamiento sigue la reproducción, permite mover manualmente la posición y exportar la tablatura final a `.txt`.

---

## Arquitectura del Proyecto

El código está dividido en módulos independientes para separar la lógica musical, el procesamiento de audio y la interfaz gráfica:

```text
GuitarTabCreator/
│
├── assets/
│   └── icons/               # Iconos PNG de los controles de transporte
|   └── intro/               # Archivos de la animación de la intro de la app
├── scripts/
│   └── midi2csv.py          # Conversor de MIDI a CSV y metadatos (.meta)
├── songs/                   # Archivos generados (.csv y .meta); puede incluir MIDI de prueba
│
├── tipos.h                  # Estructuras base (Nota, EventoArmonico)
├── tipos_mapper.h           # Configuración y tipos internos del mapeo
├── acordes.h / .cpp         # Agrupación, reconocimiento y tonalidad
├── mapper/
│   ├── mapper.h / .cpp      # Orquestador del contexto y la línea de tiempo
│   ├── candidatos.h         # Generación de posiciones y voicings
│   ├── formas_conocidas.h   # Formas abiertas, cejillas y patrones conocidos
│   ├── motor_melodico.h     # Resolución de la línea melódica
│   ├── motor_armonico.h     # Resolución de acordes y voicings
│   ├── merger.h             # Fusión de las capas melódica y armónica
│   └── viterbi.h            # Optimización temporal de posiciones
│
├── csv_parser.h / .cpp      # Lectura de archivos CSV
├── tab_generator.h / .cpp   # Formateo de tablatura ASCII por compases y .txt
├── audio_player.h / .cpp    # Síntesis, efectos DSP y salida waveOut
│
├── intro_splash.h / .cpp    # código de la animación
├── app_icon.rc              # Para mostrar el Icono en el buscador de archivos
├── resources.qrc            # Recursos gráficos embebidos en el binario
├── CMakeLists.txt           # Configuración de compilación
└── mainwindow.h / .cpp/.ui  # Interfaz gráfica en Qt 6

```

---

## Pipeline de Datos

1. **Conversor MIDI (`midi2csv.py`):** Lee el `.mid`, descarta pistas marcadas como percusión o bajo y conserva notas dentro del rango MIDI 40 a 88. Genera un `.csv` por pista con nueve columnas y un archivo `.meta` con el BPM y la métrica.
2. **`csv_parser`:** Parsea el CSV y construye un `std::vector<Nota>`.
3. **`mapper`:**
* `construirContexto`: agrupa notas cercanas, reconoce acordes y estima la tonalidad.
* `construirTimeline`: divide la pista en instantes con notas atacadas y sostenidas.
* Los motores melódico y armónico generan candidatos y Viterbi elige las secuencias de menor costo.
* `Merger`: combina ambos resultados y resuelve colisiones entre cuerdas.
4. **`tab_generator`:** Convierte el `ResultadoMapeo` en tablatura ASCII alineada usando el tempo del `.meta`.
5. **`audio_player`:** Sintetiza las notas y envía los buffers de audio a la tarjeta de sonido mediante `waveOut`.

---

## El Motor de Audio (DSP)

El procesamiento de audio corre muestra por muestra a 44.1 kHz en un hilo separado de la interfaz:

* **Karplus-Strong:** Línea de retardo circular cuya lectura y realimentación modelan la excitación y el amortiguamiento de una cuerda.
* **Síntesis aditiva:** `y[n] = suma(sin(fase_h) * envolvente_h * peso_h)` para `h` en `[1..7]`, con envolventes independientes para cada armónico.
* **Overdrive:** Saturación mediante `tanh`, controlada por el dial de ganancia (1x a 60x) y un impulso fijo adicional al activar el botón OD.
* **Ecualizador:** Tres filtros Biquad en cascada para Bass, Mid y Treble.
* **Chorus:** Buffer circular modulado por un oscilador de baja frecuencia (LFO).
* **Limitación y conversión:** La etapa final suaviza los picos mediante `tanh` y convierte la señal a PCM de 16 bits.

---

## Tecnologías Utilizadas

* **Lenguaje:** C++17
* **Interfaz Gráfica:** Qt 6 (Qt Widgets y Qt Designer)
* **Audio:** Windows Multimedia API (`waveOut`, `winmm.lib`)
* **Build System:** CMake (compilado con MinGW 64-bit)
* **Conversor MIDI:** Python 3 con `pretty_midi` y soporte opcional para empaquetarlo como ejecutable

---

## Requisitos y Ejecución

### Opción 1: Usar la compilación ya ejecutable
El release 1.0 solo debe extraerse y se puede ejecutar directamente el archivo ejecutable.

* Solo es necesario ejecutar `GuitarTabCreator.exe` y cargar cualquier archivo `.mid` o `.csv` desde el botón **Abrir canción**.

### Opción 2: Compilar desde el código fuente

Requisitos previos:

* Windows 10 o superior (64 bits).
* CMake 3.16 o superior.
* Qt 6 o Qt 5 (módulo `Widgets`) con compilador compatible con C++17 (MinGW o MSVC).
* Python 3 con `pip install pretty_midi` (solo si no se usa `midi2csv.exe`).

```bash
# 1. Clonar el repositorio
git clone https://github.com/one-and-only-H/GuitarTabStudio_source.git
cd GuitarTabStudio_source

# 2. Configurar con CMake (ajustar ruta segun instalacion de Qt)
cmake -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.12.0/mingw_64"

# 3. Compilar en Release
cmake --build build --config Release

# 4. Ejecutar
./build/GuitarTabCreator.exe

```

---

## Decisiones de Diseño

1. **Funciones auxiliares `inline`:** El código comparte funciones primitivas en los headers para mantener el pipeline compacto y facilitar su reutilización.
2. **Separación del audio:** El procesamiento se ejecuta en un hilo independiente de la interfaz, mientras los tres buffers de salida se reutilizan de forma circular.
3. **Uso de estructuras de tamaño fijo:** Las envolventes, fases y filtros emplean arreglos y `std::array` donde el tamaño es conocido.
4. **Concurrencia segura:** La interfaz y el hilo de `waveOut` intercambian reproducción y parámetros mediante variables `std::atomic`.

---

## Estado de los Módulos

| Módulo | Estado | Descripción |
| --- | --- | --- |
| `tipos.h` / `tipos_mapper.h` | Completado | Estructuras de datos del dominio |
| `acordes.cpp` | Completado | Análisis armónico y detección de tonalidad |
| `formas_conocidas.h` / `candidatos.h` | Completado | Biblioteca de formas y generación de posiciones |
| `viterbi.h` | Completado | Optimización de trayectoria en el diapasón |
| `mapper.cpp` | Completado | Orquestador del pipeline de mapeo |
| `csv_parser.cpp` | Completado | Lectura y validación de archivos CSV |
| `audio_player.h / .cpp` | Completado | Motor Karplus-Strong, aditivo, efectos y `waveOut` |
| `tab_generator.cpp` | Completado | Generación de tablatura por compases y exportación `.txt` |
| `mainwindow.ui/ .cpp / .h` (Qt) | Completado | Interfaz gráfica, iconos y sincronización de reproducción |

---

## Limitaciones y Trabajo Futuro

Para mantener el alcance de esta versión 1.0 manejable, quedaron por fuera algunas funciones que podrían añadirse más adelante:

* **Digitación de mano izquierda:** No se indica con qué dedo (1 a 4) pisar cada traste ni se marcan cejillas explícitas.
* **Técnicas expresivas:** No se infieren *hammer-ons*, *pull-offs* o *bends* automáticamente desde el MIDI; el CSV generado fija la técnica como `normal`.
* **Afinaciones alternativas:** Solo trabaja en afinación estándar (EADGBE), sin soporte para Drop D o uso de capo.
* **Cobertura de acordes:** La biblioteca incluye acordes mayores, menores, séptimas, suspendidos, disminuidos, aumentados y power chords; otras extensiones pueden requerir nuevas formas.
* **Exportación a PDF o MusicXML:** Por ahora la salida es únicamente en formato de texto plano ASCII.

---

*Hecho con C++ y muchas peleas con GitHub.*
