#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "engine/Scene.h"
#include "engine/GameObject.h"
#include "engine/SpriteRenderer.h"
#include "engine/SpriteAnimator.h"
#include "engine/Transform.h"
#include "engine/Camera.h"
#include "engine/FollowCamera.h"

int main(int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Error al inicializar SDL: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Mi Motor SDL3", 1280, 720, 0);
    if (!window) {
        SDL_Log("Error al crear la ventana: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        SDL_Log("Error al crear el renderer: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Scene scene(renderer);

    // ---- Personaje animado ----
    // Spritesheet 256x160, frames de 32x32  =>  8 columnas y 5 filas (celdas 0..39).
    GameObject* player = scene.createGameObject("Player");
    player->transform->x = 0.0f;
    player->transform->y = 0.0f;
    player->transform->scaleX = 4.0f;
    player->transform->scaleY = 4.0f;

    SpriteRenderer* playerSprite = player->addComponent<SpriteRenderer>("assets/personaje.png");

    SpriteAnimator* anim = player->addComponent<SpriteAnimator>(32, 32, 8);
    anim->addAnimation("idle", {0, 1, 2, 3}, 6.0f);             // fila 0
    anim->addAnimation("walk", {8, 9, 10, 11, 12, 13, 14, 15}, 10.0f);  // fila 1

    // ---- Direccion inicial ----
    // Se asume que el arte esta dibujado mirando a la DERECHA.
    // Cambia a true si quieres que el personaje arranque mirando a la izquierda.
    bool facingLeft = true;
    playerSprite->flipX = facingLeft;
    anim->play("idle");

    // ---- NPC quieto de referencia (mismo spritesheet, sale de cache) ----
    GameObject* npc = scene.createGameObject("NPC");
    npc->transform->x = 300.0f;
    npc->transform->scaleX = 4.0f;
    npc->transform->scaleY = 4.0f;
    SpriteRenderer* npcSprite = npc->addComponent<SpriteRenderer>("assets/personaje.png");
    npcSprite->setSourceRect(0, 0, 32, 32);

    // ---- Camara que sigue al player con zona muerta ----
    GameObject* camara = scene.createGameObject("MainCamera");
    camara->addComponent<Camera>();
    FollowCamera* follow = camara->addComponent<FollowCamera>();
    follow->setTarget(player);
    follow->deadZoneWidth  = 200.0f;
    follow->deadZoneHeight = 150.0f;
    follow->smoothSpeed    = 5.0f;

    // ---- Bucle principal ----
    const float SPEED = 200.0f; // velocidad del player en px/seg
    bool running = true;
    Uint64 lastTime = SDL_GetTicks();

    while (running) {
        Uint64 now = SDL_GetTicks();
        float dt = (now - lastTime) / 1000.0f;
        lastTime = now;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
        }

        // ---- Teclado: estado actual de todas las teclas (SDL3 => const bool*) ----
        const bool* keys = SDL_GetKeyboardState(nullptr);

        float moveX = 0.0f;
        float moveY = 0.0f;
        if (keys[SDL_SCANCODE_LEFT])  moveX -= 1.0f;
        if (keys[SDL_SCANCODE_RIGHT]) moveX += 1.0f;
        if (keys[SDL_SCANCODE_UP])    moveY -= 1.0f;
        if (keys[SDL_SCANCODE_DOWN])  moveY += 1.0f;

        // Mover al personaje
        player->transform->x += moveX * SPEED * dt;
        player->transform->y += moveY * SPEED * dt;

        // Flip segun hacia donde camina (solo cambia si hay movimiento horizontal)
        if      (moveX < 0.0f) facingLeft = false;
        else if (moveX > 0.0f) facingLeft = true;
        playerSprite->flipX = facingLeft;

        // Animacion: caminar si se mueve, quieto si no
        bool moving = (moveX != 0.0f || moveY != 0.0f);
        anim->play(moving ? "walk" : "idle");

        scene.update(dt);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);
        scene.render();
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
