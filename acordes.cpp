/*
 * ARCHIVO: acordes.h / acordes.cpp
 *
 * CONTEXTO DE LO QUE YA SE TIENE:
 * Ya se dispone del std::vector<Nota> completo cargado en memoria por el
 * modulo csv_parser.
 *
 * HERRAMIENTAS A USAR:
 * "tipos.h" y operaciones matematicas/arreglos de C++ (<vector>, <string>,
 * <array>, <cmath>, <algorithm>).
 *
 * QUE DEBE HACER ESTE ARCHIVO:
 * Agrupar notas que suenan dentro de una misma ventana de tiempo en objetos
 * EventoArmonico, identificar el nombre e inversion del acorde resultante y
 * estimar la tonalidad global de la cancion usando los perfiles de
 * Krumhansl-Schmuckler.
 */


 
#include "acordes.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

// Nombres de las doce clases de altura
// Usamos sostenidos para mantener una representación uniforme (pq por ejemplo, C# = Db, asi que solo trabajaremos con sostenido)
static const std::string NOMBRES_NOTAS[12] = {
    "C", "C#", "D", "D#", "E", "F",
    "F#", "G", "G#", "A", "A#", "B"
};

// Plantilla de un acorde
struct PlantillaAcorde {
    int calidad;
    std::string sufijo;
    std::vector<int> intervalos;
};

// Plantillas interválicas (osea por intervalos relativos a la raíz) de los acordes más comunes
static const std::vector<PlantillaAcorde> PLANTILLAS = {
    {CalidadAcorde::MAYOR, "",      {0, 4, 7}},
    {CalidadAcorde::MENOR, "m",     {0, 3, 7}},
    {CalidadAcorde::DISMINUIDO, "dim", {0, 3, 6}},
    {CalidadAcorde::AUMENTADO, "aug",  {0, 4, 8}},
    {CalidadAcorde::SUS2, "sus2",   {0, 2, 7}},
    {CalidadAcorde::SUS4, "sus4",   {0, 5, 7}},
    {CalidadAcorde::SEPTIMA, "7",   {0, 4, 7, 10}},
    {CalidadAcorde::MAYOR_SEPTIMA, "maj7", {0, 4, 7, 11}},
    {CalidadAcorde::MENOR_SEPTIMA, "m7",  {0, 3, 7, 10}},
    {CalidadAcorde::SEMIDISMINUIDO, "m7b5", {0, 3, 6, 10}},
    {CalidadAcorde::POWER_CHORD, "5", {0, 7}}
};



// Devuelve el nombre de una nota a partir de su pitch MIDI.
static std::string nombreNota(int pitch) {
    return NOMBRES_NOTAS[pitchClass(pitch)];
}

// Obtiene el sufijo de una calidad.
static std::string obtenerSufijo(int calidad) {
    for (const auto& plantilla : PLANTILLAS) {
        if (plantilla.calidad == calidad) {
            return plantilla.sufijo;
        }
    }

    return "";
}

// Calcula la similitud entre las notas observadas
// y una plantilla de acorde.
static double calcularCoincidencia(
    const std::vector<int>& notas,
    const std::vector<int>& plantilla
) {
    int coincidencias = 0;

    for (int intervalo : plantilla) {
        int notaEsperada = intervalo;

        if (std::find(
            notas.begin(),
            notas.end(),
            notaEsperada
        ) != notas.end()) {
            coincidencias++;
        }
    }

    int extras = static_cast<int>(notas.size()) - coincidencias;

    int total = static_cast<int>(plantilla.size()) + extras;

    if (total == 0) {
        return 0.0;
    }

    return static_cast<double>(2 * coincidencias) / total;
}

// Agrupa notas cuyos comienzos están suficientemente próximos.
std::vector<EventoArmonico> agruparNotas(
    const std::vector<Nota>& notas,
    double tolerancia
) {
    std::vector<Nota> ordenadas = notas;
    
    std::sort(
        ordenadas.begin(),
        ordenadas.end(),
        [](const Nota& a, const Nota& b) {
            return a.timestamp < b.timestamp;
        }
    );

    std::vector<EventoArmonico> eventos;

    if (ordenadas.empty()) {
        return eventos;
    }

    tolerancia = std::max(0.0, tolerancia);

    EventoArmonico actual;
    double inicioGrupo = ordenadas[0].timestamp;
    double finGrupo = inicioGrupo;

    actual.timestamp = inicioGrupo;

    for (const Nota& nota : ordenadas) {
        if (!actual.notas.empty() &&
            nota.timestamp - inicioGrupo > tolerancia) {

            actual.duracion = std::max(
                0.0,
                finGrupo - actual.timestamp
            );
            actual.es_simultaneo = actual.notas.size() > 1;
            eventos.push_back(actual);

            actual = EventoArmonico();
            inicioGrupo = nota.timestamp;
            finGrupo = nota.timestamp;

            actual.timestamp = inicioGrupo;
        }

        actual.notas.push_back(nota);

        finGrupo = std::max(
            finGrupo,
            nota.timestamp + nota.duracion
        );
    }

            actual.duracion = std::max(
                0.0,
                finGrupo - actual.timestamp
            );
            actual.es_simultaneo = actual.notas.size() > 1;

            // NUEVO: Si el grupo tiene más de 6 notas, es un error de agrupación.
            // Nos quedamos con las 6 más graves (rango de la guitarra).
            if (actual.notas.size() > 6) {
                std::sort(actual.notas.begin(), actual.notas.end(),
                    [](const Nota& a, const Nota& b) { return a.pitch < b.pitch; });
                actual.notas.resize(6);
            }

            eventos.push_back(actual);

    return eventos;
}

