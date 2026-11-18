
# Guitar Tab Studio

**Tab Studio** es una aplicación de escritorio para Windows que convierte archivos MIDI en tablaturas de guitarra tocables, y las reproduce con un motor de síntesis y efectos de audio en tiempo real.

El proyecto combina tres disciplinas: **teoría musical** (análisis armónico), **optimización algorítmica** (Viterbi) y **procesamiento digital de señales (DSP)**.

---
## REQUISITOS
1. compilador compatible con c++17
2. pip install pretty_midi
3. SO windows 10 o superior


## Tabla de Contenidos
1. [Características Principales](#-características-principales)
2. [Arquitectura del Proyecto](#-arquitectura-del-proyecto)
3. [Pipeline de Datos](#-pipeline-de-datos)
4. [El Motor de Audio (DSP)](#-el-motor-de-audio-dsp)
5. [Tecnologías Utilizadas](#-tecnologías-utilizadas)
6. [Compilación y Ejecución](#-compilación-y-ejecución)
7. [Reglas de Programación (Divide and Conquer)](#-reglas-de-programación-divide-and-conquer)
8. [Estado del Proyecto](#-estado-del-proyecto)
9. [Deuda Técnica (Post-MVP)](#-deuda-técnica-post-mvp)


##  Características Principales

*   **Mapeo Inteligente:** Utiliza el algoritmo de Viterbi para elegir posiciones en el diapasón que minimicen el movimiento de la mano, evitando saltos incómodos.
*   **Biblioteca CAGED:** Reconoce y prioriza formas de acordes estándar (campfire chords) para que la tablatura suene natural.
*   **Análisis Armónico:** Agrupa notas simultáneas, reconoce acordes (incluyendo inversiones) y detecta la tonalidad de la pieza (Krumhansl-Schmuckler).
*   **Síntesis Aditiva:** Cada nota se genera con 7 armónicos, cada uno con su propia envolvente ADSR para simular la física real de una cuerda de guitarra.
*   **Efectos en Tiempo Real:** Ecualizador de 3 bandas (Biquads), Overdrive (tanh) y Chorus (LFO + delay), ajustables mientras la canción suena.
*   **Modelado de Pastillas:** Perfiles de sonido distintos para Single Coil (brillante) y Humbucker (gordo).

---

##  Arquitectura del Proyecto

El proyecto sigue una arquitectura modular (Divide and Conquer), donde cada archivo tiene una única responsabilidad.

```text
MIDI_GUITARRA/
│
├── scripts/
│   └── midi_to_csv.py       # Convierte MIDI a CSV (filtra notas fuera de rango)
├── songs/                   # Archivos CSV de prueba
│
├── tipos.h                  # Estructuras de dominio (Nota, EventoArmonico)
├── tipos_mapper.h           # Configuración y candidatos del mapper
│
├── acordes.h / .cpp         # Agrupación, reconocimiento y tonalidad
├── candidatos.h             # Biblioteca CAGED y generación combinatoria
├── viterbi.h                # Optimización de secuencias (melodía/armonía)
│
├── mapper/
│   ├── mapper.h / .cpp      # Orquestador (Contexto, Timeline, Post-proceso)
│
├── csv_parser.h / .cpp      # Lectura del CSV
├── tab_generator.h / .cpp   # Exportación a tablatura ASCII
├── audio_player.h / .cpp    # Síntesis y efectos en tiempo real
│
└── mainwindow.h / .cpp/.ui  # Interfaz gráfica (Qt 6)
```

---

##  Pipeline de Datos

1.  **Python (`midi_to_csv.py`)**: Lee el `.mid`, filtra notas imposibles en guitarra (E2–E6) y exporta un CSV de 9 columnas.
2.  **`csv_parser`**: Lee el CSV y devuelve un `std::vector<Nota>`.
3.  **`mapper`**:
    *   `construirContexto`: Agrupa acordes, detecta tonalidad y resuelve solapamientos.
    *   `construirTimeline`: Divide el tiempo en instantes con notas atacadas/sostenidas.
    *   `candidatos` + `viterbi`: Eligen las posiciones óptimas.
    *   `postProcesar`: Fusiona melodía y armonía, resolviendo colisiones de cuerdas.
4.  **`tab_generator`**: Convierte el `ResultadoMapeo` en una tablatura ASCII alineada.
5.  **`audio_player`**: Toma las notas, las sintetiza y las reproduce por `waveOut`.

---

## El Motor de Audio (DSP)

El audio se procesa muestra por muestra (44.1 kHz) en un hilo separado de la UI.

*   **Síntesis Aditiva:** `y[n] = suma(sin(fase_h) * envolvente_h * peso_h)` para `h` en `[1..7]`.
*   **ADSR Individual:** Los armónicos agudos tienen un decaimiento mucho más rápido que la fundamental.
*   **Overdrive:** `y = tanh(a * x)`, donde `a` es la ganancia total (Dial + Boost).
*   **EQ:** Filtros Biquad en cascada (Bass, Mid, Treble).
*   **Chorus:** Buffer circular con lectura modulada por un LFO.
*   **Normalización:** División por `sqrt(N)` notas para evitar saltos de volumen bruscos entre acordes y notas solas.
*   **Limitador Final:** `y = tanh(x / 0.8) * 0.8` para evitar el *clipping* duro.

---

##  Tecnologías Utilizadas

*   **Lenguaje:** C++17
*   **Interfaz Gráfica:** Qt 6 (Qt Widgets + Qt Designer)
*   **Audio:** Windows API (`waveOut`, `winmm.lib`)
*   **Build System:** CMake
*   **Scripting:** Python 3 (con `mido` o `pretty_midi` para la conversión)

---

## Compilación y Ejecución

### Requisitos Previos
*   Windows 10 o superior (64 bits).
*   CMake 3.21+.
*   Qt 6.5+ (con los módulos `Widgets`).
*   Un compilador de C++ compatible con C++17 (MSVC o MinGW).

### Pasos
```bash
# 1. Clonar el repositorio
git clone https://github.com/tu-usuario/tab-studio.git
cd tab-studio

# 2. Generar el proyecto con CMake
cmake -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.5.0/msvc2019_64"

# 3. Compilar
cmake --build build --config Release

# 4. Ejecutar
./build/Release/TabStudio.exe
```

*(Nota: Asegurate de tener el archivo `songs/cancion.csv` en el directorio de ejecución, o cargarlo desde la UI).*

---

## 📐 Reglas de Programación (Divide and Conquer)

Para mantener el código limpio y rápido, el proyecto sigue estas reglas:

1.  **Funciones primitivas `inline`:** Las funciones de bajo nivel (síntesis, costos) se definen en los headers para evitar llamadas de función costosas.
2.  **Cero `std::vector` en el bucle de audio:** El bucle de 44,100 Hz usa arreglos C (`float[7]`) en el Stack para evitar fragmentación de memoria (memory churn) y chasquidos.
3.  **`std::array` para retornos:** Como los arreglos C no se pueden retornar por valor, se usa `std::array` para funciones como `dividirEnArmonicos`.
4.  **Hilos separados:** La UI (Qt) y el audio (`waveOut`) corren en hilos distintos. La comunicación se hace mediante `std::atomic`.
5.  **C++ básico:** Sin sobreingeniería. Ciclos `for`, condicionales `if` y estructuras simples.

---

## Estado del Proyecto

| Módulo | Estado | Descripción |
| :--- | :--- | :--- |
| `tipos.h` / `tipos_mapper.h` | ✅ Completado | Estructuras base |
| `acordes.cpp` | ✅ Completado | Análisis armónico y tonalidad |
| `candidatos.h` | ✅ Completado | Biblioteca CAGED + combinatoria |
| `viterbi.h` | ✅ Completado | Optimización de secuencias |
| `mapper.cpp` | ✅ Completado | Orquestador del pipeline |
| `csv_parser.cpp` | ✅ Completado | Lectura de CSV |
| `audio_player.h` | ✅ Completado | Funciones primitivas de DSP |
| `audio_player.cpp` | ⏳ En progreso | Bucle de `waveOut` y efectos |
| `tab_generator.cpp` | ⏳ Pendiente | Exportación a ASCII |
| `mainwindow` (Qt) | ⏳ En progreso | Interfaz gráfica |

**Tests:** El motor de mapeo cuenta con más de 60 pruebas unitarias (acordes, candidatos, Viterbi y pipeline completo) que pasan exitosamente.

---

##  Deuda Técnica

Estas características fueron deliberadamente excluidas del MVP para mantener el alcance manejable:

*   **Digitación:** No se asignan dedos (1-4) ni se detectan cejillas.
*   **Técnicas:** No se infieren *hammer-ons*, *pull-offs* o *bends* (se respetan los del CSV si existen).
*   **Afinaciones Alternativas:** Solo afinación estándar (EADGBE). Sin Drop D ni capo.
*   **Acordes Extendidos:** La biblioteca CAGED no incluye `sus2`, `sus4`, `add9`, `6`, etc.
*   **Exportación a PDF:** Solo se genera tablatura ASCII por ahora.

---
*Hecho con ❤️, C++ y muchas salchipapas.*
```

### ¿Por qué este README es útil para tu equipo?
1. **Alinea a todos:** Si alguien se suma al proyecto, entiende la arquitectura en 5 minutos.
2. **Justifica las decisiones:** Explica *por qué* usamos arreglos C en el audio y *por qué* no usamos la GPU (para que no pregunten).
3. **Muestra el estado real:** Deja claro qué está terminado y qué falta, para que nadie asuma que el `audio_player.cpp` ya está listo.
4. **Es profesional:** Un buen README es la diferencia entre un proyecto universitario y un proyecto que da gusto leer en GitHub.
