#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "engine/Scene.h"
#include "engine/GameObject.h"
#include "engine/SpriteRenderer.h"
#include "engine/SpriteAnimator.h"
#include "engine/Transform.h"
#include "engine/Camera.h"
#include "engine/FollowCamera.h"
#include "engine/RigidBody2D.h"
#include "engine/BoxCollider.h"
#include "engine/Debugger.h"   // <-- opcional: quitar este include y las llamadas Debug:: lo elimina

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

    // ---- Player con fisica ----
    GameObject* player = scene.createGameObject("Player");
    player->transform->x = 0.0f;
    player->transform->y = -150.0f;
    player->transform->scaleX = 4.0f;
    player->transform->scaleY = 4.0f;

    SpriteRenderer* playerSprite = player->addComponent<SpriteRenderer>("assets/personaje.png");

    SpriteAnimator* anim = player->addComponent<SpriteAnimator>(32, 32, 8);
    anim->addAnimation("idle", {0, 1, 2, 3}, 6.0f);
    anim->addAnimation("walk", {8, 9, 10, 11, 12, 13, 14, 15}, 10.0f);

    RigidBody2D* rb = player->addComponent<RigidBody2D>();

    BoxCollider* col = player->addComponent<BoxCollider>();
    col->width  = 32.0f * 4.0f;
    col->height = 32.0f * 4.0f;

    bool facingLeft = true;
    playerSprite->flipX = facingLeft;

    // ---- Suelo ----
    GameObject* suelo = scene.createGameObject("Suelo");
    suelo->transform->x = 0.0f;
    suelo->transform->y = 300.0f;
    suelo->transform->scaleX = 60.0f;
    suelo->transform->scaleY = 3.0f;
    SpriteRenderer* sueloSprite = suelo->addComponent<SpriteRenderer>("assets/pixel_adventure/Terrain/Terrain (16x16).png");
    sueloSprite->setSourceRect(0, 0, 16, 16);
    BoxCollider* sueloCol = suelo->addComponent<BoxCollider>();
    sueloCol->width = 16.0f * 60.0f;
    sueloCol->height = 16.0f * 3.0f;

    // ---- Camara ----
    GameObject* camara = scene.createGameObject("MainCamera");
    camara->addComponent<Camera>();
    FollowCamera* follow = camara->addComponent<FollowCamera>();
    follow->setTarget(player);
    follow->deadZoneWidth  = 200.0f;
    follow->deadZoneHeight = 200.0f;

    Debug::setEnabled(true); // arranca mostrando colliders; F1 lo prende/apaga

    // ---- Bucle principal ----
    const float SPEED = 250.0f;
    bool running = true;
    Uint64 lastTime = SDL_GetTicks();

    while (running) {
        Uint64 now = SDL_GetTicks();
        float dt = (now - lastTime) / 1000.0f;
        lastTime = now;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;

            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                if (event.key.scancode == SDL_SCANCODE_SPACE && rb->grounded)
                    rb->velocityY = -650.0f;
                if (event.key.scancode == SDL_SCANCODE_F1)
                    Debug::toggle(); // prender/apagar el debug en caliente
            }
        }

        const bool* keys = SDL_GetKeyboardState(nullptr);
        float moveX = 0.0f;
        if (keys[SDL_SCANCODE_LEFT])  moveX -= 1.0f;
        if (keys[SDL_SCANCODE_RIGHT]) moveX += 1.0f;

        rb->velocityX = moveX * SPEED;

        if      (moveX < 0.0f) facingLeft = false;
        else if (moveX > 0.0f) facingLeft = true;
        playerSprite->flipX = facingLeft;

        anim->play(moveX != 0.0f ? "walk" : "idle");

        scene.update(dt);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);
        scene.render();

        // ---- Debug opcional (se dibuja encima de la escena) ----
        Debug::drawColliders(scene);
        Debug::drawDeadZone(scene, follow);

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
