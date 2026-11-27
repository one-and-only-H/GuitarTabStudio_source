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

#ifndef MAPPER_H
#define MAPPER_H

#include "tipos_mapper.h"
#include "../acordes.h"
#include "tipos_contexto.h"


#include <vector>
#include <optional>
#include <string>
#include <set>

namespace Mapper {

// ============================================================
// 1. ESTRUCTURAS DEL PIPELINE
// ============================================================

// Contexto global de la canción.
struct SongContext {
    ConfigMapper config;
    
    std::vector<Nota> notas;
    std::vector<EventoArmonico> acordes;
    Tonalidad tonalidad;
    
    std::vector<Warning> warnings;
    ContextoTemporal contexto;
};

//comentarios a lo que de
// Segmento de tiempo donde el estado (notas/acordes) no cambia.
struct InstanteTimeline {
    double inicio = 0.0;
    double fin = 0.0;

    // Notas que atacan (empiezan) en este segmento.
    std::vector<Nota> notasAtacadas;

    // Notas que ya venían sonando (sostenidas).
    std::vector<Nota> notasSostenidas;

    // Acorde activo, si hay.
    std::optional<EventoArmonico> acorde;
};

// Entrada final para tab_generator.
struct TabNote {
    int pitch = -1;
    double timestamp = 0.0;
    double duracion = 0.0;
    int cuerda = 0;  // 1..6
    int traste = -1; // 0..traste_maximo
    Role role = Role::ACORDE;
    Technique tech = Technique::None;
};

struct TabEvent {
    double timestamp = 0.0;
    double duracion = 0.0;
    std::optional<EventoArmonico> acorde;
    std::vector<TabNote> notas;
    std::vector<int> notasOmitidas;
};

struct ResultadoMapeo {
    std::vector<TabNote> notas;
    std::vector<TabEvent> eventos;
    std::vector<Warning> warnings;
};

// ============================================================
// 2. UTILIDADES INLINE (Bajo nivel, testeables)
// ============================================================

// ------------------------------------------------------------
// 2.1 Roles
// ------------------------------------------------------------

// Asigna roles a las notas de un instante.
// La nota más grave del acorde se vuelve BAJO.
// Las que pertenecen al acorde son ACORDE.
// Las que no pertenecen son MELODIA.
// Revisa las notas del instante y etiqueta cada una según su relación con el acorde activo
inline void asignarRoles(InstanteTimeline& inst) {
    bool esAcordeValido =
        inst.acorde.has_value() &&
        inst.acorde->notas.size() >= 2 &&
        inst.acorde->root_pitch_class >= 0;

    if (!esAcordeValido) {
        for (auto& n : inst.notasAtacadas) n.role = Role::MELODIA;
        for (auto& n : inst.notasSostenidas) n.role = Role::MELODIA;
        return;
    }

    std::set<int> pcsAcorde;
    for (const auto& n : inst.acorde->notas) {
        if (n.pitch >= 0) pcsAcorde.insert(pitchClass(n.pitch));
    }

    auto procesarLista = [&](std::vector<Nota>& notas) {
        int masGrave = -1;
        for (size_t i = 0; i < notas.size(); ++i) {
            int pc = pitchClass(notas[i].pitch);
            if (pcsAcorde.count(pc)) {
                notas[i].role = Role::ACORDE;
                if (masGrave < 0 || notas[i].pitch < notas[masGrave].pitch) {
                    masGrave = static_cast<int>(i);
                }
            } else {
                notas[i].role = Role::MELODIA;
            }
        }
        if (masGrave >= 0) notas[masGrave].role = Role::BAJO;
    };

    procesarLista(inst.notasAtacadas);
    procesarLista(inst.notasSostenidas);
}

// ------------------------------------------------------------
// 2.2 Filtrado de notas
// ------------------------------------------------------------

// Dado un instante, devuelve las notas que deben pasarse
// por Viterbi melódico (las que son MELODIA y atacan).
// Filtra del instante las notas atacadas que fueron etiquetadas como melodía
inline std::vector<Nota> obtenerNotasMelodicasAtacadas(
    const InstanteTimeline& inst) {
    std::vector<Nota> res;
    for (const auto& n : inst.notasAtacadas) {
        if (n.role == Role::MELODIA) res.push_back(n);
    }
    return res;
}

// Dado un instante, devuelve las notas que deben pasarse
// por Viterbi armónico (las que no son MELODIA y atacan).
// Filtra del instante las notas atacadas que pertenecen al acorde o al bajo
inline std::vector<Nota> obtenerNotasArmonicasAtacadas(
    const InstanteTimeline& inst) {
    std::vector<Nota> res;
    for (const auto& n : inst.notasAtacadas) {
        if (n.role != Role::MELODIA) res.push_back(n);
    }
    return res;
}

// ------------------------------------------------------------
// 2.3 Construcción de EventoArmonico sintético
// ------------------------------------------------------------

// Para notas armónicas atacadas, construimos un evento
// que representa el "voicing libre" a optimizar.
// Crea un EventoArmonico con las notas dadas y conserva los datos del acorde original si existen
inline EventoArmonico construirEventoDeNotas(
    const std::vector<Nota>& notas,
    const std::optional<EventoArmonico>& acordeOriginal) {

    EventoArmonico ev;
    ev.notas = notas;
    ev.es_simultaneo = false;

    if (acordeOriginal.has_value()) {
        ev.root_pitch_class = acordeOriginal->root_pitch_class;
        ev.quality_index = acordeOriginal->quality_index;
        ev.bass_pitch_class = acordeOriginal->bass_pitch_class;
        ev.nombreAcorde = acordeOriginal->nombreAcorde;
    } else {
        // Voicing libre (cluster)
        ev.root_pitch_class = -1;
        ev.quality_index = -1;
        ev.bass_pitch_class = -1;
        ev.nombreAcorde = "";
    }

    return ev;
}

// ------------------------------------------------------------
// 2.4 Fusión y salida
// ------------------------------------------------------------

// Convierte un CandidatoVoicing a TabNotes.
// Convierte cada posición de una postura armónica en una nota preparada para la tablatura
inline std::vector<TabNote> convertirCandidatoATabNotes(
    const CandidatoVoicing& cand,
    double timestamp,
    double duracion) {
    
    std::vector<TabNote> res;
    for (const auto& p : cand.posiciones) {
        TabNote tn;
        tn.pitch = p.pitch;
        tn.timestamp = timestamp;
        tn.duracion = duracion;
        tn.cuerda = p.cuerda;
        tn.traste = p.traste;
        tn.role = Role::ACORDE; // se ajusta luego
        res.push_back(tn);
    }
    return res;
}
//ya se me pegó lo de la IA de poner los "//===="
// ============================================================
// 3. DECLARACIONES (lo que va en el .cpp)
// ============================================================

// Normaliza las notas, reconoce los acordes y prepara el contexto global para el proceso
SongContext construirContexto(const std::vector<Nota>& notasRaw);

// Divide la canción en instantes donde se buscan las notas atacadas, sostenidas y el acorde activo
std::vector<InstanteTimeline> construirTimeline(const SongContext& ctx);


// Ejecuta todo el flujo y devuelve la tablatura con las capas melódica y armónica fusionadas
ResultadoMapeo mapear(const std::vector<Nota>& notasRaw);

} // namespace Mapper

#endif // MAPPER_H