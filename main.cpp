#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "engine/Scene.h"
#include "engine/GameObject.h"
#include "engine/SpriteRenderer.h"
#include "engine/Transform.h"
#include "engine/Camera.h"

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

    // ---- Escena ----
    Scene scene(renderer);

    // Jugador (se movera para que se note la camara)
    GameObject* player = scene.createGameObject("Player");
    player->transform->x = 0.0f;
    player->transform->y = 0.0f;
    player->addComponent<SpriteRenderer>("assets/roca.png");

    // Objeto estatico de referencia
    GameObject* roca = scene.createGameObject("Roca");
    roca->transform->x = 400.0f;
    roca->transform->y = 0.0f;
    roca->addComponent<SpriteRenderer>("assets/nave.png");

    // Camara: su Transform es el punto que queda en el centro de la pantalla
    GameObject* camara = scene.createGameObject("MainCamera");
    camara->addComponent<Camera>();

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

        // El jugador avanza (mas adelante esto vendra del input del alumno)
        player->transform->x += 120.0f * dt;

        // La camara sigue al jugador: el jugador queda centrado y la roca se desplaza
        camara->transform->x = player->transform->x;
        camara->transform->y = player->transform->y;

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
