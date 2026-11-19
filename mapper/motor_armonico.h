#ifndef MOTOR_ARMONICO_H
#define MOTOR_ARMONICO_H

#include "mapper.h"
#include "candidatos.h"
#include "viterbi.h"

#include <vector>
#include <algorithm>
#include <cstddef>
#include <cmath>

namespace MotorArmonico {

// Resuelve los voicings de los eventos armónicos del timeline.
// Cada evento genera una capa de candidatos.
// Viterbi selecciona la secuencia de voicings con menor costo global.
inline std::vector<ResultadoArmonico> mapear(
    const std::vector<Mapper::InstanteTimeline>& timeline,
    const ConfigMapper& config
) {
    std::vector<ResultadoArmonico> resultados;

    // Cada capa contiene los voicings candidatos de un evento.
    std::vector<std::vector<CandidatoVoicing>> capas;

    // Relaciona cada capa con su instante original del timeline.
    std::vector<std::size_t> indicesTimeline;

    // Conserva el evento armónico original de cada capa.
    std::vector<EventoArmonico> eventosOriginales;

    // --------------------------------------------------------
    // 1. Generación de candidatos armónicos
    // --------------------------------------------------------

    for (std::size_t i = 0; i < timeline.size(); ++i) {

        const auto& inst = timeline[i];

        // Sin evento armónico, no hay nada que procesar.
        if (!inst.acorde)
            continue;

        const auto& evento = *inst.acorde;
        if (evento.notas.size() < 2)
            continue;

// Procesar únicamente cuando el acorde comienza
// en el instante actual del timeline.
        if (std::abs(evento.timestamp - inst.inicio) >= 1e-4)
            continue;

        // Un evento con menos de dos notas no constituye
        // un voicing armónico para este motor.
        if (evento.notas.size() < 2)
            continue;

        auto candidatos = generarCandidatosArmonicos(
            evento,
            config
        );

        if (candidatos.empty())
            continue;

        capas.push_back(std::move(candidatos));

        indicesTimeline.push_back(i);

        eventosOriginales.push_back(evento);
    }

    // Si no hay eventos armónicos procesables,
    // no se ejecuta Viterbi.
    if (capas.empty())
        return resultados;

    // --------------------------------------------------------
    // 2. Resolución temporal mediante Viterbi
    // --------------------------------------------------------

    auto camino = resolverViterbiArmonico(
        capas,
        config
    );

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

        ResultadoArmonico resultado;

        resultado.indiceTimeline = indicesTimeline[i];

        resultado.eventoOriginal = eventosOriginales[i];

        resultado.candidato = capas[i][camino[i]];

        resultados.push_back(resultado);
    }

    return resultados;
}

} // namespace MotorArmonico

#endif // MOTOR_ARMONICO_H