
#ifndef ANALIZADOR_PATRONES_H
#define ANALIZADOR_PATRONES_H

#include "mapper.h"
#include "formas_conocidas.h"

#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace AnalizadorPatrones {

// ============================================================
// TIPOS
// ============================================================

enum class DireccionPatron {
    ASCENDENTE,
    DESCENDENTE,
    MIXTO
};

struct ConfiguracionPatrones {
    std::size_t minimo_ataques = 3;
    std::size_t maximo_ataques_ventana = 12;

    double intervalo_maximo = 0.75;

    int cambios_direccion_maximos = 1;
};

struct PatronDetectado {
    std::string nombreForma;

    int root_pitch_class = -1;
    int quality_index = -1;

    int transposicion = 0;

    DireccionPatron direccion = DireccionPatron::MIXTO;

    double inicio = 0.0;
    double fin = 0.0;

    double confianza = 0.0;

    // Índices de los instantes del timeline que participan.
    std::vector<std::size_t> indicesTimeline;

    // Pitches MIDI detectados.
    std::vector<int> pitches;

    std::vector<PosicionGuitarra> posicionesForma;

};

struct NotaAtaque {
    int pitch = -1;
    double timestamp = 0.0;
    std::size_t indiceTimeline = 0;
};

// ============================================================
// UTILIDADES
// ============================================================

inline int pitchClass(int pitch) {
    int resultado = pitch % 12;

    if (resultado < 0)
        resultado += 12;

    return resultado;
}

inline int contarPitchesDistintos(
    const std::vector<int>& pitches
) {
    std::vector<int> distintos = pitches;

    std::sort(distintos.begin(), distintos.end());

    distintos.erase(
        std::unique(distintos.begin(), distintos.end()),
        distintos.end()
    );

    return static_cast<int>(distintos.size());
}

// ============================================================
// CONSTRUCCIÓN DE PLANTILLAS
// ============================================================

struct PlantillaArpegio {
    FormaConocida forma;

    int transposicion = 0;

    std::vector<int> pitches;
    std::vector<PosicionGuitarra> posiciones;
};

// Genera transposiciones físicamente válidas de una forma.
//
// Cada transposición desplaza todos los trastes activos
// conservando la geometría original.
//
// Ejemplo:
// Forma A_barra en traste 5.
// Transposición -3 -> forma equivalente en traste 2.
//
// Las cuerdas abiertas no pueden desplazarse a trastes
// negativos, por lo que limitan las transposiciones posibles.

inline std::vector<PlantillaArpegio> generarPlantillas(
    const ConfigMapper& config
) {
    std::vector<PlantillaArpegio> plantillas;

    const auto& formas = obtenerFormasConocidas();

    for (const auto& forma : formas) {

        if (forma.trastes.size() != config.afinacion.size())
            continue;

        int trasteMinimo = config.traste_maximo;
        int trasteMaximo = 0;

        bool tieneNotas = false;

        for (int traste : forma.trastes) {

            if (traste < 0)
                continue;

            tieneNotas = true;

            trasteMinimo = std::min(
                trasteMinimo,
                traste
            );

            trasteMaximo = std::max(
                trasteMaximo,
                traste
            );
        }

        if (!tieneNotas)
            continue;

        int deltaMinimo = -trasteMinimo;

        int deltaMaximo =
            config.traste_maximo - trasteMaximo;

        for (int delta = deltaMinimo; delta <= deltaMaximo; ++delta) {

            PlantillaArpegio plantilla;

            plantilla.forma = forma;
            plantilla.transposicion = delta;

            for (std::size_t cuerda = 0;
                cuerda < forma.trastes.size();
                ++cuerda) {

                int traste = forma.trastes[cuerda];

                if (traste < 0)
                    continue;

                traste += delta;

                int pitch = config.afinacion[cuerda] + traste;

                plantilla.pitches.push_back(pitch);

                PosicionGuitarra posicion;
                posicion.cuerda = static_cast<int>(cuerda) + 1;
                posicion.traste = traste;
                posicion.pitch = pitch;

                plantilla.posiciones.push_back(posicion);
            }

            if (plantilla.pitches.size() < 3)
                continue;

            // Validar ergonomía de cejilla implícita si la forma se desplazó hacia arriba
            if (delta > 0) {
                int minPisado = config.traste_maximo + 1;
                int maxPisado = 0;
                for (const auto& pos : plantilla.posiciones) {
                    if (pos.traste > 0) {
                        minPisado = std::min(minPisado, pos.traste);
                        maxPisado = std::max(maxPisado, pos.traste);
                    }
                }
                if (minPisado <= maxPisado) {
                    if ((maxPisado - minPisado) > config.max_hand_span)
                        continue;

                    int notasSobreCejilla = 0;
                    for (const auto& pos : plantilla.posiciones) {
                        if (pos.traste > minPisado)
                            ++notasSobreCejilla;
                    }
                    if (notasSobreCejilla > 3)
                        continue;
                }
            }

            plantillas.push_back(
                std::move(plantilla)
            );
        }
    }

    return plantillas;
}

