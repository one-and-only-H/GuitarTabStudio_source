# Glosario musical y técnico

Este glosario explica los términos musicales que aparecen en GuitarTabCreator y, después, los algoritmos que los conectan con la tablatura y el sonido. Los ejemplos usan nombres de nota anglosajones (C, D, E...), como en los archivos MIDI y en el código.

## Términos musicales

| Término | Definición |
| --- | --- |
| **Nota** | Sonido musical identificado por una altura y una duración. En los datos del proyecto también puede tener un instante de inicio, una intensidad y una función musical. |
| **Altura (pitch)** | Qué tan grave o agudo es un sonido. En MIDI se representa con un número entero; por ejemplo, 60 corresponde a C4 y 69 a A4. |
| **Clase de altura (pitch class)** | La identidad de una nota sin considerar su octava. Hay 12 en la escala cromática: C=0, C♯=1, D=2, D♯=3, E=4, F=5, F♯=6, G=7, G♯=8, A=9, A♯=10 y B=11. C4 y C5 son distintas alturas, pero comparten la clase C. |
| **Semitono** | La distancia de una tecla a la siguiente en el teclado, incluida la distancia entre una nota natural y su sostenido o bemol. La escala cromática tiene 12 semitonos por octava. |
| **Intervalo** | Distancia entre dos alturas. Por ejemplo, una tercera mayor equivale a cuatro semitonos y una quinta justa a siete. Los acordes se describen mediante intervalos desde su raíz. |
| **Escala** | Conjunto ordenado de notas que sirve como material para una melodía o armonía. Una escala mayor y su relativa menor pueden compartir las mismas notas, pero tienen centros tonales diferentes. |
| **Tonalidad** | Centro musical de una pieza y su modo, normalmente mayor o menor; por ejemplo, C mayor o A menor. No es simplemente la nota que más aparece: importan los patrones y el peso relativo de las 12 clases de altura. |
| **Tónica** | Nota que funciona como centro o punto de reposo de una tonalidad. En C mayor, la tónica es C. |
| **Acorde** | Varias clases de altura que se perciben como una unidad armónica. Por ejemplo, C mayor contiene C, E y G. |
| **Raíz** | Nota que da nombre al acorde y desde la que se miden sus intervalos. La raíz de C mayor es C, aunque C no sea necesariamente la nota más grave que suena. |
| **Calidad del acorde** | Tipo de acorde definido por sus intervalos, como mayor, menor, disminuido, aumentado, suspendido o de séptima. El reconocedor del proyecto compara las notas con plantillas de estas calidades. |
| **Bajo del acorde** | Nota más grave del acorde en ese momento. Puede coincidir con la raíz o ser otra nota del acorde. |
| **Inversión** | Disposición de un acorde según cuál de sus notas está en el bajo. Por ejemplo, C-E-G con E como nota más grave es una inversión de C mayor, no un acorde con otra raíz. |
| **Armonía** | Relación entre notas que suenan juntas o entre acordes sucesivos. El motor armónico del proyecto busca posiciones de guitarra para los acordes reconocidos. |
| **Melodía** | Secuencia de notas que se percibe como una línea musical. El motor melódico busca una posición de guitarra para cada nota y procura que el recorrido sea tocable. |
| **Arpegio** | Notas de un acorde tocadas una después de otra, en lugar de simultáneamente. El proyecto detecta patrones básicos de ataques compatibles con formas conocidas y los usa como preferencia al mapear notas. |
| **Voicing (disposición)** | Forma concreta de distribuir las notas de un acorde entre las cuerdas y los trastes. Un mismo acorde admite varios voicings, con diferente orden, duplicaciones y notas al aire. |
| **Digitación** | Qué dedos de la mano izquierda pisan cada cuerda y traste. No es lo mismo que un voicing: el proyecto optimiza posiciones y formas, pero no asigna dedos individualmente. |
| **Diapasón** | Superficie del mástil donde se pisan las cuerdas. En la tablatura, una nota queda descrita por una cuerda y un traste. |
| **Cuerda al aire** | Cuerda tocada sin presionar ningún traste; en la tablatura se indica con 0. |
| **Traste** | División del diapasón que eleva la altura de una cuerda un semitono por cada posición. El traste 0 representa la cuerda al aire. |
| **Afinación estándar** | Afinación de guitarra de seis cuerdas E2-A2-D3-G3-B3-E4, de la cuerda más grave a la más aguda. Las posiciones se calculan tomando estas alturas como referencia. |
| **MIDI** | Formato y protocolo que describen eventos musicales —por ejemplo, nota, inicio, duración e intensidad—, no una grabación de audio. Un archivo MIDI puede contener varias pistas o instrumentos. |
| **Número de nota MIDI** | Número entero asociado a una altura, independiente del nombre de la cuerda o instrumento. El proyecto lo usa para comparar notas, calcular su clase de altura y hallar posiciones en el mástil. |
| **Velocidad MIDI (velocity)** | Valor asociado al ataque de una nota que suele representar qué tan fuerte se tocó. No es la velocidad de reproducción. En este proyecto también pondera la contribución de una nota al análisis tonal. |
| **Tempo (BPM)** | Velocidad del pulso musical, expresada en pulsos por minuto. |
| **Compás o métrica** | Organización recurrente de los pulsos, indicada por una fracción como 4/4 o 3/4. |
| **Tablatura** | Notación que muestra las cuerdas de la guitarra y el traste donde tocar cada nota, en vez de escribir las alturas en un pentagrama. |
| **Ataque y sustain** | El ataque es el inicio de una nota; el sustain es el tiempo durante el que continúa sonando. Distinguirlos ayuda a separar nuevos eventos de notas que siguen activas. |
| **Sostenido (♯) y bemol (♭)** | Alteraciones que suben o bajan una nota un semitono. Por ejemplo, C♯ y D♭ suelen sonar igual en afinación temperada, aunque su escritura y función musical puedan diferir. El proyecto usa sostenidos para nombrar las 12 clases de altura de forma uniforme. |

