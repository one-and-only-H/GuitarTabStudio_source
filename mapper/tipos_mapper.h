
#ifndef TIPOS_MAPPER_H
#define TIPOS_MAPPER_H

#include "../tipos.h"

#include <string>
#include <vector>

// Parámetros de configuración del mapper
struct ConfigMapper {
    int traste_maximo = 20;

    int max_hand_span = 4;
    int max_finger_stretch = 4;

    int max_candidatos_melodicos = 20;

    int max_candidatos_armonicos_generados = 100;
    int max_candidatos_armonicos = 50;

    // Afinación estándar en pitches MIDI
    // Orden: primera cuerda hasta sexta cuerda
    // E4, B3, G3, D3, A2, E2.

    // cuerda 1 (aguda) → afinacion[0] = 64 (E4)
    // cuerda 6 (grave) → afinacion[5] = 40 (E2)
    std::vector<int> afinacion = {
        64, 59, 55, 50, 45, 40
    };

    double w_t_melodico = 0.65;
    double w_c_melodico = 0.0;

    double w_t_armonico = 0.35;
    double w_e_armonico = 0.40;
    double w_d_armonico = 0.10;
    double w_f_armonico = 0.15;

        // Pesos de transición melódica
    double w_transicion_traste = 0.70;
    double w_transicion_cuerda = 0.30;

    // Pesos de transición armónica
    double w_transicion_voicing = 0.70;
    double w_transicion_centro = 0.30;
};

// Posición candidata para una nota individual.
struct CandidatoMelodico {
    int indice_nota = -1;
    PosicionGuitarra posicion;
    double costo = 0.0;
};

// Posible distribución de varias notas sobre la guitarra.
struct CandidatoVoicing {
    std::vector<PosicionGuitarra> posiciones;

    double costo = 0.0;
};

// Información de un instante del proceso de mapeo.
struct InstanteMapper {
    double timestamp = 0.0;
    double duracion = 0.0;

    std::vector<Nota> notas_activas;

    std::vector<CandidatoMelodico> candidatos_melodicos;
    std::vector<CandidatoVoicing> candidatos_armonicos;
};

// Estado utilizado durante la optimización con Viterbi
struct EstadoViterbi {
    double costo_acumulado = 0.0;

    int indice_candidato = -1;
    int indice_anterior = -1;
};

// Advertencia producida durante el mapeo
struct Warning {
    std::string mensaje;

    double timestamp = 0.0;
    int pitch = -1;
};

#endif
