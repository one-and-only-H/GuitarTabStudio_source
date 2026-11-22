#ifndef KARPLUS_STRONG_H
#define KARPLUS_STRONG_H

#include "sintetizador.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

class SintetizadorKarplus : public ISintetizador {
private:
    static constexpr std::size_t MAX_VOCES = 18;
    static constexpr double PI_KS = 3.14159265358979323846;

    struct VozKS {
        int pitch = -1;
        float duracionTotal = 0.0f;
        float tiempoTranscurrido = 0.0f;
        float velocity = 0.0f;

        std::vector<float> delayLine;
        std::size_t writeIdx = 0;
        float periodoExacto = 100.0f;
        float bendInicialRatio = 0.0f;

        float prevLazo = 0.0f;
        float nivelActual = 1.0f;
        bool apagada = false;
        bool activa = false;
    };

    uint32_t sampleRate_;
    TipoPastilla pastilla_ = TipoPastilla::SINGLE_COIL;
    std::vector<VozKS> voces_;
    uint32_t rngState_ = 0x12345678u;

    // Generador pseudoaleatorio xorshift32 para excitacion y micro-variaciones
    inline float siguienteRuido() {
        uint32_t x = rngState_;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        rngState_ = x;
        return (static_cast<float>(x & 0xFFFFu) / 32768.0f) - 1.0f;
    }

    static inline float pitchAFrecuencia(int pitch) {
        return 440.0f * std::pow(2.0f, (pitch - 69) / 12.0f);
    }

    // Lectura con interpolacion lineal para afinacion exacta y pitch envelope
    inline float leerFraccional(const std::vector<float>& buf, float pos) const {
        const std::size_t N = buf.size();
        if (N == 0) return 0.0f;

        const float nf = static_cast<float>(N);
        while (pos >= nf) pos -= nf;
        while (pos < 0.0f) pos += nf;

        const std::size_t idx0 = static_cast<std::size_t>(pos);
        const std::size_t idx1 = (idx0 + 1) % N;
        const float frac = pos - static_cast<float>(idx0);
        return buf[idx0] * (1.0f - frac) + buf[idx1] * frac;
    }

    inline float procesarMuestraVoz(VozKS& v) {
        const std::size_t N = v.delayLine.size();
        if (N < 4 || !v.activa) return 0.0f;

        // Fase 4: Pitch envelope por tension inicial de la cuerda al pulsar fuerte
        const float envTension = std::exp(-v.tiempoTranscurrido * 24.0f);
        const float factorPitch = 1.0f + v.bendInicialRatio * envTension;
        const float retardoActual = std::clamp(
            v.periodoExacto / factorPitch,
            2.0f,
            static_cast<float>(N - 2)
        );

        const float posLectura = static_cast<float>(v.writeIdx) - retardoActual;
        const float actual = leerFraccional(v.delayLine, posLectura);

        // Brillo dinamico dependiente del ataque de pua y de la velocity
        const float factorAtaque = std::exp(-v.tiempoTranscurrido * 15.0f);
        const float brilloBase = (pastilla_ == TipoPastilla::SINGLE_COIL) ? 0.56f : 0.46f;
        const float boostVel = 0.08f * v.velocity;
        const float brillo = std::clamp(brilloBase + boostVel + 0.13f * factorAtaque, 0.36f, 0.76f);

        const float filtrada = brillo * actual + (1.0f - brillo) * v.prevLazo;
        v.prevLazo = filtrada;

        // Sustain magnetico de guitarra electrica y amortiguacion al soltar
        const bool enRelease = v.apagada || (v.tiempoTranscurrido >= v.duracionTotal);
        float rho = 0.9986f;
        if (enRelease) {
            rho = 0.960f;
        } else {
            const float compFrec = std::clamp((v.pitch - 40) / 48.0f, 0.0f, 1.0f);
            const float baseSustain = (pastilla_ == TipoPastilla::HUMBUCKER) ? 0.9993f : 0.9987f;
            rho = baseSustain - 0.0018f * compFrec;
        }

        const float nuevaMuestra = filtrada * rho;
        v.delayLine[v.writeIdx] = nuevaMuestra;

        // Modelado de respuesta de pastilla magnetica mediante filtro peine
        float salida = nuevaMuestra;
        if (pastilla_ == TipoPastilla::SINGLE_COIL) {
            const float posPickup = static_cast<float>(v.writeIdx) - (retardoActual * 0.18f);
            const float muestraPickup = leerFraccional(v.delayLine, posPickup);
            salida = (nuevaMuestra - 0.44f * muestraPickup) * 1.25f;
        } else {
            const float posBobina = static_cast<float>(v.writeIdx) - (retardoActual * 0.26f);
            const float muestraBobina = leerFraccional(v.delayLine, posBobina);
            salida = (nuevaMuestra + 0.52f * muestraBobina) * 1.32f;
        }

        v.writeIdx = (v.writeIdx + 1) % N;
        v.tiempoTranscurrido += 1.0f / static_cast<float>(sampleRate_);
        v.nivelActual = v.nivelActual * 0.995f + std::abs(salida) * 0.005f;

        if (enRelease && v.nivelActual < 0.0008f) {
            v.activa = false;
        }

        return salida * v.velocity;
    }

public:
    explicit SintetizadorKarplus(uint32_t sampleRate = 44100)
        : sampleRate_(sampleRate) {
        voces_.reserve(MAX_VOCES);
    }