// Reconoce la raíz, calidad e inversión de cada evento.
void reconocerAcordes(
    std::vector<EventoArmonico>& eventos,
    double umbral_confianza
) {
    for (auto& evento : eventos) {
        evento.nombreAcorde = "";
        evento.root_pitch_class = -1;
        evento.quality_index = -1;
        evento.bass_pitch_class = -1;
        evento.es_simultaneo = false;   // default: no es cluster

        if (evento.notas.empty()) {
            continue;
        }

        // Obtener las clases de altura únicas.
        std::vector<int> pitchClasses;

        int pitchMasGrave = std::numeric_limits<int>::max();

        for (const Nota& nota : evento.notas) {
            if (nota.pitch < 0) {
                continue;
            }

            int pc = pitchClass(nota.pitch);

            if (std::find(
                pitchClasses.begin(),
                pitchClasses.end(),
                pc
            ) == pitchClasses.end()) {
                pitchClasses.push_back(pc);
            }

            pitchMasGrave = std::min(
                pitchMasGrave,
                nota.pitch
            );
        }

        if (pitchClasses.empty()) {
            continue;
        }
        if (pitchClasses.size() < 2) {
            continue;
        }

        std::sort(pitchClasses.begin(), pitchClasses.end());

        // Identificar la nota más grave.
        evento.bass_pitch_class =
            pitchClass(pitchMasGrave);

        double mejorPuntuacion = -1.0;

        int mejorRaiz = -1;
        int mejorCalidad = -1;

        // Probar las doce raíces posibles.
        for (int raiz = 0; raiz < 12; raiz++) {

            // Convertir las notas observadas
            // en intervalos relativos a la raíz.
            std::vector<int> intervalosObservados;

            for (int pc : pitchClasses) {
                intervalosObservados.push_back(
                    pitchClass(pc - raiz)
                );
            }

            // Comparar contra todas las plantillas.
            for (const auto& plantilla : PLANTILLAS) {

                double puntuacion = calcularCoincidencia(
                    intervalosObservados,
                    plantilla.intervalos
                );

                bool mejorCoincidencia = puntuacion > mejorPuntuacion;

                bool empateConBajo =
                    puntuacion == mejorPuntuacion &&
                    raiz == evento.bass_pitch_class &&
                    mejorRaiz != evento.bass_pitch_class;

                if (mejorCoincidencia || empateConBajo) {
                    mejorPuntuacion = puntuacion;
                    mejorRaiz = raiz;
                    mejorCalidad = plantilla.calidad;
                }
            }
        }

        if (mejorPuntuacion < umbral_confianza) {
            // No se reconoció acorde: si hay 2+ notas, es cluster.
            evento.es_simultaneo = (evento.notas.size() >= 2);
            continue;
        }

        // Se reconoció acorde: es_simultaneo queda en false.
        evento.root_pitch_class = mejorRaiz;
        evento.quality_index = mejorCalidad;

        evento.nombreAcorde =
            nombreNota(mejorRaiz) +
            obtenerSufijo(mejorCalidad);

        // Si el bajo no es la raíz, indicar la inversión.
        if (evento.bass_pitch_class != mejorRaiz) {
            evento.nombreAcorde += "/";
            evento.nombreAcorde +=
                nombreNota(evento.bass_pitch_class);
        }
    }
}


/* Perfiles tonales de Krumhansl-Schmuckler:
El algoritmo de Krumhansl-Schmuckler: Es un método computacional 
para detectar la tonalidad de una pieza musical. 
Cuenta la duración o la frecuencia de aparición de cada clase 
de altura en un fragmento para crear un histograma o cromagrama. 
Luego calcula la correlación de Pearson entre ese vector de notas y
cada uno de los 24 perfiles tonales. La tonalidad cuyo perfil dé la 
correlación más alta es la elegida como la tonalidad estimada de la obra. (UNAL)*/
static const double PERFIL_MAYOR[12] = {
    6.35, 2.23, 3.48, 2.33, 4.38, 4.09,
    2.52, 5.19, 2.39, 3.66, 2.29, 2.88
};

