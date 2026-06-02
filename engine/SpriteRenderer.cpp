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

    SDL_FRect dst;
    if (cam) {
        // Con camara: mundo -> pantalla, y el zoom afecta tambien el tamano.
        cam->worldToScreen(t->x, t->y, dst.x, dst.y);
        dst.w = width  * t->scaleX * cam->getZoom();
        dst.h = height * t->scaleY * cam->getZoom();
    } else {
        // Sin camara: se dibuja en coordenadas de pantalla directas (como antes).
        dst.x = t->x;
        dst.y = t->y;
        dst.w = width  * t->scaleX;
        dst.h = height * t->scaleY;
    }

    SDL_RenderTextureRotated(renderer, texture, nullptr, &dst,
                             t->rotation, nullptr, SDL_FLIP_NONE);
}
