/*
 * ARCHIVO: analizador_contextual.h / analizador_contextual.cpp
 *
 * CONTEXTO DE LO QUE YA SE TIENE:
 * Ya se leyeron las notas desde el CSV (csv_parser) y se agruparon los
 * eventos simultaneos basicos (acordes), pero falta interpretar el contexto
 * musical antes de asignar trastes.
 *
 * HERRAMIENTAS A USAR:
 * "tipos.h", <vector>, <algorithm> y <cmath>.
 *  * tambien su propio tipos_contexto
 * QUE DEBE HACER ESTE ARCHIVO:
 * Analizar el contexto global y local de la pista: estimar la tonalidad,
 * detectar cambios de zona en el mastil (grave vs agudo), distinguir que
 * notas funcionan como acompanamiento frente a melodia principal y limpiar
 * solapamientos de duracion entre notas seguidas.
 * 
 * muy util para resolver el problema de markov (Resuelve cómo evolucionará un sistema aleatorio en el futuro y
 *                                                   en qué proporciones se estabilizará a largo plazo, 
 *                                                  basándose únicamente en su estado presente y en las 
 *                                                      probabilidades de cambio entre estados.)
 */
#ifndef ANALIZADOR_CONTEXTUAL_H
#define ANALIZADOR_CONTEXTUAL_H

#include "tipos_contexto.h"
#include "../tipos.h"

#include <vector>
#include <algorithm>
#include <cmath>

namespace AnalizadorContextual {

// Calcula el valor central de una lista de pitches y usa el valor de 60 si está vacía
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

// Divide el tiempo de la canción en bloques y calcula el registro medio de cada bloque
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