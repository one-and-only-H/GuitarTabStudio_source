// afinacion[cuerda - 1] devuelve el pitch MIDI de la cuerda al aire.
// cuerda 1 (aguda) → afinacion[0] = 64 (E4)
// cuerda 6 (grave) → afinacion[5] = 40 (E2)

#ifndef CANDIDATOS_H
#define CANDIDATOS_H

#include "tipos_mapper.h"
#include "formas_conocidas.h"
#include "../acordes.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <queue>
#include <string>
#include <set>
#include <vector>

// ============================================================
// FORMAS CONOCIDAS
// ============================================================

// Las cuerdas se almacenan de primera a sexta:
//
// -1 = cuerda muteada
//  0 = cuerda al aire
// >0 = traste pisado



// ============================================================
// BIBLIOTECA DE FORMAS
// ============================================================

// Contiene formas originales.
// Las transposiciones se generan durante la evaluación.



// ============================================================
// UTILIDADES
// ============================================================

// Devuelve el traste de una cuerda dentro de un voicing.
//
// -1 significa que la cuerda no participa en el voicing.

inline int trasteEnCuerda(
    const std::vector<PosicionGuitarra>& posiciones,
    int cuerda
) {
    for (const auto& posicion : posiciones) {
        if (posicion.cuerda == cuerda) {
            return posicion.traste;
        }
    }

    return -1;
}


// ============================================================
// COSTO MELÓDICO
// ============================================================

// Evalúa una posición individual.
//
// C_t: altura del traste.
// C_c: preferencia por la cuerda.
//
// La preferencia por cuerda tiene peso 0 por defecto.

inline double costoMelodico(
    const PosicionGuitarra& pos,
    const ConfigMapper& config
) {
    if (config.traste_maximo <= 0) {
        return 1.0;
    }

    if (pos.traste < 0 ||
        pos.traste > config.traste_maximo ||
        pos.cuerda < 1 ||
        pos.cuerda > 6) {
        return 1.0;
    }

    double C_t =
        static_cast<double>(pos.traste) /
        config.traste_maximo;

    double C_c =
        std::abs(pos.cuerda - 3.5) / 2.5;

    return config.w_t_melodico * C_t +
           config.w_c_melodico * C_c;
}


// ============================================================
// COSTO ARMÓNICO
// ============================================================

// Evalúa un voicing mediante:
//
// C_t: altura promedio de los trastes.
// C_e: extensión de la mano.
// C_d: distribución de las cuerdas.
// C_f: similitud con formas conocidas.
//
// Los cuatro componentes se combinan con los pesos
// definidos en ConfigMapper.

