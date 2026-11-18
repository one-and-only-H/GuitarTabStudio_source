#include "mapper.h"
#include "candidatos.h"
#include "viterbi.h"
#include "tipos_mapper.h"
#include "../acordes.h"
#include "../tipos.h"
#include "motor_melodico.h"
#include "motor_armonico.h"

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

        // Buscar acorde activo (opcional, para mostrar el nombre)
        for (const auto& a : ctx.acordes) {
            if (a.timestamp <= inst.inicio && (a.timestamp + a.duracion) > inst.inicio) {
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
// BLOQUE 3: ORQUESTADOR (SIN ACORDES, SOLO NOTAS)
// ============================================================
ResultadoMapeo mapear(const std::vector<Nota>& notasRaw) {
    int notasDescartadas = 0;
    ResultadoMapeo res;

    SongContext ctx = construirContexto(notasRaw);
    std::vector<InstanteTimeline> timeline = construirTimeline(ctx);
    
    auto resultadosMelodicos =
    MotorMelodico::mapear(timeline, ctx.config);

    auto resultadosArmonicos =
        MotorArmonico::mapear(timeline, ctx.config);

// DIAGNÓSTICO TEMPORAL
    std::cout << "\n=== MOTOR ARMONICO ===\n";

    std::cout << "Eventos en timeline: "
              << timeline.size() << '\n';

    std::cout << "Resultados armonicos: "
              << resultadosArmonicos.size() << '\n';

    for (const auto& r : resultadosArmonicos) {
        std::cout << "Evento en t="
                  << r.eventoOriginal.timestamp
                  << " | Notas: "
                << r.eventoOriginal.notas.size()
                << " | Voicing: ";

        for (const auto& p : r.candidato.posiciones) {
            std::cout << "[C" << p.cuerda
                      << ", T" << p.traste
                      << ", P" << p.pitch << "] ";
        }

    std::cout << '\n';
}

std::map<int, ResultadoMelodico> melElegida;

for (const auto& resultado : resultadosMelodicos) {
    melElegida[static_cast<int>(resultado.indiceTimeline)] = resultado;
} //f1

    // 4. Post-proceso (construir tablatura)
    for (size_t i = 0; i < timeline.size(); ++i) {
        const auto& inst = timeline[i];
        
        TabEvent ev;
        ev.timestamp = inst.inicio;
        ev.duracion = inst.fin - inst.inicio;
        ev.acorde = inst.acorde;

            if (melElegida.count(static_cast<int>(i))) {
        const auto& resultado =
            melElegida.at(static_cast<int>(i));

        const auto& cand = resultado.candidato;
        const auto& top = resultado.notaOriginal;

        TabNote tn;
        tn.pitch = cand.posicion.pitch;
        tn.timestamp = inst.inicio;
        tn.duracion = top.duracion;
        tn.cuerda = cand.posicion.cuerda;
        tn.traste = cand.posicion.traste;
        tn.role = Role::MELODIA;

        ev.notas.push_back(tn);
        res.notas.push_back(tn);
        res.eventos.push_back(ev);
    }

} // Cierra el for

return res;

} // Cierra mapear()

} // namespace Mapper