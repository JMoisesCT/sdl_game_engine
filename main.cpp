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
    // Spritesheet 256x160, cada frame 32x32  =>  8 columnas y 5 filas (40 celdas).
    // Celdas por fila: fila 0 = 0..7, fila 1 = 8..15, fila 2 = 16..23, ...
    GameObject* player = scene.createGameObject("Player");
    player->transform->x = 0.0f;
    player->transform->y = 0.0f;
    player->transform->scaleX = 4.0f; // 32x32 se ve chico: lo agrandamos x4
    player->transform->scaleY = 4.0f;

    player->addComponent<SpriteRenderer>("assets/sheet.png");

    SpriteAnimator* anim = player->addComponent<SpriteAnimator>(32, 32, 8);
    anim->addAnimation("idle", {0, 1, 2, 3, 4}, 6.0f);             // fila 0
    anim->addAnimation("walk", {8, 9, 10, 11, 12, 13, 14, 15}, 10.0f);  // fila 1
    anim->play("walk"); // ajusta estos indices al orden real de TU hoja

    // ---- NPC quieto de referencia: mismo spritesheet (sale de cache), un frame fijo ----
    GameObject* npc = scene.createGameObject("NPC");
    npc->transform->x = 300.0f;
    npc->transform->scaleX = 4.0f;
    npc->transform->scaleY = 4.0f;
    SpriteRenderer* npcSprite = npc->addComponent<SpriteRenderer>("assets/sheet.png");
    npcSprite->setSourceRect(0, 0, 32, 32); // solo el primer frame, sin animar

    // ---- Camara que sigue al player con zona muerta ----
    GameObject* camara = scene.createGameObject("MainCamera");
    camara->addComponent<Camera>();
    FollowCamera* follow = camara->addComponent<FollowCamera>();
    follow->setTarget(player);
    follow->deadZoneWidth  = 200.0f; // el player puede moverse 100 px a cada lado
    follow->deadZoneHeight = 150.0f; // sin que la camara reaccione
    follow->smoothSpeed    = 5.0f;   // pon 0 para seguimiento duro

    // ---- Bucle principal ----
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

        // Movimiento de prueba (mas adelante vendra del teclado del alumno).
        player->transform->x += 120.0f * dt;

        scene.update(dt);

        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
        SDL_RenderClear(renderer);
        scene.render();
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
