#ifndef FORMAS_CONOCIDAS_H
#define FORMAS_CONOCIDAS_H

#include "../acordes.h" 
#include <string>
#include <vector>

// ------------------------------------------------------------
// REPRESENTACIÓN DE UNA FORMA DE GUITARRA
// ------------------------------------------------------------
struct FormaConocida {
    std::string nombre;
    int root_pitch_class = -1;
    int quality_index = -1;

    // Orden: cuerda 1 (E4) a cuerda 6 (E2).
    // -1 = cuerda muteada.
    //  0 = cuerda al aire.
    // >0 = traste pisado.
    std::vector<int> trastes;
};

// ------------------------------------------------------------
// BIBLIOTECA DE FORMAS CONOCIDAS
// ------------------------------------------------------------

//un poquito más y le metemos algebra lineal

inline const std::vector<FormaConocida>& obtenerFormasConocidas() {
    static const std::vector<FormaConocida> formas = {
        // ====================================================
        // MAYORES
        // ====================================================
        {"C",     0, CalidadAcorde::MAYOR, {0, 1, 0, 2, 3, -1}},
        {"A",     9, CalidadAcorde::MAYOR, {0, 2, 2, 2, 0, -1}},
        {"G",     7, CalidadAcorde::MAYOR, {3, 0, 0, 0, 2, 3}},
        {"E",     4, CalidadAcorde::MAYOR, {0, 0, 1, 2, 2, 0}},
        {"D",     2, CalidadAcorde::MAYOR, {2, 3, 2, 0, -1, -1}},
        {"C_barra", 0, CalidadAcorde::MAYOR, {3, 5, 5, 5, 3, -1}},
        {"A_barra", 9, CalidadAcorde::MAYOR, {5, 5, 6, 7, 7, 5}},
        {"G_barra", 7, CalidadAcorde::MAYOR, {3, 3, 4, 5, 5, 3}},
        {"F",     5, CalidadAcorde::MAYOR, {1, 1, 2, 3, 3, 1}},
        {"F_simplificado", 5, CalidadAcorde::MAYOR, {1, 1, 2, 3, -1, -1}},

        // ====================================================
        // MENORES
        // ====================================================
        {"Am",    9, CalidadAcorde::MENOR, {0, 1, 2, 2, 0, -1}}, //drake's favorite
        {"Em",    4, CalidadAcorde::MENOR, {0, 0, 0, 2, 2, 0}},
        {"Dm",    2, CalidadAcorde::MENOR, {1, 3, 2, 0, -1, -1}},
        {"Am_barra", 9, CalidadAcorde::MENOR, {5, 5, 5, 7, 7, 5}},
        {"Bm",   11, CalidadAcorde::MENOR, {7, 7, 7, 9, 9, 7}},
        // Menores con cejilla (Forma de Em)
        {"Cm",   0,  CalidadAcorde::MENOR, {3, 4, 5, 5, 3, -1}}, // x35543
        {"Gm",    7, CalidadAcorde::MENOR, {3, 5, 5, 3, 3, 3}},  // 355333
        {"Dm_barra", 2, CalidadAcorde::MENOR, {5, 6, 7, 7, 5, -1}},

        // ====================================================
        // SÉPTIMAS MENORES
        // ====================================================
        {"Am7",   9, CalidadAcorde::MENOR_SEPTIMA, {0, 1, 0, 2, 0, -1}},
        {"Dm7",   2, CalidadAcorde::MENOR_SEPTIMA, {1, 1, 2, 0, -1, -1}},
        {"Em7",   4, CalidadAcorde::MENOR_SEPTIMA, {0, 0, 0, 2, 3, 0}},
        {"Em7_b3",      4, CalidadAcorde::MENOR_SEPTIMA, {0, 3, 0, 2, 2, 0}},
        {"Em7_abierto", 4, CalidadAcorde::MENOR_SEPTIMA, {-1, 3, 0, -1, 2, 0}},

        // ====================================================
        // SÉPTIMAS MAYORES
        // ====================================================
        {"Cmaj7", 0, CalidadAcorde::MAYOR_SEPTIMA, {0, 0, 0, 2, 3, -1}},
        {"Dmaj7", 2, CalidadAcorde::MAYOR_SEPTIMA, {2, 2, 2, 0, -1, -1}},
        {"Gmaj7", 7, CalidadAcorde::MAYOR_SEPTIMA, {2, 0, 0, 0, 2, 3}},

        // ====================================================
        // SÉPTIMAS (Dominantes)
        // ====================================================
        {"A7",    9, CalidadAcorde::SEPTIMA, {0, 2, 0, 2, 0, -1}},
        {"D7",    2, CalidadAcorde::SEPTIMA, {2, 1, 2, 0, -1, -1}},
        {"E7",    4, CalidadAcorde::SEPTIMA, {0, 0, 1, 0, 2, 0}},
        {"G7",    7, CalidadAcorde::SEPTIMA, {1, 0, 0, 0, 2, 3}},
        {"C7",    0, CalidadAcorde::SEPTIMA, {0, 1, 3, 2, 3, -1}},
        {"B7",   11, CalidadAcorde::SEPTIMA, {2, 0, 2, 1, 2, -1}},

        // ====================================================
        // SUSPENDIDOS
        // ====================================================
        {"Asus4", 9, CalidadAcorde::SUS4, {0, 3, 2, 2, 0, -1}},
        {"Dsus4", 2, CalidadAcorde::SUS4, {3, 3, 2, 0, -1, -1}},
        {"Dsus2", 2, CalidadAcorde::SUS2, {0, 3, 2, 0, -1, -1}},

        // ====================================================
        // DISMINUIDOS
        // ====================================================
        {"Adim", 9, CalidadAcorde::DISMINUIDO, {-1, 1, 2, 1, 0, -1}},
        {"Ddim",  2, CalidadAcorde::DISMINUIDO, {1, 0, 1, 0, -1, -1}},

        // ====================================================
        // AUMENTADOS
        // ====================================================
        {"Caug",  0, CalidadAcorde::AUMENTADO, {0, 1, 1, 2, 3, -1}},
        {"Eaug", 4, CalidadAcorde::AUMENTADO, {0, 1, 1, 2, 3, 0}},

        // ====================================================
        // POWER CHORDS
        // ====================================================
        {"E5",    4, CalidadAcorde::POWER_CHORD, {-1, -1, -1, 2, 2, 0}},
        {"A5",    9, CalidadAcorde::POWER_CHORD, {-1, -1, 2, 2, 0, -1}},

        //triadas agudas
        {"Triada_Mayor_FormaD", 2, CalidadAcorde::MAYOR, {2, 3, 2, -1, -1, -1}},
        {"Triada_Mayor_FormaE", 5, CalidadAcorde::MAYOR, {1, 1, 2, -1, -1, -1}},
        {"Triada_Menor_FormaDm", 2, CalidadAcorde::MENOR, {1, 3, 2, -1, -1, -1}},
        {"Triada_Menor_FormaAm", 4, CalidadAcorde::MENOR, {7, 8, 9, -1, -1, -1}},
        {"Triada_Menor_FormaEm", 7, CalidadAcorde::MENOR, {7, 7, 7, -1, -1, -1}},

        //novena añadida
        {"Cadd9", 0, CalidadAcorde::MAYOR, {3, 3, 0, 2, 3, -1}}
        //y va a tocar seguir añadiendo más, pero con eso cubre ya la mayoría de canciones
        

    };

    return formas;
}

#endif // FORMAS_CONOCIDAS_H