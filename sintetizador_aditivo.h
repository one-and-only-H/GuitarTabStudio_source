#ifndef SINTETIZADOR_ADITIVO_H
#define SINTETIZADOR_ADITIVO_H

#include "sintetizador.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>


// la parte más bonita del codigo



class SintetizadorAditivo : public ISintetizador {
private:
    static constexpr int NUM_HARMONICS = 7;
    static constexpr std::size_t MAX_VOCES = 18;
    static constexpr double PI_AD = 3.14159265358979323846;

    enum class EstadoADSR { ATTACK, DECAY, SUSTAIN, RELEASE, OFF };

    struct PerfilArmonico {
        float pesos[NUM_HARMONICS]{};
        float tasasDecaimiento[NUM_HARMONICS]{};
        float ataques[NUM_HARMONICS]{};
        float sustentos[NUM_HARMONICS]{};
        float releases[NUM_HARMONICS]{};
    };

    struct EnvolventeADSR {
        EstadoADSR estado = EstadoADSR::OFF;
        float valorActual = 0.0f;
        float valorInicioRelease = 0.0f;
        float tiempoEnEstado = 0.0f;
    };

    struct VozAditiva {
        int pitch = -1;
        float duracionTotal = 0.0f;
        float tiempoTranscurrido = 0.0f;
        float volumenVelocity = 0.0f;
        float brilloVelocity = 1.0f;
        float bendInicialRatio = 0.0f;
        bool forzarRelease = false;

        float frecuencias[NUM_HARMONICS]{};
        float fases[NUM_HARMONICS]{};
        EnvolventeADSR envolventes[NUM_HARMONICS]{};
        PerfilArmonico perfil{};
    };

    uint32_t sampleRate_;
    TipoPastilla pastilla_ = TipoPastilla::SINGLE_COIL;
    std::vector<VozAditiva> voces_;

    //look up table para que no tenga que hacer el calculo del seno cada sample, 
    static inline float generarOndaLUT(float frecuencia, float& fase, float sr) {
        constexpr int LUT_SIZE = 2048;
        constexpr float DOS_PI = 2.0f * static_cast<float>(PI_AD); 
        static const std::array<float, LUT_SIZE> tablaSeno = []() {
            std::array<float, LUT_SIZE> t{};
            for (int i = 0; i < LUT_SIZE; ++i) {
                t[i] = std::sin((DOS_PI * static_cast<float>(i)) / static_cast<float>(LUT_SIZE));
            }
            return t;
        }();

        const float pos = (fase / DOS_PI) * static_cast<float>(LUT_SIZE);
        const int idx0 = static_cast<int>(pos) & (LUT_SIZE - 1);
        const int idx1 = (idx0 + 1) & (LUT_SIZE - 1);
        const float frac = pos - static_cast<float>(static_cast<int>(pos));
        const float val = tablaSeno[idx0] * (1.0f - frac) + tablaSeno[idx1] * frac;

        fase += (DOS_PI * frecuencia) / sr;
        if (fase >= DOS_PI) fase -= DOS_PI;
        return val;
    }

