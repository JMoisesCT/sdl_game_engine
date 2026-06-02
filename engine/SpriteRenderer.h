#pragma once
#include <string>
#include "Component.h"

struct SDL_Texture; // declaracion adelantada: SDL solo aparece en el .cpp

// Dibuja una imagen segun el Transform. Puede dibujar la imagen completa o,
// si se le da un recorte (setSourceRect), solo una porcion: asi un spritesheet
// se convierte en un frame a la vez. El SpriteAnimator usa justamente esto.

class SpriteRenderer : public Component {
public:
    explicit SpriteRenderer(std::string imagePath);

    void awake() override;
    void render() override;

    // Dibujar solo una porcion de la imagen (un frame del spritesheet).
    void setSourceRect(float x, float y, float w, float h) {
        srcX = x; srcY = y; srcW = w; srcH = h; useSrcRect = true;
    }

private:
    std::string path;
    SDL_Texture* texture = nullptr; // prestada por el AssetManager (no somos dueno)
    float width = 0.0f;             // tamano de la imagen COMPLETA
    float height = 0.0f;

    bool  useSrcRect = false;       // false = imagen completa; true = solo el recorte
    float srcX = 0.0f, srcY = 0.0f, srcW = 0.0f, srcH = 0.0f;
};