// ============================================================
// DIRECCIÓN DEL PATRÓN
// ============================================================

inline DireccionPatron obtenerDireccion(
    const std::vector<int>& pitches
) {
    if (pitches.size() < 2)
        return DireccionPatron::MIXTO;

    bool ascendente = true;
    bool descendente = true;

    for (std::size_t i = 1; i < pitches.size(); ++i) {

        if (pitches[i] <= pitches[i - 1])
            ascendente = false;

        if (pitches[i] >= pitches[i - 1])
            descendente = false;
    }

    if (ascendente)
        return DireccionPatron::ASCENDENTE;

    if (descendente)
        return DireccionPatron::DESCENDENTE;

    return DireccionPatron::MIXTO;
}

inline int contarCambiosDireccion(
    const std::vector<int>& pitches
) {
    int cambios = 0;
    int direccionAnterior = 0;

    for (std::size_t i = 1; i < pitches.size(); ++i) {

        int diferencia = pitches[i] - pitches[i - 1];

        int direccionActual = 0;

        if (diferencia > 0)
            direccionActual = 1;

        else if (diferencia < 0)
            direccionActual = -1;

        if (direccionActual == 0)
            continue;

        if (direccionAnterior != 0 &&
            direccionActual != direccionAnterior) {

            ++cambios;
        }

        direccionAnterior = direccionActual;
    }

    return cambios;
}

// ============================================================
// COMPARACIÓN CON UNA FORMA
// ============================================================

inline bool coincideConPlantilla(
    const std::vector<int>& pitches,
    const PlantillaArpegio& plantilla
) {
    for (int pitch : pitches) {

        bool encontrado = false;

        for (int pitchForma : plantilla.pitches) {

            if (pitch == pitchForma) {
                encontrado = true;
                break;
            }
        }

        if (!encontrado)
            return false;
    }

    return true;
}

// ============================================================
// CONFIANZA
// ============================================================

inline double calcularConfianza(
    const std::vector<int>& pitches,
    const PlantillaArpegio& plantilla
) {
    int distintosPatron =
        contarPitchesDistintos(pitches);

    int distintosForma =
        contarPitchesDistintos(plantilla.pitches);

    if (distintosPatron < 3 ||
        distintosForma == 0) {

        return 0.0;
    }

    // Cobertura de las notas distintas de la plantilla.
    double cobertura =
        static_cast<double>(distintosPatron) /
        distintosForma;

    cobertura = std::min(cobertura, 1.0);

    // Penalización por patrones con demasiadas inversiones.
    int cambios = contarCambiosDireccion(pitches);

    double factorDireccion = 1.0;

    if (cambios > 1)
        factorDireccion = 0.70;

    else if (cambios == 1)
        factorDireccion = 0.90;

    return cobertura * factorDireccion;
}

inline bool sonEquivalentes(
    const PatronDetectado& a,
    const PatronDetectado& b
) {
    return
        a.indicesTimeline == b.indicesTimeline &&
        a.pitches == b.pitches;
}

// ============================================================
// DETECCIÓN
// ============================================================

