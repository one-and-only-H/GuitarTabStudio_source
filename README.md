# GuitarTabStudio

**GuitarTabStudio** es una aplicacion de escritorio para Windows que convierte archivos MIDI en tablaturas de guitarra tocables, y las reproduce en tiempo real usando sintesis de audio y efectos digitales.

El proyecto junta tres areas principales: **teoria musical** (analisis armonico y posicionamiento en el diapason), **optimizacion algoritmica** (Viterbi) y **procesamiento digital de señales (DSP)**.

---

## Tabla de Contenidos
1. [Caracteristicas Principales](#caracteristicas-principales)
2. [Arquitectura del Proyecto](#arquitectura-del-proyecto)
3. [Pipeline de Datos](#pipeline-de-datos)
4. [El Motor de Audio (DSP)](#el-motor-de-audio-dsp)
5. [Tecnologias Utilizadas](#tecnologias-utilizadas)
6. [Requisitos y Ejecucion](#requisitos-y-ejecucion)
7. [Decisiones de Diseño](#decisiones-de-diseño)
8. [Estado de los Modulos](#estado-de-los-modulos)
9. [Limitaciones y Trabajo Futuro](#limitaciones-y-trabajo-futuro)

---

## Caracteristicas Principales

- **Mapeo Inteligente:** Usa el algoritmo de Viterbi para elegir trastes y cuerdas que minimicen el movimiento de la mano izquierda, evitando saltos incomodos en el mastil.
- **Biblioteca CAGED:** Reconoce y prioriza formas de acordes abiertas y estandar (campfire chords) para que la digitacion sea natural.
- **Analisis Harmonico:** Agrupa notas simultaneas, identifica acordes (incluyendo inversiones) y estima la tonalidad de la pieza con el algoritmo de Krumhansl-Schmuckler.
- **Doble Motor de Sintesis:** Permite alternar entre **Karplus-Strong** (modelado fisico de cuerda pulsada) y **Sintesis Aditiva** (7 armonicos con envolventes ADSR independientes).
- **Efectos en Tiempo Real:** Ecualizador de 3 bandas (filtros Biquad), Overdrive (`tanh`) con switch de boost y Chorus (LFO + linea de retardo), modificables mientras suena la pista.
- **Modelado de Pastillas:** Simula la respuesta en frecuencia de pastillas Single Coil (mas brillante) y Humbucker (con mayor cuerpo).
- **Sincronizacion Visual y Exportacion:** La barra de desplazamiento sigue la reproduccion por compases, soporta *seek* manual y permite exportar la tablatura final a `.txt`.

---

## Arquitectura del Proyecto

El codigo esta dividido por modulos independientes para separar la logica musical, el procesamiento de audio y la interfaz grafica:

```text
GuitarTabCreator/
│
├── assets/
│   └── icons/               # Iconos PNG de los controles de transporte
├── scripts/
│   ├── midi2csv.py          # Conversor de MIDI a CSV + metadatos (.meta)
│   └── midi2csv.exe         # Version empaquetada para PCs sin Python
├── songs/                   # Canciones de prueba (.mid, .csv y .meta)
│
├── tipos.h                  # Estructuras base (Nota, EventoArmonico)
├── tipos_mapper.h           # Configuracion y candidatos del mapper
├── acordes.h / .cpp         # Agrupacion, reconocimiento y tonalidad
├── candidatos.h             # Biblioteca CAGED y generacion combinatoria
├── viterbi.h                # Optimizacion de secuencias
│
├── mapper/
│   └── mapper.h / .cpp      # Orquestador (Contexto, Timeline, Post-proceso)
│
├── csv_parser.h / .cpp      # Lectura de archivos CSV
├── tab_generator.h / .cpp   # Formateo de tablatura ASCII por compases y .txt
├── audio_player.h / .cpp    # Sintesis, efectos DSP y salida waveOut
│
├── resources.qrc            # Recursos graficos embebidos en el binario
├── CMakeLists.txt           # Configuracion de compilacion
└── mainwindow.h / .cpp/.ui  # Interfaz grafica en Qt 6
