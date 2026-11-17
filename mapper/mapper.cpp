//MAPPER.CPP - mapea notas de una cancion a tablatura
//llama a el poco de headers que ya se hicieron
//1 semana para llegar por fin a este punto, pero ya se puede mapear una canción completa a tablatura


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
    ctx.config = ConfigMapper(); // Valores por defecto

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

    // 3. Agrupar y reconocer acordes
    ctx.acordes = agruparNotas(ctx.notas);
    reconocerAcordes(ctx.acordes);

    // 4. Resolver solapamientos de acordes
    // (Si un acorde empieza antes de que termine el anterior, truncamos el anterior)
    for (size_t i = 0; i + 1 < ctx.acordes.size(); ++i) {
        double finActual = ctx.acordes[i].timestamp + ctx.acordes[i].duracion;
        double inicioSiguiente = ctx.acordes[i + 1].timestamp;
        
        if (finActual > inicioSiguiente) {
            ctx.acordes[i].duracion = inicioSiguiente - ctx.acordes[i].timestamp;
        }
    }

    // 5. Detectar tonalidad
    ctx.tonalidad = detectarTonalidad(ctx.acordes);

    return ctx;
}

// ============================================================
// BLOQUE 2: CONSTRUCCIÓN DE LA TIMELINE
// ============================================================

std::vector<InstanteTimeline> construirTimeline(const SongContext& ctx) {
    std::vector<InstanteTimeline> timeline;

    // 1. Recolectar fronteras
    std::set<double> fronterasSet;
    for (const auto& n : ctx.notas) {
        fronterasSet.insert(n.timestamp);
        fronterasSet.insert(n.timestamp + n.duracion);
    }
    for (const auto& a : ctx.acordes) {
        fronterasSet.insert(a.timestamp);
        fronterasSet.insert(a.timestamp + a.duracion);
    }

    std::vector<double> t(fronterasSet.begin(), fronterasSet.end());
    if (t.size() < 2) return timeline;

    // 2. Iterar sobre los segmentos
    const double EPS = 1e-4;
    for (size_t i = 0; i + 1 < t.size(); ++i) {
        double inicio = t[i];
        double fin = t[i + 1];
        
        if (fin - inicio < EPS) continue;

        InstanteTimeline inst;
        inst.inicio = inicio;
        inst.fin = fin;

        // 3. Notas atacadas y sostenidas
        for (const auto& n : ctx.notas) {
            if (n.timestamp >= inicio && n.timestamp < fin) {
                inst.notasAtacadas.push_back(n);
            } else if (n.timestamp < inicio && (n.timestamp + n.duracion) > inicio) {
                inst.notasSostenidas.push_back(n);
            }
        }

        // 4. Acorde activo
        for (const auto& a : ctx.acordes) {
            if (a.timestamp <= inicio && (a.timestamp + a.duracion) > inicio) {
                inst.acorde = a;
                break;
            }
        }

        // 5. Asignar roles (función inline del header)
        asignarRoles(inst);

        if (!inst.notasAtacadas.empty() || !inst.notasSostenidas.empty()) {
            timeline.push_back(inst);
        }
    }

    return timeline;
}

// ============================================================
// BLOQUE 3: ORQUESTADOR Y POST-PROCESO
// ============================================================

