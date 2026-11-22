#ifndef SINTETIZADOR_H
#define SINTETIZADOR_H

#include <cstddef>
#include <cstdint>

enum class TipoPastilla {
    SINGLE_COIL,
    HUMBUCKER
};

enum class TipoSintetizador {
    ADITIVO,
    KARPLUS_STRONG
};

class ISintetizador {
public:
    virtual ~ISintetizador() = default;

    virtual void configurarPastilla(TipoPastilla tipo) = 0;

    // elapsed: > 0.0f cuando se reconstruye el estado tras un seek()
    virtual void noteOn(
        int pitch,
        float velocity,
        float duracion,
        float elapsed = 0.0f
    ) = 0;

    virtual void noteOff(int pitch) = 0;

    // Renderiza exactamente 'cantidadMuestras' en el buffer de salida
    virtual void render(
        float* buffer,
        std::size_t cantidadMuestras
    ) = 0;

    virtual void reset() = 0;

    virtual bool tieneVocesActivas() const = 0;
};

#endif // SINTETIZADOR_H