inline double costoArmonico(
    const std::vector<PosicionGuitarra>& posiciones,
    const ConfigMapper& config,
    int root_pitch_class = -1,
    int quality_index = -1,
    const std::vector<int>* pitchesPermitidos = nullptr
) {
    if (posiciones.empty() || config.traste_maximo <= 0) {
        return 1.0;
    }

    // --------------------------------------------------------
    // 1. Altura promedio de los trastes
    // --------------------------------------------------------
    double sumaTrastes = 0.0;
    int cuerdasAlAire = 0;
    int notasValidas = 0;

    int cuerdaMinima = 6;
    int cuerdaMaxima = 1;

    int trasteMinimo = config.traste_maximo;
    int trasteMaximo = 0;

    int notasPisadas = 0;

    for (const auto& posicion : posiciones) {
        if (pitchesPermitidos) {
            bool perteneceAlEvento = false;
            for (int p : *pitchesPermitidos) {
                if (pitchClass(posicion.pitch) == pitchClass(p)) {
                    perteneceAlEvento = true;
                    break;
                }
            }
            if (!perteneceAlEvento) {
                continue;
            }
        }
        
        notasValidas++;
        sumaTrastes += posicion.traste;

        cuerdaMinima = std::min(cuerdaMinima, posicion.cuerda);
        cuerdaMaxima = std::max(cuerdaMaxima, posicion.cuerda);

        if (posicion.traste == 0) {
            cuerdasAlAire++;
        }

        if (posicion.traste > 0) {
            trasteMinimo = std::min(trasteMinimo, posicion.traste);
            trasteMaximo = std::max(trasteMaximo, posicion.traste);
            notasPisadas++;
        }
    }
    
    if (notasValidas == 0) notasValidas = 1;

    double C_t = sumaTrastes / (notasValidas * config.traste_maximo);

    // --------------------------------------------------------
    // 2. Extensión de la mano (C_e)
    // --------------------------------------------------------
    double C_e = 0.0;

    if (notasPisadas >= 2) {
        int extension = trasteMaximo - trasteMinimo;
        C_e = std::min(
            1.0,
            static_cast<double>(std::max(0, extension - config.max_hand_span)) /
            std::max(1, config.max_hand_span)
        );
    }

    // --------------------------------------------------------
    // 3. Distribución de las cuerdas (C_d)
    // --------------------------------------------------------
    double C_d = static_cast<double>((cuerdaMaxima - cuerdaMinima + 1) - notasValidas) / 5.0;
    C_d = std::clamp(C_d, 0.0, 1.0);

    // --------------------------------------------------------
    // 4. Similitud con formas conocidas (C_f)
    // --------------------------------------------------------
    double C_f = 0.0;

    if (root_pitch_class >= 0 && root_pitch_class < 12 && quality_index >= 0) {
        auto similitudConForma = [&](const FormaConocida& forma) {
            int activasForma = 0;
            int activasCandidato = 0;
            int interseccion = 0;
            double sumaDiferencias = 0.0;

            for (int cuerda = 1; cuerda <= 6; cuerda++) {
                int trasteForma = forma.trastes[cuerda - 1];
                int trasteCandidato = trasteEnCuerda(posiciones, cuerda);

                bool activaForma = trasteForma >= 0;
                bool activaCandidato = trasteCandidato >= 0;

                if (activaForma) activasForma++;
                if (activaCandidato) activasCandidato++;

                if (activaForma && activaCandidato) {
                    interseccion++;
                    sumaDiferencias += std::abs(trasteForma - trasteCandidato);
                }
            }

            if (activasForma == 0) return 1.0;

            double distanciaTrastes = sumaDiferencias / (activasForma * config.traste_maximo);
            int diferenciaSimetrica = activasForma + activasCandidato - 2 * interseccion;
            double lambda = 0.30;
            double penalizacionCuerdas = lambda * diferenciaSimetrica / 6.0;

            return std::clamp(distanciaTrastes + penalizacionCuerdas, 0.0, 1.0);
        };

        double mejorSimilitud = 1.0;
        bool encontroForma = false;
        const auto& formas = obtenerFormasConocidas();

        for (const auto& forma : formas) {
            if (forma.quality_index != quality_index) continue;

            int desplazamiento = pitchClass(root_pitch_class - forma.root_pitch_class);

            for (int octava = -2; octava <= 2; octava++) {
                int transposicion = desplazamiento + 12 * octava;
                FormaConocida formaTranspuesta = forma;
                bool valida = true;

                for (int& traste : formaTranspuesta.trastes) {
                    if (traste < 0) continue;
                    traste += transposicion;
                    if (traste < 0 || traste > config.traste_maximo) {
                        valida = false;
                        break;
                    }
                }

                if (!valida) continue;

                double similitud = similitudConForma(formaTranspuesta);
                mejorSimilitud = std::min(mejorSimilitud, similitud);
                encontroForma = true;
            }
        }

        if (encontroForma) {
            C_f = mejorSimilitud;
        }
    }

    // --------------------------------------------------------
    // 5. Penalizar saltos entre cuerdas adyacentes (C_s)
    // --------------------------------------------------------
    double C_s = 0.0;
    
    // Ordenar posiciones por cuerda para comparar adyacentes
    std::vector<PosicionGuitarra> ordenadas = posiciones;
    std::sort(ordenadas.begin(), ordenadas.end(),
        [](const PosicionGuitarra& a, const PosicionGuitarra& b) {
            return a.cuerda < b.cuerda;
        });
    
    int saltosPenalizados = 0;
    int paresAdyacentes = 0;
    
    for (size_t i = 1; i < ordenadas.size(); i++) {
        if (ordenadas[i].cuerda - ordenadas[i-1].cuerda == 1) {
            
            // Si alguna de las dos notas es al aire, el salto es gratis
            if (ordenadas[i].traste == 0 || ordenadas[i-1].traste == 0) {
                paresAdyacentes++;
                continue;
            }
            
            int delta = std::abs(ordenadas[i].traste - ordenadas[i-1].traste);
            
            // El estiramiento en trastes bajos es más difícil.
            int minTraste = std::min(ordenadas[i].traste, ordenadas[i-1].traste);
            double factorDificultad = (minTraste <= 5) ? 1.5 : 1.0;
            
            double deltaPenalizado = delta * factorDificultad;
            
            if (deltaPenalizado > config.max_hand_span) {
                saltosPenalizados++;
            }
            paresAdyacentes++;
        }
    }
    
    if (paresAdyacentes > 0) {
        C_s = static_cast<double>(saltosPenalizados) / paresAdyacentes;
    }

    // --------------------------------------------------------
    // 6. Costo total
    // --------------------------------------------------------
    return
        config.w_t_armonico * C_t +
        config.w_e_armonico * C_e +
        config.w_d_armonico * C_d +
        config.w_s_armonico * C_s;
}

