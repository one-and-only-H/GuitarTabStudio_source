/*
 * ARCHIVO: mapper/mapper.h / mapper/mapper.cpp
 *
 * CONTEXTO DE LO QUE YA SE TIENE:
 * Ya estan implementados y probados por separado los modulos de analisis
 * armonico (acordes), generacion de posturas (candidatos) y optimizacion de
 * trayectoria (viterbi).
 *
 * HERRAMIENTAS A USAR:
 * "tipos.h", "tipos_mapper.h", "acordes.h", "candidatos.h", "viterbi.h",
 * <vector> y <algorithm>.
 *
 * QUE DEBE HACER ESTE ARCHIVO:
 * Orquestar todo el proceso de digitacion en cuatro fases: construir el
 * contexto armonico de la pista, dividir la linea de tiempo en notas
 * atacadas y sostenidas, ejecutar Viterbi para melodia y armonia, y fusionar
 * ambas capas resolviendo colisiones cuando dos notas piden la misma cuerda.
 */
#include "mapper.h"
#include "candidatos.h"
#include "viterbi.h"
#include "tipos_contexto.h"
#include "tipos_mapper.h"
#include "../acordes.h"
#include "../tipos.h"
#include "motor_melodico.h"
#include "motor_armonico.h"
#include "merger.h"

#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace Mapper {

// ============================================================
// BLOQUE 1: CONSTRUCCIÓN DEL CONTEXTO
// ============================================================
SongContext construirContexto(const std::vector<Nota>& notasRaw) {
    SongContext ctx;
    ctx.config = ConfigMapper();

    // 1. Filtrar y copiar notas válidas
    for (const auto& n : notasRaw) {
        if (n.pitch >= 0 && n.duracion > 0.0) {
            ctx.notas.push_back(n);
        }
    }

    // 2. Ordenar por timestamp
    std::sort(ctx.notas.begin(), ctx.notas.end(),
        [](const Nota& a, const Nota& b) {
            return a.timestamp < b.timestamp;
        });

    // 3. Agrupar en eventos (solo para la UI / tonalidad)
    // Usamos una tolerancia muy baja para no mezclar notas de riffs.
    ctx.acordes = agruparNotas(ctx.notas, 0.005);
    reconocerAcordes(ctx.acordes);

    // 4. Detectar tonalidad (ignoramos los acordes, usamos las notas)
    ctx.tonalidad = detectarTonalidad(ctx.acordes);

    return ctx;
}

// ============================================================
// BLOQUE 2: CONSTRUCCIÓN DE LA TIMELINE (NOTA POR NOTA)
// ============================================================
std::vector<InstanteTimeline> construirTimeline(const SongContext& ctx) {
    std::vector<InstanteTimeline> timeline;
    if (ctx.notas.empty()) return timeline;

    // Creamos un instante por cada nota única en el tiempo
    std::vector<double> tiempos;
    for (const auto& n : ctx.notas) tiempos.push_back(n.timestamp);
    std::sort(tiempos.begin(), tiempos.end());
    tiempos.erase(std::unique(tiempos.begin(), tiempos.end(), 
        [](double a, double b) { return std::abs(a - b) < 1e-4; }), tiempos.end());

    for (size_t i = 0; i < tiempos.size(); ++i) {
        InstanteTimeline inst;
        inst.inicio = tiempos[i];
        

        // Buscar notas que atacan exactamente en este instante
        for (const auto& n : ctx.notas) {
            if (std::abs(n.timestamp - inst.inicio) < 1e-4) {
                inst.notasAtacadas.push_back(n);
            }
        }
         if (i + 1 < tiempos.size()) {
            inst.fin = tiempos[i + 1];
        } else {
            // Último instante: usar la duración máxima de sus notas
            double maxDur = 0.0;
            for (const auto& n : inst.notasAtacadas) {
                maxDur = std::max(maxDur, n.duracion);
            }
            inst.fin = inst.inicio + maxDur; // Si maxDur es 0, será inst.inicio
        }

        // Buscar acorde que comienza en este instante
        for (const auto& a : ctx.acordes) {
            if (a.notas.size() >= 2 && std::abs(a.timestamp - inst.inicio) < 1e-4) {
                inst.acorde = a;
                break;
            }
        }

        // Asignar roles
        asignarRoles(inst);

        if (!inst.notasAtacadas.empty()) {
            timeline.push_back(inst);
        }
    }

    return timeline;
}


// ============================================================
// BLOQUE 3: ORQUESTADOR
// ============================================================

ResultadoMapeo mapear(const std::vector<Nota>& notasRaw) {

    // 1. Construir contexto global.
    SongContext ctx = construirContexto(notasRaw);

    // 2. Construir timeline.
    std::vector<InstanteTimeline> timeline =
        construirTimeline(ctx);
    std::size_t cntMelodia = 0, cntAcorde = 0, cntBajo = 0;
    for (const auto& inst : timeline) {
        for (const auto& n : inst.notasAtacadas) {
            if (n.role == Role::MELODIA) ++cntMelodia;
            else if (n.role == Role::ACORDE) ++cntAcorde;
            else if (n.role == Role::BAJO) ++cntBajo;
        }
    }
    std::cout << "[DEBUG ROLES] MELODIA: " << cntMelodia
              << " | ACORDE: " << cntAcorde
              << " | BAJO: " << cntBajo << '\n';
    // 3. Ejecutar motor melódico.
    auto patronesArpegio = AnalizadorPatrones::detectar(
        timeline,
        ctx.config
    );

    auto resultadosMelodicos = MotorMelodico::mapear(
                                                    timeline,
                                                    ctx.config,
                                                    patronesArpegio
                                                    );

    // 4. Ejecutar motor armónico.
    auto resultadosArmonicos =
        MotorArmonico::mapear(timeline, ctx.config);

    // 5. Fusionar resultados.
    ResultadoMapeo resultado = Merger::fusionar(
        timeline,
        resultadosMelodicos,
        resultadosArmonicos,
        ctx.warnings
    );

    // ========================================================
    // DIAGNÓSTICO TEMPORAL
    // ========================================================

    std::cout << "\n=== MAPPER ===\n";

    std::cout << "Eventos en timeline: "
              << timeline.size() << '\n';

    std::cout << "Resultados melodicos: "
              << resultadosMelodicos.size() << '\n';

    std::cout << "Resultados armonicos: "
              << resultadosArmonicos.size() << '\n';

    std::cout << "Eventos finales: "
              << resultado.eventos.size() << '\n';

    std::cout << "Notas finales: "
              << resultado.notas.size() << '\n';

    for (const auto& r : resultadosArmonicos) {
        std::cout << "Evento armonico en t="
                  << r.eventoOriginal.timestamp
                  << " | Notas: "
                  << r.eventoOriginal.notas.size()
                  << " | Voicing: ";

        for (const auto& p : r.candidato.posiciones) {
            std::cout << "[C" << p.cuerda
                      << ", T" << p.traste
                      << ", P" << p.pitch << "] ";
        }
            // el indentado salva vidas, trust me
        std::cout << '\n';
    }

    return resultado;
}


} // namespace Mapper