#pragma once
#include <string>
#include <unordered_map>

struct SDL_Renderer;
struct SDL_Texture;

// Carga y guarda en cache las texturas. Es el DUENO de todas ellas:
// si dos objetos piden la misma imagen, comparten la misma textura en memoria.

class AssetManager {
public:
    explicit AssetManager(SDL_Renderer* renderer);
    ~AssetManager();

    // Es dueno de recursos: no permitimos copiarlo.
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    // Devuelve la textura de esa ruta. La carga SOLO la primera vez;
    // las siguientes devuelve la que ya esta en cache. nullptr si falla.
    SDL_Texture* loadTexture(const std::string& path);

private:
    SDL_Renderer* renderer = nullptr;                       // no somos dueno
    std::unordered_map<std::string, SDL_Texture*> textures; // SI somos dueno
};