static const double PERFIL_MENOR[12] = {
    6.33, 2.68, 3.52, 5.38, 2.60, 3.53,
    2.54, 4.75, 3.98, 2.69, 3.34, 3.17
};

// Correlación de Pearson entre dos perfiles.
static double correlacion(
    const double a[12],
    const double b[12]
) {
    double mediaA = 0.0;
    double mediaB = 0.0;

    for (int i = 0; i < 12; i++) {
        mediaA += a[i];
        mediaB += b[i];
    }

    mediaA /= 12.0;
    mediaB /= 12.0;

    double numerador = 0.0;
    double sumaA = 0.0;
    double sumaB = 0.0;

    for (int i = 0; i < 12; i++) {
        double da = a[i] - mediaA;
        double db = b[i] - mediaB;

        numerador += da * db;
        sumaA += da * da;
        sumaB += db * db;
    }

    double denominador = std::sqrt(sumaA * sumaB);

    if (denominador == 0.0) {
        return 0.0;
    }

    return numerador / denominador;
}

// Estima la tonalidad general de una secuencia.
Tonalidad detectarTonalidad(
    const std::vector<EventoArmonico>& eventos
) {
    Tonalidad resultado;

    double histograma[12] = {};

    for (const auto& evento : eventos) {
        for (const Nota& nota : evento.notas) {
            if (nota.pitch < 0) {
                continue;
            }

            int pc = pitchClass(nota.pitch);

            double peso = std::max(0.1, nota.duracion);

            if (nota.velocidad > 0) {
                peso *= nota.velocidad / 127.0;
            }

            histograma[pc] += peso;
        }
    }

    double mejorCorrelacion =
        -std::numeric_limits<double>::infinity();

    int mejorTonica = -1;
    bool mejorEsMayor = true;

    // Evaluar las 24 tonalidades posibles.
    for (int tonica = 0; tonica < 12; tonica++) {
        double perfilMayor[12];
        double perfilMenor[12];

        for (int i = 0; i < 12; i++) {
            perfilMayor[i] = PERFIL_MAYOR[
                pitchClass(i - tonica)
            ];

            perfilMenor[i] = PERFIL_MENOR[
                pitchClass(i - tonica)
            ];
        }

        double corrMayor = correlacion(
            histograma,
            perfilMayor
        );

        double corrMenor = correlacion(
            histograma,
            perfilMenor
        );

        if (corrMayor > mejorCorrelacion) {
            mejorCorrelacion = corrMayor;
            mejorTonica = tonica;
            mejorEsMayor = true;
        }

        if (corrMenor > mejorCorrelacion) {
            mejorCorrelacion = corrMenor;
            mejorTonica = tonica;
            mejorEsMayor = false;
        }
    }

    if (mejorTonica >= 0) {
        resultado.tonica_pitch_class = mejorTonica;
        resultado.mayor = mejorEsMayor;
        resultado.correlacion = mejorCorrelacion;

        resultado.nombre = nombreNota(mejorTonica);

        resultado.nombre += mejorEsMayor ? " mayor" : " menor";
    }

    return resultado;
}

//estos comentarios de completions de vs code me tienen aburrido
// Transpone una nota individual.
Nota transponerNota(
    const Nota& nota,
    int semitonos
) {
    Nota resultado = nota;

    if (resultado.pitch < 0) {
        return resultado;
    }

    resultado.pitch += semitonos;

    resultado.nota = nombreNota(resultado.pitch);
    resultado.octava = resultado.pitch / 12 - 1;

    return resultado;
}

// Transpone un evento armónico completo
EventoArmonico transponerAcorde(
    const EventoArmonico& evento,
    int semitonos
) {
    EventoArmonico resultado = evento;

    for (Nota& nota : resultado.notas) {
        nota = transponerNota(nota, semitonos);
    }

    if (resultado.root_pitch_class >= 0) {
        resultado.root_pitch_class =
            pitchClass(
                resultado.root_pitch_class + semitonos
            );
    }

    if (resultado.bass_pitch_class >= 0) {
        resultado.bass_pitch_class =
            pitchClass(
                resultado.bass_pitch_class + semitonos
            );
    }

    if (resultado.root_pitch_class >= 0 &&
        resultado.quality_index >= 0) {

        resultado.nombreAcorde =
            nombreNota(resultado.root_pitch_class) +
            obtenerSufijo(resultado.quality_index);

        if (resultado.bass_pitch_class >= 0 &&
            resultado.bass_pitch_class !=
            resultado.root_pitch_class) {

            resultado.nombreAcorde += "/";
            resultado.nombreAcorde +=
                nombreNota(resultado.bass_pitch_class);
        }
    }

    return resultado;
}
