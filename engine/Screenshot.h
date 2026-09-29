#pragma once
#include <string>

struct SDL_Renderer;

// Captura de pantalla a PNG. No es un componente: es una funcion suelta que el bucle
// de main llama cuando quiere (p.ej. al pulsar una tecla).
//
// Uso tipico (en el bucle, con TODO ya dibujado y ANTES de SDL_RenderPresent):
//     if (Input::wasPressed(Key::F9)) Screenshot::save(renderer);
//
// Guarda en la carpeta indicada (relativa al directorio de trabajo; se crea si no
// existe) un archivo "captura_AAAAMMDD_HHMMSS_mmm.png". Devuelve la ruta del archivo,
// o una cadena vacia si fallo (el motivo queda en el log).

namespace Screenshot {
    std::string save(SDL_Renderer* renderer, const std::string& folder = "screenshots");
}