    static inline PerfilArmonico construirPerfil(TipoPastilla pastilla) {
        PerfilArmonico perfil{};
        if (pastilla == TipoPastilla::SINGLE_COIL) {
            const float p[] = {1.0f, 0.85f, 0.75f, 0.55f, 0.35f, 0.20f, 0.10f};
            const float d[] = {1.4f, 0.9f,  0.55f, 0.35f, 0.22f, 0.15f, 0.10f};
            std::copy(p, p + NUM_HARMONICS, perfil.pesos);
            std::copy(d, d + NUM_HARMONICS, perfil.tasasDecaimiento);
        } else {
            const float p[] = {1.0f, 0.95f, 0.50f, 0.22f, 0.10f, 0.04f, 0.01f};
            const float d[] = {1.8f, 1.3f,  0.70f, 0.40f, 0.25f, 0.15f, 0.10f};
            std::copy(p, p + NUM_HARMONICS, perfil.pesos);
            std::copy(d, d + NUM_HARMONICS, perfil.tasasDecaimiento);
        }

        const float a[] = {0.002f, 0.002f, 0.001f, 0.001f, 0.001f, 0.001f, 0.001f};
        const float s[] = {0.65f,  0.45f,  0.22f,  0.08f,  0.02f,  0.00f,  0.00f};
        const float r[] = {0.25f,  0.18f,  0.12f,  0.08f,  0.05f,  0.03f,  0.02f};
        std::copy(a, a + NUM_HARMONICS, perfil.ataques);
        std::copy(s, s + NUM_HARMONICS, perfil.sustentos);
        std::copy(r, r + NUM_HARMONICS, perfil.releases);
        return perfil;
    }
    //hell in heaven is the next function
    static inline void reconstruirADSR(EnvolventeADSR& env, float elapsed,
                                       float duracion, float ataque, float decayRate,
                                       float sustain, float release) {
        env = EnvolventeADSR{};
        const float attackTime = std::max(0.001f, ataque);
        const float decayTime = std::max(0.001f, 0.5f * decayRate);

        if (elapsed < attackTime && elapsed < duracion) {
            env.estado = EstadoADSR::ATTACK;
            env.tiempoEnEstado = elapsed;
            env.valorActual = std::clamp(elapsed / attackTime, 0.0f, 1.0f);
            return;
        }

        float valorAlSoltar = 1.0f;
        if (duracion < attackTime) {
            valorAlSoltar = std::clamp(duracion / attackTime, 0.0f, 1.0f);
        } else if (duracion < attackTime + decayTime) {
            const float prog = (duracion - attackTime) / decayTime;
            valorAlSoltar = 1.0f - ((1.0f - sustain) * std::clamp(prog, 0.0f, 1.0f));
        } else {
            valorAlSoltar = sustain;
        }

        if (elapsed < duracion) {
            if (elapsed < attackTime + decayTime) {
                env.estado = EstadoADSR::DECAY;
                env.tiempoEnEstado = elapsed - attackTime;
                const float prog = std::clamp(env.tiempoEnEstado / decayTime, 0.0f, 1.0f);
                env.valorActual = 1.0f - ((1.0f - sustain) * prog);
            } else {
                env.estado = EstadoADSR::SUSTAIN;
                env.tiempoEnEstado = elapsed - attackTime - decayTime;
                env.valorActual = sustain;
            }
            return;
        }

        const float tRel = std::max(0.001f, release);
        const float relElapsed = elapsed - duracion;
        if (relElapsed >= tRel || valorAlSoltar <= 0.0f) {
            env.estado = EstadoADSR::OFF;
            env.valorActual = 0.0f;
        } else {
            env.estado = EstadoADSR::RELEASE;
            env.tiempoEnEstado = relElapsed;
            env.valorInicioRelease = valorAlSoltar;
            env.valorActual = valorAlSoltar * (1.0f - relElapsed / tRel);
        }
    }

    static inline bool vozActiva(const VozAditiva& v) {
        for (int h = 0; h < NUM_HARMONICS; ++h) {
            if (v.envolventes[h].estado == EstadoADSR::ATTACK ||
                v.envolventes[h].estado == EstadoADSR::DECAY ||
                v.envolventes[h].estado == EstadoADSR::SUSTAIN) {
                return true;
            }
            if (v.envolventes[h].estado == EstadoADSR::RELEASE &&
                v.envolventes[h].valorActual > 0.003f) {
                return true;
            }
        }
        return false;
    }
    //record:  49m en hacer esa
    

public:
    explicit SintetizadorAditivo(uint32_t sampleRate = 44100)
        : sampleRate_(sampleRate) {
        voces_.reserve(MAX_VOCES);
    }

    void configurarPastilla(TipoPastilla tipo) override {
        pastilla_ = tipo;
    }

    void noteOn(int pitch, float velocity, float duracion, float elapsed = 0.0f) override {
        VozAditiva v{};
        v.pitch = pitch;
        v.duracionTotal = duracion;
        v.tiempoTranscurrido = elapsed;

        const float vel = std::clamp(velocity, 0.0f, 1.0f);
        v.volumenVelocity = vel * vel;
        // Fase 4: Brillo armonico y desvio inicial de pitch segun velocity
        v.brilloVelocity = 0.45f + 0.55f * vel;
        const float centsMax = 18.0f * (vel * vel);
        v.bendInicialRatio = std::pow(2.0f, centsMax / 1200.0f) - 1.0f;

        const float f0 = 440.0f * std::pow(2.0f, (pitch - 69) / 12.0f);
        v.perfil = construirPerfil(pastilla_);

        for (int h = 0; h < NUM_HARMONICS; ++h) {
            v.frecuencias[h] = f0 * static_cast<float>(h + 1);
            v.fases[h] = std::fmod(2.0f * static_cast<float>(PI_AD) * v.frecuencias[h] * elapsed,
                                   2.0f * static_cast<float>(PI_AD));
            reconstruirADSR(v.envolventes[h], elapsed, duracion,
                            v.perfil.ataques[h], v.perfil.tasasDecaimiento[h],
                            v.perfil.sustentos[h], v.perfil.releases[h]);
        }

        if (!vozActiva(v)) return;
        if (voces_.size() >= MAX_VOCES) {
            voces_.erase(voces_.begin());
        }
        voces_.push_back(v);
    }