inline std::vector<PatronDetectado> detectar(
    const std::vector<Mapper::InstanteTimeline>& timeline,
    const ConfigMapper& configMapper,
    const ConfiguracionPatrones& configPatrones = {}
) {
    std::vector<PatronDetectado> resultados;

    if (timeline.empty())
        return resultados;

    // --------------------------------------------------------
    // 1. Extraer ataques individuales
    // --------------------------------------------------------

    std::vector<NotaAtaque> ataques;

    for (std::size_t i = 0; i < timeline.size(); ++i) {

        for (const auto& nota : timeline[i].notasAtacadas) {

            if (nota.pitch < 0)
                continue;

            ataques.push_back({
            nota.pitch,
            nota.timestamp,
            i
        });
        }
    }

    if (ataques.size() < configPatrones.minimo_ataques)
        return resultados;

    // --------------------------------------------------------
    // 2. Generar formas transpuestas válidas
    // --------------------------------------------------------

    auto plantillas = generarPlantillas(configMapper);

    // --------------------------------------------------------
    // 3. Evaluar ventanas de ataques
    // --------------------------------------------------------

    for (const auto& plantilla : plantillas) {

        std::size_t maxNotas = std::min(
            configPatrones.maximo_ataques_ventana,
            ataques.size()
        );

        for (std::size_t longitud =
                 configPatrones.minimo_ataques;
             longitud <= maxNotas;
             ++longitud) {

            for (std::size_t inicio = 0;
                 inicio + longitud <= ataques.size();
                 ++inicio) {

                std::vector<int> pitches;
                std::vector<std::size_t> indices;

                bool intervaloValido = true;

                // --------------------------------------------
                // Construir ventana
                // --------------------------------------------

                for (std::size_t j = 0;
                     j < longitud;
                     ++j) {

                    const auto& ataque =
                        ataques[inicio + j];

                    pitches.push_back(
                        ataque.pitch
                    );

                    indices.push_back(
                        ataque.indiceTimeline
                    );

                    if (j > 0) {

                        double intervalo =
                            ataque.timestamp -
                            ataques[inicio + j - 1]
                                .timestamp;

                        if (ataque.indiceTimeline == ataques[inicio + j - 1].indiceTimeline ||
                            intervalo < 0.0 ||
                            intervalo >configPatrones.intervalo_maximo) {

                            intervaloValido = false;
                            break;
                        }
                    }
                }

                if (!intervaloValido)
                    continue;

                // --------------------------------------------
                // Requisitos musicales
                // --------------------------------------------

                if (contarPitchesDistintos(pitches) < 3)
                    continue;

                int cambios =
                    contarCambiosDireccion(pitches);

                if (cambios >
                    configPatrones.cambios_direccion_maximos) {

                    continue;
                }

                // --------------------------------------------
                // Compatibilidad con forma
                // --------------------------------------------

                if (!coincideConPlantilla(
                        pitches,
                        plantilla
                    )) {

                    continue;
                }

                // --------------------------------------------
                // Construir resultado
                // --------------------------------------------

                PatronDetectado patron;

                patron.nombreForma =
                    plantilla.forma.nombre;

                patron.root_pitch_class =
                    pitchClass(
                        plantilla.forma.root_pitch_class +
                        plantilla.transposicion
                    );

                patron.quality_index =
                    plantilla.forma.quality_index;

                patron.transposicion =
                    plantilla.transposicion;

                patron.direccion =
                    obtenerDireccion(pitches);

                patron.inicio =
                    ataques[inicio].timestamp;

                patron.fin =
                    ataques[inicio + longitud - 1]
                        .timestamp;

                patron.confianza =
                    calcularConfianza(
                        pitches,
                        plantilla
                    );

                patron.indicesTimeline =
                    std::move(indices);

                patron.pitches =
                    std::move(pitches);

                patron.posicionesForma = 
                    plantilla.posiciones;

                resultados.push_back(
                    std::move(patron)
                );
            }
        }
    }

    // --------------------------------------------------------
    // 4. Ordenar detecciones
    // --------------------------------------------------------

    std::sort(
        resultados.begin(),
        resultados.end(),
        [](const PatronDetectado& a,
           const PatronDetectado& b) {

            if (a.inicio != b.inicio)
                return a.inicio < b.inicio;

            if (a.confianza != b.confianza)
                return a.confianza > b.confianza;

            return a.pitches.size() > b.pitches.size();
        }
    );
    
    std::vector<PatronDetectado> unicos;
    for (auto& p : resultados) {
        bool duplicado = false;
        for (const auto& u : unicos) {
            if (sonEquivalentes(p, u)) {
                duplicado = true;
                break;
            }
        }
        if (!duplicado) {
            unicos.push_back(std::move(p));
        }
    }

    return unicos;
}

} // namespace AnalizadorPatrones

#endif // ANALIZADOR_PATRONES_H
