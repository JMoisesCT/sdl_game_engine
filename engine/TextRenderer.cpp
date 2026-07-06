#include "TextRenderer.h"
#include "GameObject.h"
#include "Transform.h"
#include "Scene.h"
#include "Camera.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

void TextRenderer::setFont(TTF_Font* f) {
    if (font == f) return;
    font = f;
    dirty = true; // otra fuente => otra textura
}

void TextRenderer::setText(const std::string& t) {
    if (text == t) return; // mismo texto: no ensuciamos (evita regenerar cada frame)
    text = t;
    dirty = true;
}

void TextRenderer::setColor(TextColor c) {
    if (c.r == color.r && c.g == color.g && c.b == color.b && c.a == color.a) return;
    color = c;
    dirty = true;
}

void TextRenderer::releaseTexture() {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    texW = texH = 0.0f;
}

void TextRenderer::rebuildTexture(SDL_Renderer* renderer) {
    // Siempre soltamos la textura anterior antes de generar la nueva.
    releaseTexture();
    dirty = false;

    if (!font || text.empty()) return; // sin fuente o sin texto: no hay nada que dibujar

    SDL_Color fg{ color.r, color.g, color.b, color.a };
    // SDL3_ttf: TTF_RenderText_Solid recibe un 'length' extra; 0 = cadena terminada
    // en null. _Solid (sin antialiasing) para que la fuente pixel quede nitida.
    SDL_Surface* surface = TTF_RenderText_Solid(font, text.c_str(), 0, fg);
    if (!surface) {
        SDL_Log("TextRenderer: no se pudo renderizar el texto '%s': %s", text.c_str(), SDL_GetError());
        return;
    }

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    if (!texture) {
        SDL_Log("TextRenderer: no se pudo crear la textura del texto: %s", SDL_GetError());
        return;
    }

    // Pixel art nitido: muestreo por vecino mas cercano, igual que las texturas.
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDL_GetTextureSize(texture, &texW, &texH);
}

void TextRenderer::render() {
    SDL_Renderer* renderer = gameObject->scene->getRenderer();

    // Dirty flag: solo regeneramos la textura cuando el texto/color/fuente cambio.
    if (dirty) rebuildTexture(renderer);
    if (!texture) return;

    Transform* t = gameObject->transform;
    Camera* cam = gameObject->scene->getActiveCamera();

    // Centro del texto en pantalla. En screenSpace ignoramos la camara (HUD fijo);
    // si no, el Transform es mundo y lo pasamos por la camara con su zoom.
    float centerX, centerY;
    float zoom = 1.0f;
    if (!screenSpace && cam) {
        cam->worldToScreen(t->x, t->y, centerX, centerY);
        zoom = cam->getZoom();
    } else {
        centerX = t->x;
        centerY = t->y;
    }

    float drawW = texW * zoom;
    float drawH = texH * zoom;

    // Anclado al centro y en coordenadas ENTERAS (nitidez de pixel art).
    SDL_FRect dst;
    dst.w = drawW;
    dst.h = drawH;
    dst.x = SDL_roundf(centerX - drawW * 0.5f);
    dst.y = SDL_roundf(centerY - drawH * 0.5f);

    SDL_RenderTexture(renderer, texture, nullptr, &dst);
}

TextRenderer::~TextRenderer() {
    releaseTexture(); // liberamos nuestra textura al destruirnos
}
