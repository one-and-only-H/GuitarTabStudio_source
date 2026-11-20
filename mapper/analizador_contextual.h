
#ifndef ANALIZADOR_CONTEXTUAL_H
#define ANALIZADOR_CONTEXTUAL_H

#include "tipos_contexto.h"
#include "../tipos.h"

#include <vector>
#include <algorithm>
#include <cmath>

namespace AnalizadorContextual {

inline int calcularMediana(std::vector<int> pitches) {
    if (pitches.empty())
        return 60;

    std::sort(pitches.begin(), pitches.end());

    const std::size_t medio = pitches.size() / 2;

    if (pitches.size() % 2 != 0)
        return pitches[medio];

    return static_cast<int>(
        std::round(
            (pitches[medio - 1] + pitches[medio]) / 2.0
        )
    );
}

inline ContextoTemporal analizar(
    const std::vector<Nota>& notas,
    double duracionBloque = 20.0
) {
    ContextoTemporal contexto;

    if (notas.empty() || duracionBloque <= 0.0)
        return contexto;

    double inicioCancion = notas.front().timestamp;
    double finCancion = inicioCancion;

    for (const auto& nota : notas) {
        if (nota.pitch < 0 || nota.duracion <= 0.0)
            continue;

        finCancion = std::max(finCancion, nota.timestamp);
    }

    const std::size_t cantidadBloques =
        static_cast<std::size_t>(
            std::floor(
                (finCancion - inicioCancion) / duracionBloque
            )
        ) + 1;

    for (std::size_t i = 0; i < cantidadBloques; ++i) {
        PerfilContextual perfil;

        perfil.inicio = inicioCancion + i * duracionBloque;
        perfil.fin = perfil.inicio + duracionBloque;

        std::vector<int> pitches;

        for (const auto& nota : notas) {
            if (nota.pitch < 0 || nota.duracion <= 0.0)
                continue;

            if (nota.timestamp >= perfil.inicio &&
                nota.timestamp < perfil.fin) {
                pitches.push_back(nota.pitch);
            }
        }

        if (pitches.empty())
            continue;

        perfil.registroMediano = calcularMediana(pitches);

        contexto.perfiles.push_back(perfil);
    }

    return contexto;
}

} // namespace AnalizadorContextual

#endif