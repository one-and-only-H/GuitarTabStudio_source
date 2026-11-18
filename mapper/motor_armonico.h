
#ifndef MOTOR_ARMONICO_H
#define MOTOR_ARMONICO_H

#include "mapper.h"
#include "candidatos.h"
#include "viterbi.h"

#include <vector>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

namespace MotorArmonico {

// Resultado de resolver un evento armónico.
// indiceTimeline permite al merger localizar el evento original.
struct ResultadoArmonico {
    std::size_t indiceTimeline = 0;
    EventoArmonico eventoOriginal;
    CandidatoVoicing candidato;
};


// Resuelve los eventos armónicos del timeline.
//
// Solo procesa eventos cuando comienzan, evitando introducir
// repetidamente el mismo acorde mientras permanece activo.
//
// Cada evento genera una capa de candidatos. Posteriormente,
// Viterbi selecciona la secuencia de voicings óptima.
inline std::vector<ResultadoArmonico> mapear(
    const std::vector<Mapper::InstanteTimeline>& timeline,
    const ConfigMapper& config
) {
    std::vector<ResultadoArmonico> resultados;

    std::vector<std::vector<CandidatoVoicing>> capas;
    std::vector<std::size_t> indicesTimeline;
    std::vector<EventoArmonico> eventosOriginales;

    // --------------------------------------------------------
    // 1. Generar candidatos para cada evento armónico
    // --------------------------------------------------------

    for (std::size_t i = 0; i < timeline.size(); ++i) {

        const auto& inst = timeline[i];

        if (!inst.acorde.has_value())
            continue;

        const auto& evento = *inst.acorde;

        // Procesar únicamente el instante de inicio del evento.
        if (std::abs(evento.timestamp - inst.inicio) >= 1e-4)
            continue;

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

    // No hay eventos armónicos resolubles.
    if (capas.empty())
        return resultados;

    // --------------------------------------------------------
    // 2. Resolver la secuencia mediante Viterbi
    // --------------------------------------------------------

    auto camino = resolverViterbiArmonico(capas, config);

    // --------------------------------------------------------
    // 3. Reconstruir los resultados
    // --------------------------------------------------------

    const std::size_t limite = std::min(
        camino.size(),
        capas.size()
    );

    for (std::size_t i = 0; i < limite; ++i) {

        if (camino[i] >= capas[i].size())
            continue;

        ResultadoArmonico resultado;

        resultado.indiceTimeline = indicesTimeline[i];
        resultado.eventoOriginal = eventosOriginales[i];
        resultado.candidato = capas[i][camino[i]];

        resultados.push_back(std::move(resultado));
    }

    return resultados;
}

} // namespace MotorArmonico

#endif // MOTOR_ARMONICO_H
