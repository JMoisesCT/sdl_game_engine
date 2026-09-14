#include "ParallaxBackground.h"
#include "GameObject.h"
#include "Transform.h"
#include "Scene.h"
#include "Camera.h"

#include <SDL3/SDL.h>
#include <cmath>

ParallaxBackground::ParallaxBackground(std::string imagePath)
    : path(std::move(imagePath)) {}

void ParallaxBackground::awake() {
    texture = gameObject->scene->getAssets().loadTexture(path);
    if (texture) SDL_GetTextureSize(texture, &texW, &texH); // SDL3: floats
}

void ParallaxBackground::update(float dt) {
    autoX += scrollSpeedX * dt;
    autoY += scrollSpeedY * dt;
}

void ParallaxBackground::render() {
    if (!texture || texW <= 0.0f || texH <= 0.0f) return;

    SDL_Renderer* renderer = gameObject->scene->getRenderer();
    int outW = 0, outH = 0;
    SDL_GetCurrentRenderOutputSize(renderer, &outW, &outH);

    float tileW = texW * scale;
    float tileH = texH * scale;
    if (tileW <= 0.0f || tileH <= 0.0f) return;

    // Desplazamiento del mosaico: la camara arrastra el fondo solo en su 'factor'.
    float camX = 0.0f, camY = 0.0f;
    if (Camera* cam = gameObject->scene->getActiveCamera()) {
        camX = cam->gameObject->transform->x;
        camY = cam->gameObject->transform->y;
    }
    float offX = -(camX * factorX + autoX);
    float offY = -(camY * factorY + autoY);

    // Llevar el desplazamiento al rango [-tileW, 0): asi el primer mosaico empieza
    // justo antes del borde izquierdo y el patron se repite sin huecos ni saltos.
    offX = std::fmod(offX, tileW); if (offX > 0.0f) offX -= tileW;
    offY = std::fmod(offY, tileH); if (offY > 0.0f) offY -= tileH;

    // Coordenadas enteras, igual que en el tilemap: con pixel art, medio pixel de
    // desfase entre dos copias del mosaico se ve como una costura.
    for (float y = offY; y < outH; y += tileH) {
        for (float x = offX; x < outW; x += tileW) {
            float left = std::round(x), top = std::round(y);
            SDL_FRect dst{ left, top,
                           std::round(x + tileW) - left,
                           std::round(y + tileH) - top };
            SDL_RenderTexture(renderer, texture, nullptr, &dst);
        }
    }
}
