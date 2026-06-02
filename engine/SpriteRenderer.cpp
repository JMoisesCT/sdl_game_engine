#include "SpriteRenderer.h"
#include "GameObject.h"
#include "Transform.h"
#include "Scene.h"

#include <SDL3/SDL.h>

SpriteRenderer::SpriteRenderer(std::string imagePath)
    : path(std::move(imagePath)) {}

void SpriteRenderer::awake() {
    // Pedimos la textura al AssetManager de la escena: la carga una sola vez
    // y la comparte con cualquier otro objeto que use la misma imagen.
    texture = gameObject->scene->getAssets().loadTexture(path);
    if (texture) {
        SDL_GetTextureSize(texture, &width, &height); // tamano original (px)
    }
}

void SpriteRenderer::render() {
    if (!texture) return;

    SDL_Renderer* renderer = gameObject->scene->getRenderer();
    Transform* t = gameObject->transform;

    SDL_FRect dst;
    dst.x = t->x;
    dst.y = t->y;
    dst.w = width  * t->scaleX;
    dst.h = height * t->scaleY;

    SDL_RenderTextureRotated(renderer, texture, nullptr, &dst,
                             t->rotation, nullptr, SDL_FLIP_NONE);
}