## Algoritmos y procesamiento de audio

### Problema de Markov

En términos simples, un modelo de Markov supone que, para describir el siguiente paso de una secuencia, basta con conocer el estado actual (o un contexto limitado), en vez de recordar toda la historia. Por ejemplo, al elegir la siguiente posición de guitarra, interesa cuánto cuesta pasar desde la posición anterior.

### Algoritmo de Viterbi

Viterbi encuentra la secuencia de estados más conveniente entre varias opciones posibles en cada instante. En el caso clásico, cada estado tiene una probabilidad y el algoritmo encuentra la secuencia más probable en un modelo de Markov. En GuitarTabCreator se usa la misma idea de optimización dinámica, expresada como **costos**:

1. Para cada nota o acorde se generan posiciones candidatas en el diapasón.
2. Cada candidato tiene un costo local, por ejemplo por usar trastes altos o una forma menos conveniente.
3. Cada cambio entre candidatos tiene un costo de transición, por ejemplo por mover mucho la mano o cambiar de voicing.
4. El algoritmo conserva el menor costo acumulado y reconstruye la secuencia completa de menor costo.

**Qué resuelve:** elegir posiciones que funcionen bien como frase o progresión, no solo escoger la opción más cómoda nota por nota. Se aplica por separado a la línea melódica y a los voicings armónicos. La implementación trabaja con costos y no calcula probabilidades explícitas.

### Detección de tonalidad: perfiles Krumhansl–Schmuckler

El proyecto estima la tonalidad global con el método Krumhansl–Schmuckler. Primero acumula un histograma de las 12 clases de altura presentes; cada nota aporta un peso basado en su duración y, si tiene velocity positiva, en su velocity. Luego compara ese histograma con perfiles de referencia para las 12 tonalidades mayores y las 12 menores, usando la correlación de Pearson. La mejor coincidencia es la tonalidad estimada.

**Qué resuelve:** proponer el centro tonal general de la pieza a partir del conjunto de notas, algo útil como resumen del contexto musical. Es una estimación: una pieza ambigua, breve o con cambios de tonalidad puede no tener una única respuesta clara.

### Reconocimiento de acordes por plantillas

Para cada evento, el reconocedor reduce las notas a sus clases de altura y compara los intervalos observados con plantillas de acordes (mayor, menor, séptima y otras calidades incluidas). También registra la nota más grave como bajo del acorde.

**Qué resuelve:** asignar un nombre y una calidad a grupos de notas simultáneas para que el motor armónico pueda generar voicings candidatos. Como compara contra un conjunto limitado de plantillas, no pretende identificar cualquier acorde posible.

### Agrupación temporal de notas

Las notas cuyos inicios están dentro de una tolerancia temporal se reúnen en un mismo evento armónico. Así se pueden analizar juntas las notas que probablemente forman un acorde.

**Qué resuelve:** convertir una lista de notas individuales en unidades de análisis simultáneas, antes de reconocer acordes y estimar la tonalidad.

### Detección de patrones de arpegio

El analizador busca ventanas de ataques que encajen con patrones y formas de guitarra conocidas, incluyendo sus transposiciones válidas en el mástil. La coincidencia se usa como preferencia para elegir posiciones melódicas.

**Qué resuelve:** favorecer una forma reconocible y coherente cuando una secuencia de notas parece provenir de un arpegio. Es una detección básica; no equivale a una transcripción completa de la técnica interpretativa.

### Filtro Biquad

Un Biquad es un filtro digital recursivo de segundo orden: calcula cada muestra de salida usando la muestra actual, entradas anteriores y salidas anteriores. Sus coeficientes determinan qué frecuencias deja pasar, atenúa o realza.

**Qué resuelve:** modificar el timbre de la señal de audio sin tener que analizar nota por nota. En el proyecto, filtros Biquad implementan las bandas Bass, Mid y Treble del ecualizador y varias etapas de modelado de gabinete; por ejemplo, para limpiar subgraves, realzar presencia o atenuar agudos. No es un algoritmo de análisis musical.

### Síntesis Karplus–Strong

Método de síntesis de cuerda pulsada que alimenta una línea de retardo con ruido y la realimenta con filtrado y amortiguación. La longitud del retardo está relacionada con la frecuencia de la nota.

**Qué resuelve:** generar un sonido parecido al de una cuerda pulsada a partir de una altura MIDI, sin reproducir una grabación.

### Síntesis aditiva

Construye un sonido sumando ondas sinusoidales de varias frecuencias. En el sintetizador del proyecto se usan siete armónicos con pesos y envolventes para dar forma al timbre.

**Qué resuelve:** crear y controlar el timbre de cada nota mediante sus componentes armónicos, como una segunda alternativa a Karplus–Strong.

### Chorus con LFO y retardo

El chorus mezcla la señal con copias ligeramente retardadas cuyo tiempo cambia según un LFO (oscilador de baja frecuencia).

**Qué resuelve:** dar una sensación de amplitud y modulación al sonido. El LFO cambia el retardo; no cambia la secuencia de notas.

### Overdrive y saturación

El overdrive aumenta la ganancia y aplica una saturación no lineal suave (en el proyecto, mediante `tanh`), que añade armónicos al sonido.

**Qué resuelve:** producir un timbre más saturado o distorsionado, como efecto de audio, sin alterar la tablatura ni las alturas MIDI de origen.
