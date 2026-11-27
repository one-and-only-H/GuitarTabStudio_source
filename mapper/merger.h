
#ifndef MERGER_H
#define MERGER_H

#include "mapper.h"

#include <vector>
#include <map>
#include <algorithm>
#include <cstddef>

namespace Merger {

// ============================================================
// 1. UTILIDADES INTERNAS
// ============================================================

// Comprueba si dos notas ocupan exactamente la misma posición.
// Compara la cuerda y el traste para saber si dos notas están en el mismo sitio
inline bool mismaPosicion(
    const Mapper::TabNote& a,
    const Mapper::TabNote& b
) {
    return a.cuerda == b.cuerda &&
           a.traste == b.traste;
}

// Inserta una nota si su posición todavía no está ocupada
// dentro del evento.
// Añade una nota al evento solo cuando no existe otra en su misma cuerda y traste
inline void insertarNota(
    Mapper::TabEvent& evento,
    const Mapper::TabNote& nota
) {
    auto existente = std::find_if(
        evento.notas.begin(),
        evento.notas.end(),
        [&](const Mapper::TabNote& actual) {
            return mismaPosicion(actual, nota);
        }
    );

    if (existente == evento.notas.end()) {
        evento.notas.push_back(nota);
    }
}

// Actualiza el intervalo temporal del evento según sus notas.
// Amplía el inicio y el fin del evento para cubrir el intervalo de la nota recibida
inline void actualizarTiempo(
    Mapper::TabEvent& evento,
    const Mapper::TabNote& nota
) {
    if (evento.notas.empty()) {
        evento.timestamp = nota.timestamp;
        evento.duracion = nota.duracion;
        return;
    }

    const double inicio = std::min(
        evento.timestamp,
        nota.timestamp
    );

    const double fin = std::max(
        evento.timestamp + evento.duracion,
        nota.timestamp + nota.duracion
    );

    evento.timestamp = inicio;
    evento.duracion = fin - inicio;
}

// Busca el rol original de una nota dentro del evento armónico.
// Recupera el rol guardado del pitch; si no aparece, lo marca como parte del acorde
inline Role obtenerRol(
    const EventoArmonico& evento,
    int pitch
) {
    for (const auto& nota : evento.notas) {
        if (nota.pitch == pitch)
            return nota.role;
    }

    return Role::ACORDE;
}

// Comprueba si un pitch aparece en un voicing.
// Revisa todas las posiciones del candidato para detectar si contiene el pitch indicado
inline bool contienePitch(
    const CandidatoVoicing& candidato,
    int pitch
) {
    return std::any_of(
        candidato.posiciones.begin(),
        candidato.posiciones.end(),
        [&](const PosicionGuitarra& posicion) {
            return posicion.pitch == pitch;
        }
    );
}

// ============================================================
// 2. CONVERSIÓN DE RESULTADOS
// ============================================================

// Convierte un resultado melódico a TabNote.
// Copia la nota original y su posición elegida al formato usado por la tablatura
inline Mapper::TabNote convertirMelodico(
    const ResultadoMelodico& resultado
) {
    Mapper::TabNote nota;

    nota.pitch = resultado.notaOriginal.pitch;
    nota.timestamp = resultado.notaOriginal.timestamp;
    nota.duracion = resultado.notaOriginal.duracion;

    nota.cuerda = resultado.candidato.posicion.cuerda;
    nota.traste = resultado.candidato.posicion.traste;

    nota.role = resultado.notaOriginal.role;
    nota.tech = resultado.notaOriginal.tech;

    return nota;
}

// Convierte una posición armónica a TabNote.
// Convierte una posición de un voicing armónico en una TabNote conservando su rol
inline Mapper::TabNote convertirArmonico(
    const ResultadoArmonico& resultado,
    const PosicionGuitarra& posicion
) {
    Mapper::TabNote nota;

    nota.pitch = posicion.pitch;
    nota.timestamp = resultado.eventoOriginal.timestamp;
    nota.duracion = resultado.eventoOriginal.duracion;

    nota.cuerda = posicion.cuerda;
    nota.traste = posicion.traste;

    nota.role = obtenerRol(
        resultado.eventoOriginal,
        posicion.pitch
    );

    nota.tech = Technique::None;

    return nota;
}

// ============================================================
// 3. FUSIÓN PRINCIPAL
// ============================================================

// Junta las notas melódicas y armónicas en eventos ordenados y evita posiciones duplicadas
inline Mapper::ResultadoMapeo fusionar(
    const std::vector<Mapper::InstanteTimeline>& timeline,
    const std::vector<ResultadoMelodico>& melodicos,
    const std::vector<ResultadoArmonico>& armonicos,
    const std::vector<Warning>& warnings = {}
) {
    Mapper::ResultadoMapeo resultado;
    resultado.warnings = warnings;

    // Cada índice del timeline tendrá como máximo un TabEvent.
    std::map<std::size_t, Mapper::TabEvent> eventos;

    // --------------------------------------------------------
    // 3.1 Incorporar resultados melódicos
    // --------------------------------------------------------

    for (const auto& melodico : melodicos) {
        if (melodico.indiceTimeline >= timeline.size())
            continue;

        auto& evento = eventos[melodico.indiceTimeline];

        auto nota = convertirMelodico(melodico);

        actualizarTiempo(evento, nota);
        insertarNota(evento, nota);
    }

    // --------------------------------------------------------
    // 3.2 Incorporar resultados armónicos
    // --------------------------------------------------------

    for (const auto& armonico : armonicos) {
        if (armonico.indiceTimeline >= timeline.size())
            continue;

        auto& evento = eventos[armonico.indiceTimeline];

        evento.acorde = armonico.eventoOriginal;

        for (const auto& posicion : armonico.candidato.posiciones) {
            auto nota = convertirArmonico(
                armonico,
                posicion
            );

            actualizarTiempo(evento, nota);
            insertarNota(evento, nota);
        }

        // Registrar pitches originales que no fueron incluidos
        // en el voicing seleccionado.
        for (const auto& notaOriginal : armonico.eventoOriginal.notas) {
            if (!contienePitch(
                armonico.candidato,
                notaOriginal.pitch
            )) {
                if (std::find(
                    evento.notasOmitidas.begin(),
                    evento.notasOmitidas.end(),
                    notaOriginal.pitch
                ) == evento.notasOmitidas.end()) {
                    evento.notasOmitidas.push_back(
                        notaOriginal.pitch
                    );
                }
            }
        }
    }

    // --------------------------------------------------------
    // 3.3 Construir salida final
    // --------------------------------------------------------

    for (auto& par : eventos) {
        auto& evento = par.second;

        // Ordenar notas por cuerda.
        std::sort(
            evento.notas.begin(),
            evento.notas.end(),
            [](const Mapper::TabNote& a, const Mapper::TabNote& b) {
                return a.cuerda < b.cuerda;
            }
        );

        // Añadir las notas al resultado global.
        for (const auto& nota : evento.notas) {
            resultado.notas.push_back(nota);
        }

        resultado.eventos.push_back(std::move(evento));
    }

    // Orden cronológico global.
    std::sort(
        resultado.notas.begin(),
        resultado.notas.end(),
        [](const Mapper::TabNote& a, const Mapper::TabNote& b) {
            return a.timestamp < b.timestamp;
        }
    );

    return resultado;
}

} // namespace Merger

#endif // MERGER_H
