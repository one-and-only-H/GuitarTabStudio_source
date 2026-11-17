#include "csv_parser.h"
#include "tipos.h"

#include <fstream>
#include <iostream>
#include <sstream>

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