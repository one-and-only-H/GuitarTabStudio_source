// afinacion[cuerda - 1] devuelve el pitch MIDI de la cuerda al aire.
// cuerda 1 (aguda) → afinacion[0] = 64 (E4)
// cuerda 6 (grave) → afinacion[5] = 40 (E2)

#ifndef CANDIDATOS_H
#define CANDIDATOS_H

#include "tipos_mapper.h"
#include "../acordes.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <queue>
#include <string>
#include <vector>

// ============================================================
// FORMAS CONOCIDAS
// ============================================================

// Las cuerdas se almacenan de primera a sexta:
//
// -1 = cuerda muteada
//  0 = cuerda al aire
// >0 = traste pisado

struct FormaConocida {
    std::string nombre;

    int root_pitch_class = -1;
    int quality_index = -1;

    std::vector<int> trastes;
};


// ============================================================
// BIBLIOTECA DE FORMAS
// ============================================================

// Contiene formas originales.
// Las transposiciones se generan durante la evaluación.

inline const std::vector<FormaConocida>& obtenerFormasConocidas() {

    static const std::vector<FormaConocida> formas = {

        // ----------------------------------------------------
        // MAYORES
        // ----------------------------------------------------

        {"C", 0, CalidadAcorde::MAYOR,
            {0, 1, 0, 2, 3, -1}},  

        {"A", 9, CalidadAcorde::MAYOR,
            {0, 2, 2, 2, 0, -1}},

        {"G", 7, CalidadAcorde::MAYOR,
            {3, 0, 0, 0, 2, 3}},

        {"E", 4, CalidadAcorde::MAYOR,
            {0, 0, 1, 2, 2, 0}},

        {"D", 2, CalidadAcorde::MAYOR,
            {2, 3, 2, 0, -1, -1}},

        // Formas móviles alternativas

        {"C_barra", 0, CalidadAcorde::MAYOR,
            {3, 5, 5, 5, 3, -1}},

        {"A_barra", 9, CalidadAcorde::MAYOR,
            {5, 5, 6, 7, 7, 5}},

        {"G_barra", 7, CalidadAcorde::MAYOR,
            {3, 3, 4, 5, 5, 3}},

        // ----------------------------------------------------
        // MENORES
        // ----------------------------------------------------

        {"Am", 9, CalidadAcorde::MENOR,
            {0, 1, 2, 2, 0, -1}},

        {"Em", 4, CalidadAcorde::MENOR,
            {0, 0, 0, 2, 2, 0}},

        {"Dm", 2, CalidadAcorde::MENOR,
            {1, 3, 2, 0, -1, -1}},

        // Formas móviles alternativas

        {"Am_barra", 9, CalidadAcorde::MENOR,
            {5, 5, 5, 7, 7, 5}},

        {"Bm", 11, CalidadAcorde::MENOR,
            {7, 7, 7, 9, 9, 7}},

        // ----------------------------------------------------
        // SÉPTIMAS
        // ----------------------------------------------------

        {"A7", 9, CalidadAcorde::SEPTIMA,
            {0, 2, 0, 2, 0, -1}},

        {"D7", 2, CalidadAcorde::SEPTIMA,
            {2, 1, 2, 0, -1, -1}},

        {"E7", 4, CalidadAcorde::SEPTIMA,
            {0, 0, 1, 0, 2, 0}},

        {"G7", 7, CalidadAcorde::SEPTIMA,
            {1, 0, 0, 0, 2, 3}},

        {"C7", 0, CalidadAcorde::SEPTIMA,
            {0, 1, 3, 2, 3, -1}},

        {"B7", 11, CalidadAcorde::SEPTIMA,
            {2, 0, 2, 1, 2, -1}},

        // ----------------------------------------------------
        // F MAYOR
        // ----------------------------------------------------

        {"F", 5, CalidadAcorde::MAYOR,
            {1, 1, 2, 3, 3, 1}},

        {"F_simplificado", 5, CalidadAcorde::MAYOR,
            {1, 1, 2, 3, -1, -1}},

        // ----------------------------------------------------
        // POWER CHORD
        // ----------------------------------------------------

        {"E5", 4, CalidadAcorde::POWER_CHORD,
            {-1, -1, -1, 2, 2, 0}}
    };
    
    return formas;
}



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
    int notasValidas = 0; // Contador de notas que pertenecen al evento original

    int cuerdaMinima = 6;
    int cuerdaMaxima = 1;

    int trasteMinimo = config.traste_maximo;
    int trasteMaximo = 0;

    int notasPisadas = 0;

    for (const auto& posicion : posiciones) {
        // Si nos pasaron pitches permitidos, ignorar los que no estén en el evento
        if (pitchesPermitidos) {
            bool perteneceAlEvento = false;
            for (int p : *pitchesPermitidos) {
                if (pitchClass(posicion.pitch) == pitchClass(p)) {
                    perteneceAlEvento = true;
                    break;
                }
            }
            if (!perteneceAlEvento) {
                continue; // Ignorar esta nota para el cálculo de costo
            }
        }
        
        // ¡ACÁ FALTABA INCREMENTAR!
        notasValidas++;

        sumaTrastes += posicion.traste;

        cuerdaMinima = std::min(cuerdaMinima, posicion.cuerda);
        cuerdaMaxima = std::max(cuerdaMaxima, posicion.cuerda);

        if (posicion.traste == 0) {
            cuerdasAlAire++;
        }

        // Las cuerdas al aire no cuentan para la extensión de la mano.
        if (posicion.traste > 0) {
            trasteMinimo = std::min(trasteMinimo, posicion.traste);
            trasteMaximo = std::max(trasteMaximo, posicion.traste);
            notasPisadas++;
        }
    }
    
    if (notasValidas == 0) notasValidas = 1; // Evita división por cero

    // ¡ACÁ CAMBIAMOS posiciones.size() POR notasValidas!
    double C_t = sumaTrastes / (notasValidas * config.traste_maximo);


    // --------------------------------------------------------
    // 2. Extensión de la mano
    // --------------------------------------------------------

    double C_e = 0.0;

    if (notasPisadas >= 2) {

        int extension =
            trasteMaximo - trasteMinimo;

        C_e = std::min(
            1.0,
            static_cast<double>(
                std::max(0, extension - config.max_hand_span)
            ) /
            std::max(1, config.max_hand_span)
        );
    }


    // --------------------------------------------------------
    // 3. Distribución de las cuerdas
    // --------------------------------------------------------

    // Usar notasValidas en lugar de posiciones.size()
    double C_d =static_cast<double>((cuerdaMaxima - cuerdaMinima + 1) - notasValidas) / 5.0;

    C_d = std::clamp(C_d, 0.0, 1.0);


    // --------------------------------------------------------
    // 4. Similitud con formas conocidas
    // --------------------------------------------------------

    double C_f = 0.0;

    if (root_pitch_class >= 0 &&
        root_pitch_class < 12 &&
        quality_index >= 0) {

        // Calcula la distancia entre el candidato
        // y una forma conocida ya transpuesta.

        auto similitudConForma =
            [&](const FormaConocida& forma) {

                int activasForma = 0;
                int activasCandidato = 0;
                int interseccion = 0;

                double sumaDiferencias = 0.0;

                for (int cuerda = 1; cuerda <= 6; cuerda++) {

                    int trasteForma =
                        forma.trastes[cuerda - 1];

                    int trasteCandidato =
                        trasteEnCuerda(posiciones, cuerda);

                    bool activaForma = trasteForma >= 0;
                    bool activaCandidato = trasteCandidato >= 0;

                    if (activaForma) {
                        activasForma++;
                    }

                    if (activaCandidato) {
                        activasCandidato++;
                    }

                    if (activaForma && activaCandidato) {

                        interseccion++;

                        sumaDiferencias +=
                            std::abs(
                                trasteForma - trasteCandidato
                            );
                    }
                }

                if (activasForma == 0) {
                    return 1.0;
                }

                // Distancia entre trastes coincidentes.

                double distanciaTrastes =
                    sumaDiferencias /
                    (activasForma * config.traste_maximo);

                // Diferencia entre cuerdas activas.

                int diferenciaSimetrica =
                    activasForma +
                    activasCandidato -
                    2 * interseccion;

                double lambda = 0.30;

                double penalizacionCuerdas =
                    lambda * diferenciaSimetrica / 6.0;

                return std::clamp(
                    distanciaTrastes + penalizacionCuerdas,
                    0.0,
                    1.0
                );
            };


        // Buscar la mejor forma correspondiente
        // a la raíz y calidad del acorde.

        double mejorSimilitud = 1.0;

        bool encontroForma = false;

        const auto& formas = obtenerFormasConocidas();

        for (const auto& forma : formas) {

            if (forma.quality_index != quality_index) {
                continue;
            }

            int desplazamiento =
                pitchClass(
                    root_pitch_class -
                    forma.root_pitch_class
                );

            // Se prueban desplazamientos cromáticos
            // y sus equivalentes en otras octavas.

            for (int octava = -2; octava <= 2; octava++) {

                int transposicion =
                    desplazamiento + 12 * octava;

                FormaConocida formaTranspuesta = forma;

                bool valida = true;

                for (int& traste : formaTranspuesta.trastes) {

                    if (traste < 0) {
                        continue;
                    }

                    traste += transposicion;

                    if (traste < 0 ||
                        traste > config.traste_maximo) {

                        valida = false;
                        break;
                    }
                }

                if (!valida) {
                    continue;
                }

                double similitud =
                    similitudConForma(formaTranspuesta);

                mejorSimilitud = std::min(
                    mejorSimilitud,
                    similitud
                );

                encontroForma = true;
            }
        }

        // Si no existe una forma compatible,
        // no se penaliza al candidato.

        if (encontroForma) {
            C_f = mejorSimilitud;
        }
    }


    // --------------------------------------------------------
    // 5. Costo total
    // --------------------------------------------------------

    return
        config.w_t_armonico * C_t +
        config.w_e_armonico * C_e +
        config.w_d_armonico * C_d +
        config.w_f_armonico * C_f;
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
// 2.5 Candidatos desde la biblioteca de formas conocidas
// --------------------------------------------------------
if (evento.root_pitch_class >= 0 && evento.quality_index >= 0) {
    const auto& formas = obtenerFormasConocidas();
    for (const auto& forma : formas) {
        if (forma.quality_index != evento.quality_index) continue;
        
        int desplazamiento = pitchClass(evento.root_pitch_class - forma.root_pitch_class);
        
        // Probamos octavas cercanas
        for (int octava = -1; octava <= 1; octava++) {
            int transposicion = desplazamiento + 12 * octava;
            CandidatoVoicing cand;
            bool valida = true;
            
            for (int c = 0; c < 6; c++) {
                if (forma.trastes[c] < 0) continue;
                int traste = forma.trastes[c] + transposicion;
                
                if (traste < 0 || traste > config.traste_maximo) { 
                    valida = false; 
                    break; 
                }
                
                PosicionGuitarra p;
                p.cuerda = c + 1;
                p.traste = traste;
                p.pitch = config.afinacion[c] + traste;
                cand.posiciones.push_back(p);
            }
            
            if (valida && !cand.posiciones.empty()) {
                cand.costo = costoArmonico(cand.posiciones, config, evento.root_pitch_class, evento.quality_index, &pitches);
                resultado.push_back(cand);
            }
        }
    }
}
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
