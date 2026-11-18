import pretty_midi
import csv
import os
import sys

# --- MAPEO DE NOTAS ---
NOMBRES_NOTAS = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']

def pitch_a_nota(pitch):
    """Convierte un pitch MIDI (0-127) a nota y octava (C4 = 60)."""
    octava = (pitch // 12) - 1
    nombre = NOMBRES_NOTAS[pitch % 12]
    return nombre, octava

def midi_a_csv(ruta_midi, ruta_csv):
    print(f"Leyendo {ruta_midi}...")
    
    try:
        # pretty_midi carga el archivo y maneja tempo/compás automáticamente
        pm = pretty_midi.PrettyMIDI(ruta_midi)
    except Exception as e:
        print(f"Error al abrir el archivo MIDI: {e}")
        return False

    notas_extraidas = []
    
    # Iterar sobre todos los instrumentos (pistas) del MIDI
    for instrumento in pm.instruments:
        # Opcional: ignorar la batería (canal 9 en MIDI estándar)
        if instrumento.is_drum:
            continue
            
        for nota in instrumento.notes:
            # FILTRO: Solo notas en el rango de la guitarra (E2=40 a E6=88)
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

    # Ordenar todas las notas por timestamp (importante para la timeline del mapper)
    notas_extraidas.sort(key=lambda n: n['timestamp'])

    print(f"Escribiendo {len(notas_extraidas)} notas en {ruta_csv}...")
    
    # Asegurarse de que el directorio existe
    os.makedirs(os.path.dirname(ruta_csv), exist_ok=True)
    
    with open(ruta_csv, mode='w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        # Cabecera exacta que espera C++
        writer.writerow(['nota', 'octava', 'pitch', 'duracion', 'timestamp', 'velocidad', 'tecnica', 'role', 'parametro'])
        
        for n in notas_extraidas:
            writer.writerow([
                n['nota'],
                n['octava'],
                n['pitch'],
                n['duracion'],
                n['timestamp'],
                n['velocidad'],
                "normal", # Técnica por defecto
                1,        # Role = ACORDE (el mapper lo recalcula)
                0.0       # Parámetro
            ])
            
    print("¡Conversión terminada con éxito!")
    return True

if __name__ == "__main__":
    if len(sys.argv) < 2:
        
        print("Uso: python midi_to_csv.py <archivo.mid>")
        sys.exit(1)


    archivo_midi = sys.argv[1]
    
    nombre_base = os.path.splitext(os.path.basename(archivo_midi))[0]
    # Guardar en la carpeta songs/ (asumiendo que el script está en scripts/)
    ruta_salida = os.path.join(os.path.dirname(__file__), '..', 'songs', f"{nombre_base}.csv")
    
    midi_a_csv(archivo_midi, ruta_salida)