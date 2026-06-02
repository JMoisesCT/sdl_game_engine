#pragma once
#include <string>
#include "Component.h"

struct SDL_Texture; // declaracion adelantada: SDL solo aparece en el .cpp

// Componente que dibuja una imagen segun el Transform del objeto.
// Ya NO carga ni destruye la textura: se la pide al AssetManager de la escena.

class SpriteRenderer : public Component {
public:
    explicit SpriteRenderer(std::string imagePath);

    void awake() override;  // pide la textura al AssetManager
    void render() override; // la dibuja en pantalla

private:
    std::string path;
    SDL_Texture* texture = nullptr; // PRESTADA por el AssetManager (no somos dueno)
    float width = 0.0f;
    float height = 0.0f;
};
