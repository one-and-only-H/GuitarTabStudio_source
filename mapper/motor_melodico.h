
#ifndef MOTOR_MELODICO_H
#define MOTOR_MELODICO_H

#include "mapper.h"
#include "candidatos.h"
#include "viterbi.h"

#include <vector>
#include <algorithm>
#include <cstddef>

namespace MotorMelodico {

// Resuelve la posición de las notas melódicas del timeline.
// Si coinciden varias notas melódicas en un instante,
// conserva la más aguda, manteniendo el comportamiento monofónico.
inline std::vector<ResultadoMelodico> mapear(
    const std::vector<Mapper::InstanteTimeline>& timeline,
    const ConfigMapper& config
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

        const auto& notasMelodicas = inst.notasAtacadas;

        if (notasMelodicas.empty())
            continue;

        // El motor sigue siendo monofónico:
        // selecciona la nota melódica más aguda.
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
