#include "AssetManager.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

AssetManager::AssetManager(SDL_Renderer* renderer)
    : renderer(renderer) {}

AssetManager::~AssetManager() {
    // Liberamos todas las texturas que cargamos.
    for (auto& [path, texture] : textures) {
        SDL_DestroyTexture(texture);
    }
    textures.clear();

    // Y todas las fuentes.
    for (auto& [key, font] : fonts) {
        TTF_CloseFont(font);
    }
    fonts.clear();
}

SDL_Texture* AssetManager::loadTexture(const std::string& path) {
    // Si ya esta cargada, la devolvemos directo (sin recargar).
    auto it = textures.find(path);
    if (it != textures.end()) {
        return it->second;
    }

    // Primera vez: cargar desde disco.
    SDL_Surface* surface = IMG_Load(path.c_str());
    if (!surface) {
        SDL_Log("No se pudo cargar la imagen '%s': %s", path.c_str(), SDL_GetError());
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);

    if (!texture) {
        SDL_Log("No se pudo crear la textura '%s': %s", path.c_str(), SDL_GetError());
        return nullptr;
    }

    // Muestreo por vecino mas cercano (no bilineal): coherencia de pixel art y
    // evita el sangrado de bordes al escalar (tilesets y sprites por igual).
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    // Guardar en cache y devolver.
    textures[path] = texture;
    return texture;
}

TTF_Font* AssetManager::loadFont(const std::string& path, int size) {
    // Clave compuesta (ruta + tamanio): la misma fuente a dos tamanios son dos
    // recursos distintos, cada uno en su entrada de cache.
    std::string key = path + "#" + std::to_string(size);

    // Si ya esta abierta a ese tamanio, la devolvemos directo (sin reabrir).
    auto it = fonts.find(key);
    if (it != fonts.end()) {
        return it->second;
    }

    // Primera vez: abrir desde disco. En SDL3_ttf el tamanio en puntos es float.
    TTF_Font* font = TTF_OpenFont(path.c_str(), (float)size);
    if (!font) {
        SDL_Log("No se pudo abrir la fuente '%s' a %d pt: %s", path.c_str(), size, SDL_GetError());
        return nullptr;
    }

    // Guardar en cache y devolver.
    fonts[key] = font;
    return font;
}
