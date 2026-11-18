#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import csv
import re
import subprocess

# ─── 1. AUTO-INSTALACIÓN FORZADA DE DEPENDENCIAS ───────────────
def instalar_pretty_midi():
    print("Verificando dependencias (pretty_midi)...")
    try:
        import pretty_midi
        print("  -> pretty_midi ya está instalado.")
    except ImportError:
        print("  -> pretty_midi no encontrado. Instalando automáticamente...")
        try:
            # Forzamos la instalación usando el mismo ejecutable de Python
            subprocess.check_call([sys.executable, "-m", "pip", "install", "pretty_midi"])
            print("  -> Instalación completada con éxito.")
        except Exception as e:
            print(f"\n[ERROR CRÍTICO] No se pudo instalar 'pretty_midi'.")
            print(f"Detalle: {e}")
            print("Por favor, instalalo manualmente con: pip install pretty_midi")
            sys.exit(1)

# Ejecutamos la verificación antes de importar
instalar_pretty_midi()
import pretty_midi

# ─── 2. CONSTANTES Y UTILIDADES ────────────────────────────────

NOMBRES_NOTAS = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
PALABRAS_EXCLUIDAS = ['bass', 'bajo', 'drum', 'bateria', 'percussion', 'kick', 'snare']

def limpiar_nombre(nombre):
    """Convierte un nombre de pista en un nombre de archivo seguro."""
    nombre = nombre.lower()
    nombre = re.sub(r'[^a-z0-9_]', '_', nombre)
    return nombre.strip('_') or "pista"

def pitch_a_nota(pitch):
    """Convierte un pitch MIDI (0-127) a nota y octava (C4 = 60)."""
    octava = (pitch // 12) - 1
    nombre = NOMBRES_NOTAS[pitch % 12]
    return nombre, octava

# ─── 3. LÓGICA PRINCIPAL ──────────────────────────────────────

def midi_a_csvs(ruta_midi, carpeta_salida):
    print(f"\n[INFO] Leyendo: {ruta_midi}")
    
    if not os.path.exists(ruta_midi):
        print(f"[ERROR] El archivo MIDI no existe: {ruta_midi}")
        return []

    os.makedirs(carpeta_salida, exist_ok=True)
    nombre_base_midi = os.path.splitext(os.path.basename(ruta_midi))[0]
    
    try:
        pm = pretty_midi.PrettyMIDI(ruta_midi)
    except Exception as e:
        print(f"[ERROR] Archivo MIDI corrupto o inválido: {e}")
        return []

    archivos_generados = []

    for i, instrumento in enumerate(pm.instruments):
        # 1. Determinar el nombre de la pista
        nombre_pista = instrumento.name.strip() if instrumento.name else f"pista_{i}"
        if not nombre_pista or nombre_pista.lower() in ["", "pista"]:
            nombre_pista = f"prog_{instrumento.program}"

        nombre_archivo = limpiar_nombre(nombre_pista)

        # 2. Filtros de exclusión
        if instrumento.is_drum:
            print(f"  [IGNORADA] Pista {i} ('{nombre_pista}'): Es batería.")
            continue
            
        if any(palabra in nombre_pista.lower() for palabra in PALABRAS_EXCLUIDAS):
            print(f"  [IGNORADA] Pista {i} ('{nombre_pista}'): Excluida por nombre.")
            continue

        # 3. Extraer notas válidas para guitarra (E2=40 a E6=88)
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
            print(f"  [IGNORADA] Pista {i} ('{nombre_pista}'): No tiene notas en rango [40-88].")
            continue

        # 4. Ordenar y guardar
        notas_extraidas.sort(key=lambda n: n['timestamp'])
        ruta_csv = os.path.join(carpeta_salida, f"{nombre_base_midi}_{nombre_archivo}.csv")
        
        with open(ruta_csv, mode='w', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            # Cabecera exacta esperada por C++
            writer.writerow(['nota', 'octava', 'pitch', 'duracion', 'timestamp', 'velocidad', 'tecnica', 'role', 'parametro'])
            
            for n in notas_extraidas:
                writer.writerow([
                    n['nota'], n['octava'], n['pitch'],
                    n['duracion'], n['timestamp'], n['velocidad'],
                    "normal", 1, 0.0
                ])
                
        print(f"  [GUARDADA] Pista {i} ('{nombre_pista}'): {len(notas_extraidas)} notas -> {ruta_csv}")
        archivos_generados.append(ruta_csv)

    print("\n[INFO] ¡Conversión terminada!")
    return archivos_generados

# ─── 4. PUNTO DE ENTRADA ──────────────────────────────────────

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Uso: python midi_to_csv.py <archivo.mid>")
        sys.exit(1)
        
    archivo_midi = sys.argv[1]
    
    # Carpeta songs/ relativa a este script
    carpeta_songs = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'songs')
    
    midi_a_csvs(archivo_midi, carpeta_songs)