// ============================================================
// GENERACIÓN DE CANDIDATOS MELÓDICOS
// ============================================================

// Genera posiciones posibles para una nota.
//
// indice_nota queda en -1.
// El caller debe asignarlo según el índice de la nota
// dentro de InstanteMapper::notas_activas.

inline std::vector<CandidatoMelodico> generarCandidatosMelodicos(
    const Nota& nota,
    const ConfigMapper& config
) {
    std::vector<CandidatoMelodico> candidatos;

    if (nota.pitch < 0 ||
        config.traste_maximo < 0 ||
        config.max_candidatos_melodicos <= 0) {
        return candidatos;
    }

    for (int i = 0; i < 6; i++) {

        if (i >= static_cast<int>(config.afinacion.size())) {
            break;
        }

        int cuerda = i + 1;

        int traste =
            nota.pitch - config.afinacion[i];

        if (traste < 0 ||
            traste > config.traste_maximo) {
            continue;
        }

        PosicionGuitarra posicion;

        posicion.cuerda = cuerda;
        posicion.traste = traste;
        posicion.pitch = nota.pitch;

        CandidatoMelodico candidato;

        candidato.indice_nota = -1;
        candidato.posicion = posicion;
        candidato.costo = costoMelodico(
            posicion,
            config
        );

        candidatos.push_back(candidato);
    }

    std::sort(
        candidatos.begin(),
        candidatos.end(),
        [](const CandidatoMelodico& a,
           const CandidatoMelodico& b) {
            return a.costo < b.costo;
        }
    );

    if (candidatos.size() >
        static_cast<size_t>(config.max_candidatos_melodicos)) {

        candidatos.resize(
            config.max_candidatos_melodicos
        );
    }

    return candidatos;
}


// ============================================================
// GENERACIÓN DE CANDIDATOS ARMÓNICOS
// ============================================================

namespace detalleCandidatos {

    // Genera todas las posiciones posibles para un pitch.

    inline std::vector<PosicionGuitarra> posicionesParaPitch(
        int pitch,
        const ConfigMapper& config
    ) {
        std::vector<PosicionGuitarra> posiciones;

        for (int i = 0; i < 6; i++) {

            if (i >= static_cast<int>(config.afinacion.size())) {
                break;
            }

            int traste =
                pitch - config.afinacion[i];

            if (traste < 0 ||
                traste > config.traste_maximo) {
                continue;
            }

            PosicionGuitarra posicion;

            posicion.cuerda = i + 1;
            posicion.traste = traste;
            posicion.pitch = pitch;

            posiciones.push_back(posicion);
        }

        return posiciones;
    }


    // El candidato con mayor costo queda en la cima
    // de la priority_queue.

    struct ComparadorCosto {

        bool operator()(
            const CandidatoVoicing& a,
            const CandidatoVoicing& b
        ) const {
            return a.costo < b.costo;
        }
    };

}


// Genera combinaciones de posiciones para todas las notas
// distintas del evento.
//
// Explora las combinaciones válidas y conserva los mejores
// max_candidatos_armonicos_generados.
// Después devuelve los max_candidatos_armonicos mejores.


