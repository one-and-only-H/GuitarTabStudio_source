#include "mapper.h"
#include "candidatos.h"
#include "viterbi.h"
#include "tipos_mapper.h"
#include "../acordes.h"
#include "../tipos.h"

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
    
    if (timeline.empty()) return res;

    // 1. Preparar capas para Viterbi Melódico
    std::vector<std::vector<CandidatoMelodico>> capasMel;
    std::vector<int> idxMel;

    for (size_t i = 0; i < timeline.size(); ++i) {
        const auto& inst = timeline[i];
        
        // Si hay notas atacadas, generamos candidatos para la más aguda
        if (!inst.notasAtacadas.empty()) {
            // Tomamos la nota más aguda como la melodía principal
            auto top = *std::max_element(inst.notasAtacadas.begin(), inst.notasAtacadas.end(),
                [](const Nota& a, const Nota& b) { return a.pitch < b.pitch; });
            
            auto cands = generarCandidatosMelodicos(top, ctx.config);
            if (!cands.empty()) {
                capasMel.push_back(cands);
                idxMel.push_back(static_cast<int>(i));
            }
        }
    }

    // 2. Correr Viterbi Melódico
    auto pathMel = resolverViterbiMelodico(capasMel, ctx.config);

    // 3. Mapear resultados
    std::map<int, CandidatoMelodico> melElegida;
    size_t limiteMel = std::min(pathMel.size(), capasMel.size());
    for (size_t i = 0; i < limiteMel; ++i) {
        melElegida[idxMel[i]] = capasMel[i][pathMel[i]];
    }

    // 4. Post-proceso (construir tablatura)
    for (size_t i = 0; i < timeline.size(); ++i) {
        const auto& inst = timeline[i];
        
        TabEvent ev;
        ev.timestamp = inst.inicio;
        ev.duracion = inst.fin - inst.inicio;
        ev.acorde = inst.acorde;

        // Si Viterbi eligió una posición para este instante
        if (melElegida.count(static_cast<int>(i))) {
            const auto& cand = melElegida[static_cast<int>(i)];
            
            // Buscamos la nota original en el instante para recuperar su duración
            const Nota* notaOriginal = nullptr;
            auto top = *std::max_element(inst.notasAtacadas.begin(), inst.notasAtacadas.end(),
                [](const Nota& a, const Nota& b) { return a.pitch < b.pitch; });
            
            TabNote tn;
            tn.pitch = cand.posicion.pitch;
            tn.timestamp = inst.inicio;
            // AQUÍ: Usar la duración original del MIDI
            tn.duracion = top.duracion; 
            tn.cuerda = cand.posicion.cuerda;
            tn.traste = cand.posicion.traste;
            tn.role = Role::MELODIA;
            
            ev.notas.push_back(tn);
            res.notas.push_back(tn);
            res.eventos.push_back(ev);
        }
    }

    return res;
}

} // namespace Mapper