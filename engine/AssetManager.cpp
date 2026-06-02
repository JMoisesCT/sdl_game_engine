#include "AssetManager.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

AssetManager::AssetManager(SDL_Renderer* renderer)
    : renderer(renderer) {}

AssetManager::~AssetManager() {
    // Liberamos todas las texturas que cargamos.
    for (auto& [path, texture] : textures) {
        SDL_DestroyTexture(texture);
    }
    textures.clear();
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

    // Guardar en cache y devolver.
    textures[path] = texture;
    return texture;
}