//que funcion tan frustrante
//you should really try and do all of this in the main.cpp, 
inline std::vector<CandidatoVoicing> generarCandidatosArmonicos(
    const EventoArmonico& evento,
    const ConfigMapper& config
) {
    std::vector<CandidatoVoicing> resultado;

    int maxGenerados =
        config.max_candidatos_armonicos_generados;

    int maxConservados =
        config.max_candidatos_armonicos;

    if (evento.notas.empty() ||
        
        config.traste_maximo < 0 ||
        maxGenerados <= 0 ||
        maxConservados <= 0) {
        return resultado;
    }
    // Solo saltar si es una sola nota (eso lo maneja melodía)
    if (evento.notas.size() < 2) {
        return resultado;
    }


    // --------------------------------------------------------
    // 1. Obtener pitches únicos
    // --------------------------------------------------------

    std::vector<int> pitches;

    for (const Nota& nota : evento.notas) {

        if (nota.pitch < 0) {
            continue;
        }

        if (std::find(
            pitches.begin(),
            pitches.end(),
            nota.pitch
        ) == pitches.end()) {

            pitches.push_back(nota.pitch);
        }
    }

    if (pitches.empty() || pitches.size() > 6) {
        return resultado;
    }

    std::sort(pitches.begin(), pitches.end());

    // --------------------------------------------------------
    // 2. Generar posiciones posibles para cada pitch
    // --------------------------------------------------------
    
    std::vector<std::vector<PosicionGuitarra>> opciones;
    bool combinatoriaValida = true;

    for (int pitch : pitches) {
        std::vector<PosicionGuitarra> posiciones =
            detalleCandidatos::posicionesParaPitch(pitch, config);

        // Si una nota no puede tocarse en la guitarra,
        // la combinatoria falla, pero no perdemos la biblioteca.
        if (posiciones.empty()) {
            combinatoriaValida = false;
            break;
        }

        opciones.push_back(posiciones);
    }

    if (!combinatoriaValida) {
        // No se puede hacer combinatoria (alguna nota no cabe en el mástil).
        // Devolvemos solo los candidatos de la biblioteca que ya generamos.
        std::sort(
            resultado.begin(),
            resultado.end(),
            [](const CandidatoVoicing& a, const CandidatoVoicing& b) {
                return a.costo < b.costo;
            }
        );
        
        if (resultado.size() > static_cast<size_t>(maxConservados)) {
            resultado.resize(maxConservados);
        }
        
        return resultado;
    }


    // --------------------------------------------------------
    // 3. Preparar selección de los mejores candidatos
    // --------------------------------------------------------

    std::priority_queue<
        CandidatoVoicing,
        std::vector<CandidatoVoicing>,
        detalleCandidatos::ComparadorCosto
    > mejores;


    // --------------------------------------------------------
    // 4. Explorar combinaciones
    // --------------------------------------------------------

    std::vector<PosicionGuitarra> actual;

    std::function<void(size_t)> explorar =
        [&](size_t indice) {

            // Todas las notas tienen una posición asignada.

            if (indice == opciones.size()) {

                CandidatoVoicing candidato;

                candidato.posiciones = actual;

                std::sort(
                    candidato.posiciones.begin(),
                    candidato.posiciones.end(),
                    [](const PosicionGuitarra& a,
                       const PosicionGuitarra& b) {
                        return a.cuerda < b.cuerda;
                    }
                );

                candidato.costo = costoArmonico(
                    candidato.posiciones,
                    config,
                    evento.root_pitch_class,
                    evento.quality_index
                );

                if (static_cast<int>(mejores.size()) <
                    maxGenerados) {

                    mejores.push(candidato);
                }
                else if (candidato.costo < mejores.top().costo) {

                    mejores.pop();
                    mejores.push(candidato);
                }

                return;
            }


            // Probar cada posición disponible
            // para la nota actual.

            for (const auto& posicion : opciones[indice]) {

                bool cuerdaOcupada = false;

                for (const auto& anterior : actual) {

                    if (anterior.cuerda == posicion.cuerda) {
                        cuerdaOcupada = true;
                        break;
                    }
                }

                if (cuerdaOcupada) {
                    continue;
                }

                actual.push_back(posicion);

                explorar(indice + 1);

                actual.pop_back();
            }
        };

    explorar(0); //para correr la lambda


    // --------------------------------------------------------
    // 5. Extraer y ordenar los mejores candidatos
    // --------------------------------------------------------

    while (!mejores.empty()) {

        resultado.push_back(mejores.top());

        mejores.pop();
    }

    std::sort(
        resultado.begin(),
        resultado.end(),
        [](const CandidatoVoicing& a,
           const CandidatoVoicing& b) {
            return a.costo < b.costo;
        }
    );


    // Conservar únicamente los 50 mejores por defecto.

    if (resultado.size() >
        static_cast<size_t>(maxConservados)) {

        resultado.resize(maxConservados);
    }

    return resultado;
}

#endif
