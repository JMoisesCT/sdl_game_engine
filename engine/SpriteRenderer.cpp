#include "SpriteRenderer.h"
#include "GameObject.h"
#include "Transform.h"
#include "Scene.h"
#include "Camera.h"

#include <SDL3/SDL.h>

SpriteRenderer::SpriteRenderer(std::string imagePath)
    : path(std::move(imagePath)) {}

void SpriteRenderer::awake() {
    if (path.empty()) return; // sin textura inicial: la pondra el animator con setTexture
    texture = gameObject->scene->getAssets().loadTexture(path);
    if (texture) {
        SDL_GetTextureSize(texture, &width, &height);
    }
}

void SpriteRenderer::setTexture(SDL_Texture* tex) {
    texture = tex; // prestada: el dueno sigue siendo el AssetManager
    if (texture) {
        SDL_GetTextureSize(texture, &width, &height);
    }
}

void SpriteRenderer::render() {
    if (!texture) return;

    SDL_Renderer* renderer = gameObject->scene->getRenderer();
    Transform* t = gameObject->transform;
    Camera* cam = gameObject->scene->getActiveCamera();

    // Tamano base: el del frame recortado, o el de la imagen completa.
    float baseW = useSrcRect ? srcW : width;
    float baseH = useSrcRect ? srcH : height;

    float zoom = cam ? cam->getZoom() : 1.0f;
    float drawW = baseW * t->scaleX * zoom;
    float drawH = baseH * t->scaleY * zoom;

    // El Transform marca el CENTRO del sprite en el mundo.
    float centerX, centerY;
    if (cam) {
        cam->worldToScreen(t->x, t->y, centerX, centerY); // donde cae el centro en pantalla
    }
    else {
        centerX = t->x;
        centerY = t->y;
    }

    SDL_FRect dst;
    dst.w = drawW;
    dst.h = drawH;
    dst.x = centerX - drawW * 0.5f; // restamos medio sprite para centrarlo
    dst.y = centerY - drawH * 0.5f; // en el punto del Transform

    // Banderas simples -> modo de volteo de SDL.
    SDL_FlipMode flip = SDL_FLIP_NONE;
    if (flipX) flip = (SDL_FlipMode)(flip | SDL_FLIP_HORIZONTAL);
    if (flipY) flip = (SDL_FlipMode)(flip | SDL_FLIP_VERTICAL);

    SDL_FRect src{ srcX, srcY, srcW, srcH };
    const SDL_FRect* srcPtr = useSrcRect ? &src : nullptr;

    // center = nullptr => SDL rota alrededor del centro del dst, que ahora es el
    // centro real del sprite. Asi la rotacion tambien queda bien anclada.
    SDL_RenderTextureRotated(renderer, texture, srcPtr, &dst,
        t->rotation, nullptr, flip);
}