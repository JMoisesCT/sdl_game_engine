#include "Screenshot.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <cstdio>

namespace Screenshot {

std::string save(SDL_Renderer* renderer, const std::string& folder) {
    if (!renderer) return "";

    // Lee lo que hay dibujado AHORA en el renderer. Por eso se llama antes de
    // SDL_RenderPresent: despues del Present el contenido del backbuffer no esta
    // garantizado (puede salir negro o el frame anterior).
    SDL_Surface* shot = SDL_RenderReadPixels(renderer, nullptr);
    if (!shot) {
        SDL_Log("Screenshot: no se pudo leer la pantalla: %s", SDL_GetError());
        return "";
    }

    SDL_CreateDirectory(folder.c_str()); // si ya existe, no pasa nada

    // Nombre con fecha y hora local; los milisegundos evitan pisar una captura si se
    // pulsa la tecla dos veces en el mismo segundo.
    SDL_Time now = 0;
    SDL_DateTime dt{};
    SDL_GetCurrentTime(&now);
    SDL_TimeToDateTime(now, &dt, true);
    char name[64];
    std::snprintf(name, sizeof(name), "captura_%04d%02d%02d_%02d%02d%02d_%03d.png",
                  dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second,
                  dt.nanosecond / 1000000);
    std::string path = folder + "/" + name;

    bool ok = IMG_SavePNG(shot, path.c_str());
    SDL_DestroySurface(shot);
    if (!ok) {
        SDL_Log("Screenshot: no se pudo guardar %s: %s", path.c_str(), SDL_GetError());
        return "";
    }

    // Igual que al guardar un nivel: la ruta es relativa al directorio de trabajo, asi
    // que se deja la ruta completa en el log para saber donde quedo.
    char* cwd = SDL_GetCurrentDirectory(); // termina en separador
    SDL_Log("Screenshot: guardada en %s%s", cwd ? cwd : "", path.c_str());
    SDL_free(cwd);
    return path;
}

} // namespace Screenshot
