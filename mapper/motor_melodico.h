
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

inline double calcularCostoArpegio(
    std::size_t indiceTimeline,
    const Nota& nota,
    const CandidatoMelodico& candidato,
    const std::vector<AnalizadorPatrones::PatronDetectado>& patrones,
    const ConfigMapper& config
) {
    double mejorPenalizacion =
        std::numeric_limits<double>::infinity();

    for (const auto& patron : patrones) {

        if (patron.confianza <= 0.0)
            continue;

        for (std::size_t i = 0;
             i < patron.indicesTimeline.size() &&
             i < patron.pitches.size();
             ++i) {

            if (patron.indicesTimeline[i] != indiceTimeline ||
                patron.pitches[i] != nota.pitch) {
                continue;
            }

            double mejorDistancia = 1.0;

            for (const auto& posicionForma : patron.posicionesForma) {

                if (posicionForma.pitch != nota.pitch)
                    continue;

                double distanciaTraste =
                    std::abs(
                        candidato.posicion.traste -
                        posicionForma.traste
                    ) / static_cast<double>(
                        std::max(1, config.traste_maximo)
                    );

                double distanciaCuerda =
                    std::abs(
                        candidato.posicion.cuerda -
                        posicionForma.cuerda
                    ) / 5.0;

                double distancia =
                    0.5 * distanciaTraste +
                    0.5 * distanciaCuerda;

                mejorDistancia = std::min(
                    mejorDistancia,
                    distancia
                );
            }

            double penalizacion =
                mejorDistancia *
                std::clamp(patron.confianza, 0.0, 1.0);

            mejorPenalizacion = std::min(
                mejorPenalizacion,
                penalizacion
            );
        }
    }

    if (!std::isfinite(mejorPenalizacion))
        return 0.0;

    return config.w_patron_arpegio * mejorPenalizacion;
}
// Resuelve la posición de las notas melódicas del timeline.
// Si coinciden varias notas melódicas en un instante,
// conserva la más aguda, manteniendo el comportamiento monofónico.


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

        // El motor sigue siendo monofónico:
        auto top = *std::max_element(
            notasMelodicas.begin(),
            notasMelodicas.end(),
            [](const Nota& a, const Nota& b) {
                return a.pitch < b.pitch;
            }
        );

        auto candidatos = generarCandidatosMelodicos(top, config);

        if (candidatos.empty())
            continue;

    // ----------------------------------------------------
    // Ajuste de costos según patrones de arpegio
    // ----------------------------------------------------

        for (auto& candidato : candidatos) {
            candidato.costo += calcularCostoArpegio(
                i,
                top,
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
        notasOriginales.push_back(top);
    }

    if (capas.empty())
        return resultados;

// --------------------------------------------------------
// 2. Resolución temporal mediante Viterbi
// --------------------------------------------------------

auto camino = resolverViterbiMelodico(capas, config);

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
