/*
 * ARCHIVO: csv_parser.h / csv_parser.cpp
 *
 * CONTEXTO DE LO QUE YA SE TIENE:
 * El conversor MIDI ya exporto la pista seleccionada a un archivo .csv de 9
 * columnas (nota, octava, pitch, duracion, timestamp, velocidad, tecnica,
 * role, parametro) y se cuenta con la estructura Nota en tipos.h.
 *
 * HERRAMIENTAS A USAR:
 * "tipos.h", lectura de archivos y cadenas de C++ (<fstream>, <sstream>,
 * <string>, <vector>, <algorithm>, <stdexcept>).
 *
 * QUE DEBE HACER ESTE ARCHIVO:
 * Abrir el archivo .csv desde disco, saltar la linea de encabezados, separar
 * cada fila por comas, validar que los valores numericos y rangos de pitch
 * sean coherentes, y devolver un std::vector<Nota> ordenado por timestamp.
 */
#include "csv_parser.h"
#include "tipos.h"

#include <fstream>
#include <iostream>
#include <sstream>

//alivia hacer esto por fin despues de tanto del mapper
std::vector<Nota> parsearCSV(const std::string& rutaArchivo) {
    std::vector<Nota> notas;
    std::ifstream archivo(rutaArchivo);
    
    if (!archivo.is_open()) {
        throw std::runtime_error("No se pudo abrir el archivo: " + rutaArchivo);
    }

    std::string linea;
    
    // Saltar la cabecera
    std::getline(archivo, linea);

    int numLinea = 1; // La cabecera es la 1, los datos empiezan en la 2
    while (std::getline(archivo, linea)) {
        numLinea++;
        
        // Ignorar líneas vacías o solo con espacios
        if (limpiarCampo(linea).empty()) continue;
        
        try {
            notas.push_back(parsearNotaCSV(linea));
        } catch (const std::exception& e) {
            // Si una línea falla, avisamos y seguimos, para no perder toda la canción
            std::cerr << "Warning: error en línea " << numLinea << " (" << e.what() << "). Ignorando.\n";
        }
    }
    
    archivo.close();
    return notas;
}