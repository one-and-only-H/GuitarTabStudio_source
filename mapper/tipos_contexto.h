
#ifndef TIPOS_CONTEXTO_H
#define TIPOS_CONTEXTO_H

#include <vector>

struct PerfilContextual {
    double inicio = 0.0;
    double fin = 0.0;

    int registroMediano = 60;
};

struct ContextoTemporal {
    std::vector<PerfilContextual> perfiles;
};

#endif