    void noteOff(int pitch) override {
        for (auto& v : voces_) {
            if (v.pitch == pitch && !v.forzarRelease) {
                v.forzarRelease = true;
                v.duracionTotal = v.tiempoTranscurrido;
                break;
            }
        }
    }

    void render(float* buffer, std::size_t cantidadMuestras) override {
        if (!buffer || cantidadMuestras == 0) return;

        const float sr = static_cast<float>(sampleRate_);
        const float dt = 1.0f / sr;

        for (std::size_t m = 0; m < cantidadMuestras; ++m) {
            float sumaNotas = 0.0f;
            for (auto& v : voces_) {
                const float envTension = std::exp(-v.tiempoTranscurrido * 24.0f);
                const float factorPitch = 1.0f + v.bendInicialRatio * envTension;

                float sumaArm = 0.0f;
                for (int h = 0; h < NUM_HARMONICS; ++h) {
                    auto& env = v.envolventes[h];
                    if (v.tiempoTranscurrido >= v.duracionTotal &&
                        env.estado != EstadoADSR::RELEASE && env.estado != EstadoADSR::OFF) {
                        env.valorInicioRelease = env.valorActual;
                        env.estado = EstadoADSR::RELEASE;
                        env.tiempoEnEstado = 0.0f;
                    }
                    env.tiempoEnEstado += dt;

                    switch (env.estado) {
                        case EstadoADSR::ATTACK: {
                            const float a = std::max(0.001f, v.perfil.ataques[h]);
                            env.valorActual = std::min(1.0f, env.tiempoEnEstado / a);
                            if (env.valorActual >= 1.0f) {
                                env.valorActual = 1.0f;
                                env.estado = EstadoADSR::DECAY;
                                env.tiempoEnEstado = 0.0f;
                            }
                            break;
                        }
                        case EstadoADSR::DECAY: {
                            const float d = std::max(0.001f, 0.5f * v.perfil.tasasDecaimiento[h]);
                            const float p = std::min(1.0f, env.tiempoEnEstado / d);
                            env.valorActual = 1.0f - ((1.0f - v.perfil.sustentos[h]) * p);
                            if (p >= 1.0f) {
                                env.valorActual = v.perfil.sustentos[h];
                                env.estado = EstadoADSR::SUSTAIN;
                                env.tiempoEnEstado = 0.0f;
                            }
                            break;
                        }
                        case EstadoADSR::SUSTAIN:
                            env.valorActual = v.perfil.sustentos[h];
                            break;
                        case EstadoADSR::RELEASE: {
                            const float r = std::max(0.001f, v.perfil.releases[h]);
                            const float p = std::min(1.0f, env.tiempoEnEstado / r);
                            env.valorActual = env.valorInicioRelease * (1.0f - p);
                            if (p >= 1.0f) {
                                env.valorActual = 0.0f;
                                env.estado = EstadoADSR::OFF;
                            }
                            break;
                        }
                        case EstadoADSR::OFF:
                            env.valorActual = 0.0f;
                            break;
                    }

                    const float escalaArm = (h >= 2) ? v.brilloVelocity : 1.0f;
                    const float onda = generarOndaLUT(v.frecuencias[h] * factorPitch, v.fases[h], sr);
                    sumaArm += onda * env.valorActual * v.perfil.pesos[h] * escalaArm;
                }

                sumaNotas += (sumaArm / static_cast<float>(NUM_HARMONICS)) * v.volumenVelocity;
                v.tiempoTranscurrido += dt;
            }

            const int n = std::max(1, static_cast<int>(voces_.size()));
            buffer[m] = sumaNotas / std::sqrt(static_cast<float>(n));
        }

        voces_.erase(
            std::remove_if(voces_.begin(), voces_.end(),
                           [](const VozAditiva& v) { return !vozActiva(v); }),
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

#endif // SINTETIZADOR_ADITIVO_H