ResultadoMapeo mapear(const std::vector<Nota>& notasRaw) {
    ResultadoMapeo res;

    // 1. Construir contexto y timeline
    SongContext ctx = construirContexto(notasRaw);
    std::vector<InstanteTimeline> timeline = construirTimeline(ctx);
    
    if (timeline.empty()) return res;

    // 2. Preparar secuencias para Viterbi
    std::vector<std::vector<CandidatoMelodico>> capasMel;
    std::vector<std::vector<CandidatoVoicing>> capasArm;
    std::vector<int> idxMel; // Mapea capa de Viterbi -> índice de timeline
    std::vector<int> idxArm;

    for (size_t i = 0; i < timeline.size(); ++i) {
        auto& inst = timeline[i];

        // --- Melodía ---
        auto melNotas = obtenerNotasMelodicasAtacadas(inst);
        if (!melNotas.empty()) {
            // MVP: Asumimos melodía monofónica, tomamos la nota más aguda
            auto top = *std::max_element(melNotas.begin(), melNotas.end(),
                [](const Nota& a, const Nota& b) { return a.pitch < b.pitch; });
            
            auto cands = generarCandidatosMelodicos(top, ctx.config);
            if (!cands.empty()) {
                capasMel.push_back(cands);
                idxMel.push_back(static_cast<int>(i));
            }
        }

        // --- Armonía ---
        auto armNotas = obtenerNotasArmonicasAtacadas(inst);
        if (!armNotas.empty()) {
            EventoArmonico ev = construirEventoDeNotas(armNotas, inst.acorde);
            auto cands = generarCandidatosArmonicos(ev, ctx.config);
            if (!cands.empty()) {
                capasArm.push_back(cands);
                idxArm.push_back(static_cast<int>(i));
            }
        }
    }

    // 3. Correr Viterbi
    auto pathMel = resolverViterbiMelodico(capasMel, ctx.config);
    auto pathArm = resolverViterbiArmonico(capasArm, ctx.config);

    // 4. Mapear resultados de Viterbi por instante
    std::map<int, CandidatoMelodico> melElegida;
    size_t limiteMel = std::min(pathMel.size(), capasMel.size());
    for (size_t i = 0; i < limiteMel; ++i) {
        melElegida[idxMel[i]] = capasMel[i][pathMel[i]];
}

    std::map<int, CandidatoVoicing> armElegida;
    size_t limiteArm = std::min(pathArm.size(), capasArm.size());
    for (size_t i = 0; i < limiteArm; ++i) {
        armElegida[idxArm[i]] = capasArm[i][pathArm[i]];
    }

    // 5. Post-proceso: Fusión y colisiones
    // La clave es (pitch, timestamp_inicio) para no pisar notas repetidas
    std::map<std::pair<int, double>, PosicionGuitarra> sostenidasPos; 

        for (size_t i = 0; i < timeline.size(); ++i) {
        const auto& inst = timeline[i];
        
        TabEvent ev;
        ev.timestamp = inst.inicio;
        ev.duracion = inst.fin - inst.inicio;
        ev.acorde = inst.acorde;

        std::set<int> cuerdasUsadas;

        // A. Procesar Sostenidas (ya tienen posición asignada)
        for (const auto& n : inst.notasSostenidas) {
            auto key = std::make_pair(n.pitch, n.timestamp);
            
            if (sostenidasPos.count(key)) {
                PosicionGuitarra p = sostenidasPos[key];
                TabNote tn;
                tn.pitch = p.pitch;
                tn.timestamp = inst.inicio;
                tn.duracion = ev.duracion;
                tn.cuerda = p.cuerda;
                tn.traste = p.traste;
                tn.role = n.role;
                
                ev.notas.push_back(tn);
                cuerdasUsadas.insert(p.cuerda);
            } else {
                // Fallback: si no está en el mapa (por saltos o huecos), 
                // le asignamos una posición por defecto para que no desaparezca.
                auto posiciones = detalleCandidatos::posicionesParaPitch(n.pitch, ctx.config);
                if (!posiciones.empty()) {
                    PosicionGuitarra p = posiciones[0];
                    TabNote tn;
                    tn.pitch = p.pitch;
                    tn.timestamp = inst.inicio;
                    tn.duracion = ev.duracion;
                    tn.cuerda = p.cuerda;
                    tn.traste = p.traste;
                    tn.role = n.role;
                    
                    ev.notas.push_back(tn);
                    cuerdasUsadas.insert(p.cuerda);
                    sostenidasPos[key] = p; // Corregir el mapa para futuros instantes
                }
            }
        }

        // B. Procesar Melodía (Ley absoluta, gana colisiones)
        if (melElegida.count(static_cast<int>(i))) {
            const auto& cand = melElegida[static_cast<int>(i)];
            PosicionGuitarra p = cand.posicion;
            
            if (!cuerdasUsadas.count(p.cuerda)) {
                TabNote tn;
                tn.pitch = p.pitch;
                tn.timestamp = inst.inicio;
                tn.duracion = ev.duracion;
                tn.cuerda = p.cuerda;
                tn.traste = p.traste;
                tn.role = Role::MELODIA;
                
                ev.notas.push_back(tn);
                cuerdasUsadas.insert(p.cuerda);
                // Guardar usando la clave compuesta: (pitch, timestamp de ataque)
                sostenidasPos[{p.pitch, inst.inicio}] = p; 
            }
        }

        // C. Procesar Armonía (Cede ante colisiones)
        if (armElegida.count(static_cast<int>(i))) {
            const auto& cand = armElegida[static_cast<int>(i)];
            
            for (const auto& p : cand.posiciones) {
                if (cuerdasUsadas.count(p.cuerda)) {
                    ev.notasOmitidas.push_back(p.pitch);
                } else {
                    TabNote tn;
                    tn.pitch = p.pitch;
                    tn.timestamp = inst.inicio;
                    tn.duracion = ev.duracion;
                    tn.cuerda = p.cuerda;
                    tn.traste = p.traste;
                    tn.role = Role::ACORDE;
                    
                    ev.notas.push_back(tn);
                    cuerdasUsadas.insert(p.cuerda);
                    // Guardar usando la clave compuesta
                    sostenidasPos[{p.pitch, inst.inicio}] = p; 
                }
            }
        }

        // Guardar evento si tiene contenido
        if (!ev.notas.empty()) {
            for (const auto& tn : ev.notas) {
                res.notas.push_back(tn);
            }
            res.eventos.push_back(ev);
        }
    }

    return res;
}

} // namespace Mapper