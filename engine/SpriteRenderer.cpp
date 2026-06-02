#include "SpriteRenderer.h"
#include "GameObject.h"
#include "Transform.h"
#include "Scene.h"
#include "Camera.h"

#include <SDL3/SDL.h>

SpriteRenderer::SpriteRenderer(std::string imagePath)
    : path(std::move(imagePath)) {}

void SpriteRenderer::awake() {
    texture = gameObject->scene->getAssets().loadTexture(path);
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

    SDL_FRect dst;
    if (cam) {
        cam->worldToScreen(t->x, t->y, dst.x, dst.y);
        dst.w = baseW * t->scaleX * cam->getZoom();
        dst.h = baseH * t->scaleY * cam->getZoom();
    } else {
        dst.x = t->x;
        dst.y = t->y;
        dst.w = baseW * t->scaleX;
        dst.h = baseH * t->scaleY;
    }

    if (useSrcRect) {
        SDL_FRect src{ srcX, srcY, srcW, srcH };
        SDL_RenderTextureRotated(renderer, texture, &src, &dst,
                                 t->rotation, nullptr, SDL_FLIP_NONE);
    } else {
        SDL_RenderTextureRotated(renderer, texture, nullptr, &dst,
                                 t->rotation, nullptr, SDL_FLIP_NONE);
    }
}
