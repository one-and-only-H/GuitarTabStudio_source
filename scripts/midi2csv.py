#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import csv
import re
import subprocess

# Evitar errores de codificacion CP1252 en Windows al imprimir nombres con tildes
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")


def instalar_pretty_midi():
    # Si corre compilado como .exe, no intentar lanzar pip con sys.executable
    if getattr(sys, "frozen", False):
        return
    try:
        import pretty_midi  # noqa: F401
    except ImportError:
        try:
            subprocess.check_call([sys.executable, "-m", "pip", "install", "pretty_midi"])
        except Exception as e:
            print(f"[ERROR] No se pudo instalar pretty_midi: {e}", file=sys.stderr, flush=True)
            sys.exit(1)


instalar_pretty_midi()
import pretty_midi


NOMBRES_NOTAS = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
PALABRAS_EXCLUIDAS = ['bass', 'bajo', 'drum', 'bateria', 'percussion', 'kick', 'snare']


def limpiar_nombre(nombre):
    nombre = nombre.lower()
    nombre = re.sub(r'[^a-z0-9_]', '_', nombre)
    return nombre.strip('_') or "pista"


def pitch_a_nota(pitch):
    octava = (pitch // 12) - 1
    nombre = NOMBRES_NOTAS[pitch % 12]
    return nombre, octava


def extraer_metrica_midi(pm):
    bpm = 120.0
    try:
        _, tempi = pm.get_tempo_changes()
        if len(tempi) > 0 and tempi[0] > 0:
            bpm = round(float(tempi[0]), 2)
    except Exception:
        bpm = 120.0

    numerador = 4
    denominador = 4
    try:
        if pm.time_signature_changes:
            ts = pm.time_signature_changes[0]
            if ts.numerator > 0 and ts.denominator > 0:
                numerador = int(ts.numerator)
                denominador = int(ts.denominator)
    except Exception:
        numerador, denominador = 4, 4

    return bpm, numerador, denominador


def midi_a_csvs(ruta_midi, carpeta_salida):
    if not os.path.exists(ruta_midi):
        print(f"[ERROR] El archivo MIDI no existe: {ruta_midi}", file=sys.stderr, flush=True)
        return []

    os.makedirs(carpeta_salida, exist_ok=True)
    nombre_base_midi = os.path.splitext(os.path.basename(ruta_midi))[0]

    try:
        pm = pretty_midi.PrettyMIDI(ruta_midi)
    except Exception as e:
        print(f"[ERROR] Archivo MIDI corrupto o invalido: {e}", file=sys.stderr, flush=True)
        return []

    bpm, numerador, denominador = extraer_metrica_midi(pm)
    print(f"BPM:{bpm}", flush=True)
    print(f"TIMESIG:{numerador}/{denominador}", flush=True)

    archivos_generados = []

    for i, instrumento in enumerate(pm.instruments):
        nombre_pista = instrumento.name.strip() if instrumento.name else f"pista_{i}"
        if not nombre_pista or nombre_pista.lower() in ["", "pista"]:
            nombre_pista = f"prog_{instrumento.program}"

        nombre_archivo = limpiar_nombre(nombre_pista)

        if instrumento.is_drum:
            continue

        if any(palabra in nombre_pista.lower() for palabra in PALABRAS_EXCLUIDAS):
            continue

        notas_extraidas = []
        for nota in instrumento.notes:
            if 40 <= nota.pitch <= 88:
                nombre, octava = pitch_a_nota(nota.pitch)
                notas_extraidas.append({
                    'pitch': nota.pitch,
                    'nota': nombre,
                    'octava': octava,
                    'duracion': round(nota.end - nota.start, 4),
                    'timestamp': round(nota.start, 4),
                    'velocidad': nota.velocity
                })

        if not notas_extraidas:
            continue

        notas_extraidas.sort(key=lambda n: n['timestamp'])
        ruta_csv = os.path.abspath(
            os.path.join(carpeta_salida, f"{nombre_base_midi}_{nombre_archivo}.csv")
        )

        with open(ruta_csv, mode='w', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            writer.writerow([
                'nota', 'octava', 'pitch', 'duracion',
                'timestamp', 'velocidad', 'tecnica', 'role', 'parametro'
            ])
            for n in notas_extraidas:
                writer.writerow([
                    n['nota'], n['octava'], n['pitch'],
                    n['duracion'], n['timestamp'], n['velocidad'],
                    "normal", 1, 0.0
                ])

        # Guardar metadatos de tempo y compas para cargas directas del CSV
        ruta_meta = ruta_csv + ".meta"
        with open(ruta_meta, mode='w', encoding='utf-8') as fmeta:
            fmeta.write(f"bpm={bpm}\n")
            fmeta.write(f"numerador={numerador}\n")
            fmeta.write(f"denominador={denominador}\n")

        # Reportar ruta exacta a C++ por stdout
        print(f"CSV_OUT:{ruta_csv}", flush=True)
        archivos_generados.append(ruta_csv)

    return archivos_generados


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Uso: midi2csv <archivo.mid>", file=sys.stderr, flush=True)
        sys.exit(1)

    archivo_midi = sys.argv[1]

    # En modo --onefile, __file__ apunta a %TEMP%; sys.executable apunta a scripts/midi2csv.exe
    if getattr(sys, "frozen", False):
        dir_base = os.path.dirname(os.path.abspath(sys.executable))
    else:
        dir_base = os.path.dirname(os.path.abspath(__file__))

    carpeta_songs = os.path.abspath(os.path.join(dir_base, '..', 'songs'))
    midi_a_csvs(archivo_midi, carpeta_songs)