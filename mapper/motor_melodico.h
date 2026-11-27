/*
 * ARCHIVO: motor_armonico.h 
 * CONTEXTO DE LO QUE YA SE TIENE:
 * Ya se identificaron los eventos armonicos y acordes de la cancion, y se
 * cuenta con el generador de posturas (candidatos.h) y el optimizador de
 * secuencias (viterbi.h).
 *
 * HERRAMIENTAS A USAR:
 * "candidatos.h", "viterbi.h", "analizador_patrones.h" <vector> y <algorithm>.
 *
 * QUE DEBE HACER ESTE ARCHIVO:
 * Resolver la digitacion de la capa armonica (acordes y rasgueos): filtrar
 * los candidatos dando prioridad a formas de la biblioteca CAGED (acordes
 * abiertos y cejillas basicas) y correr Viterbi para elegir la secuencia de
 * posiciones con transiciones mas comodas entre un acorde y el siguiente.
 */

 //IMPORTANTE
 /* 
 Implementado de forma básica el reconocimiento de arpeggios (acordes que o se tocan al mismo
 tiempo sino nota por nota). no reconoce a lo largo de mucho tiempo asi que puede generar falsos positivos
 o volarse alguno. fue validado el funcionamiento ya
 
 */
//repito, de algo sirve haber estudiado musica

#ifndef MOTOR_MELODICO_H
#define MOTOR_MELODICO_H

#include "mapper.h"
#include "candidatos.h"
#include "viterbi.h"
#include "analizador_patrones.h"

#include <vector>
#include <algorithm>
#include <cstddef>
#include <limits>
#include <cmath>

namespace MotorMelodico {

// Ajusta el costo local de una posición según si encaja en un patrón de arpegio detectado
inline double calcularCostoArpegio(
    std::size_t indiceTimeline,
    const Nota& nota,
    const CandidatoMelodico& candidato,
    const std::vector<AnalizadorPatrones::PatronDetectado>& patrones,
    const ConfigMapper& config
) {
    (void)config;

    constexpr double PENALIZACION_BASE_FUERA_FORMA = 1.50;
    constexpr double BONO_BASE_EN_FORMA = 0.25;

    double mejorAjuste = 0.0;
    double mayorConfianza = 0.0;

    for (const auto& patron : patrones) {
        if (patron.confianza <= 0.0) continue;

        bool perteneceAlPatron = false;
        for (std::size_t k = 0;
             k < patron.indicesTimeline.size() && k < patron.pitches.size();
             ++k) {
            if (patron.indicesTimeline[k] == indiceTimeline &&
                patron.pitches[k] == nota.pitch) {
                perteneceAlPatron = true;
                break;
            }
        }

        if (!perteneceAlPatron) continue;

        bool coincidePosicionForma = false;
        for (const auto& posForma : patron.posicionesForma) {
            if (posForma.cuerda == candidato.posicion.cuerda &&
                posForma.traste == candidato.posicion.traste) {
                coincidePosicionForma = true;
                break;
            }
        }

        if (patron.confianza > mayorConfianza) {
            mayorConfianza = patron.confianza;
            if (coincidePosicionForma) {
                mejorAjuste = -BONO_BASE_EN_FORMA * patron.confianza;
            } else if (candidato.posicion.traste == 0 && patron.transposicion <= 4) {
                mejorAjuste = 0.0; // No prohibir cuerdas al aire en primera posición (evita D|5 en vez de G|0)
            } else {
                mejorAjuste = PENALIZACION_BASE_FUERA_FORMA * patron.confianza;
            }
        }
    }

    return mejorAjuste;
}
// Genera posiciones melódicas y usa Viterbi para elegir la secuencia más cómoda
inline std::vector<ResultadoMelodico> mapear(
    const std::vector<Mapper::InstanteTimeline>& timeline,
    const ConfigMapper& config,
    const std::vector<AnalizadorPatrones::PatronDetectado>& patrones
) {
    std::vector<ResultadoMelodico> resultados;

    // Cada capa contiene los candidatos de un instante procesable.
    std::vector<std::vector<CandidatoMelodico>> capas;
    
    // Relaciona cada capa con su instante original en el timeline.
    std::vector<std::size_t> indicesTimeline;

    // Conserva la nota original correspondiente a cada capa.
    std::vector<Nota> notasOriginales;
// --------------------------------------------------------
// 1. Generación de candidatos
// --------------------------------------------------------

    for (std::size_t i = 0; i < timeline.size(); ++i) {
        const auto& inst = timeline[i];

        auto notasMelodicas =
            Mapper::obtenerNotasMelodicasAtacadas(inst);

        if (notasMelodicas.empty())
            continue;

        // Ordenar de grave a agudo para procesar todas las voces del arpegio/pasaje
        std::sort(
            notasMelodicas.begin(),
            notasMelodicas.end(),
            [](const Nota& a, const Nota& b) {
                return a.pitch < b.pitch;
            }
        );

        for (const auto& notaMelodica : notasMelodicas) {
            auto candidatos = generarCandidatosMelodicos(notaMelodica, config);

            if (candidatos.empty())
                continue;

            for (auto& candidato : candidatos) {
                candidato.costo += calcularCostoArpegio(
                    i,
                    notaMelodica,
                    candidato,
                    patrones,
                    config
                );
            }

            std::sort(
                candidatos.begin(),
                candidatos.end(),
                [](const CandidatoMelodico& a,
                   const CandidatoMelodico& b) {
                    return a.costo < b.costo;
                }
            );

            capas.push_back(std::move(candidatos));
            indicesTimeline.push_back(i);
            notasOriginales.push_back(notaMelodica);
        }
    }

    if (capas.empty())
        return resultados;

// --------------------------------------------------------
// 2. Resolución temporal mediante Viterbi
// --------------------------------------------------------

auto camino = resolverViterbiMelodico(capas, notasOriginales, config);

    // --------------------------------------------------------
    // 3. Construcción de resultados
    // --------------------------------------------------------

    const std::size_t limite = std::min(
        camino.size(),
        capas.size()
    );

    for (std::size_t i = 0; i < limite; ++i) {

        // Protección ante índices inválidos.
        if (camino[i] >= capas[i].size())
            continue;

        ResultadoMelodico resultado;

        resultado.indiceTimeline = indicesTimeline[i];
        resultado.notaOriginal = notasOriginales[i];
        resultado.candidato = capas[i][camino[i]];

        resultados.push_back(resultado);
    }

    return resultados;
}

} // namespace MotorMelodico

#endif // MOTOR_MELODICO_H