    void configurarPastilla(TipoPastilla tipo) override {
        pastilla_ = tipo;
    }

    void noteOn(int pitch, float velocity, float duracion, float elapsed = 0.0f) override {
        const float freq = pitchAFrecuencia(pitch);
        if (freq <= 20.0f) return;

        // Buffer con margen extra para evitar colision entre escritura y lectura
        const float periodo = static_cast<float>(sampleRate_) / freq;
        const std::size_t longitudPeriodo = std::max<std::size_t>(2, static_cast<std::size_t>(std::round(periodo)));
        const std::size_t longitudBuffer = longitudPeriodo + 4;

        VozKS voz;
        voz.pitch = pitch;
        voz.duracionTotal = std::max(0.02f, duracion);
        voz.tiempoTranscurrido = 0.0f;
        voz.velocity = std::clamp(velocity, 0.0f, 1.0f);
        voz.periodoExacto = periodo;
        voz.delayLine.resize(longitudBuffer, 0.0f);
        voz.writeIdx = 0;
        voz.activa = true;

        // Desvio inicial de afinacion proporcional a la fuerza del golpe (hasta +22 cents)
        const float centsMax = 22.0f * (voz.velocity * voz.velocity);
        voz.bendInicialRatio = std::pow(2.0f, centsMax / 1200.0f) - 1.0f;

        // Variacion organica en la posicion de pua y color segun velocity
        const float jitterPua = 0.16f + 0.08f * ((siguienteRuido() + 1.0f) * 0.5f);
        const std::size_t puntoPua = std::clamp<std::size_t>(
            static_cast<std::size_t>(longitudPeriodo * jitterPua),
            1,
            std::max<std::size_t>(1, longitudPeriodo - 1)
        );

        const float baseSuavizado = (pastilla_ == TipoPastilla::SINGLE_COIL) ? 0.45f : 0.30f;
        const float suavizadoPua = std::clamp(baseSuavizado + 0.32f * voz.velocity, 0.25f, 0.82f);
        float estadoPua = 0.0f;

        for (std::size_t i = 0; i < longitudBuffer; ++i) {
            const std::size_t posCuerda = i % longitudPeriodo;
            const float ruido = siguienteRuido();
            estadoPua += suavizadoPua * (ruido - estadoPua);

            const float tri = (posCuerda < puntoPua)
                ? (static_cast<float>(posCuerda) / static_cast<float>(puntoPua))
                : (static_cast<float>(longitudPeriodo - posCuerda) /
                   static_cast<float>(std::max<std::size_t>(1, longitudPeriodo - puntoPua)));

            voz.delayLine[i] = 0.42f * estadoPua + 0.68f * (tri * 2.0f - 1.0f);
        }

        // Eliminar componente continua inicial
        float sumaDC = 0.0f;
        for (float s : voz.delayLine) sumaDC += s;
        const float mediaDC = sumaDC / static_cast<float>(longitudBuffer);
        for (float& s : voz.delayLine) s -= mediaDC;

        // Adelantar estado si la nota arranca en medio de un seek o cambio de motor
        if (elapsed > 0.0f) {
            const std::size_t muestrasAdelantar = static_cast<std::size_t>(
                std::min(elapsed, voz.duracionTotal + 0.5f) * static_cast<float>(sampleRate_)
            );
            for (std::size_t k = 0; k < muestrasAdelantar && voz.activa; ++k) {
                procesarMuestraVoz(voz);
            }
            if (!voz.activa) return;
        }

        if (voces_.size() >= MAX_VOCES) {
            voces_.erase(voces_.begin());
        }
        voces_.push_back(std::move(voz));
    }

    void noteOff(int pitch) override {
        for (auto& v : voces_) {
            if (v.activa && v.pitch == pitch && !v.apagada) {
                v.apagada = true;
                break;
            }
        }
    }

    void render(float* buffer, std::size_t cantidadMuestras) override {
        if (!buffer || cantidadMuestras == 0) return;

        for (std::size_t m = 0; m < cantidadMuestras; ++m) {
            float suma = 0.0f;
            int activas = 0;

            for (auto& v : voces_) {
                if (v.activa) {
                    suma += procesarMuestraVoz(v);
                    ++activas;
                }
            }

            const float norm = (activas > 1) ? (1.0f / std::sqrt(static_cast<float>(activas))) : 1.0f;
            buffer[m] = suma * norm * 0.80f;
        }

        voces_.erase(
            std::remove_if(voces_.begin(), voces_.end(),
                           [](const VozKS& v) { return !v.activa; }),
            voces_.end()
        );
    }

    void reset() override {
        voces_.clear();
    }

    bool tieneVocesActivas() const override {
        return !voces_.empty();
    }
};

#endif // KARPLUS_STRONG_H