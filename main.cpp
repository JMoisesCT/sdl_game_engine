#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h> // deja que SDL prepare el main por nosotros

#include "engine/Scene.h"
#include "engine/GameObject.h"
#include "engine/SpriteRenderer.h"
#include "engine/Transform.h"

int main(int argc, char* argv[]) {
    // ---- Init de SDL: esto ya lo conocen tus alumnos ----
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

    // ---- Aqui empieza a usarse el motor ----
    Scene scene(renderer);

    GameObject* player = scene.createGameObject("Player");
    player->transform->x = 0.0f;
    player->transform->y = 0.0f;
    player->addComponent<SpriteRenderer>("assets/bg.png"); // <-- cargar un sprite

    // ---- Bucle principal: el alumno sigue siendo el dueno ----
    bool running = true;
    Uint64 lastTime = SDL_GetTicks();

    while (running) {
        // Delta time en segundos
        Uint64 now = SDL_GetTicks();
        float dt = (now - lastTime) / 1000.0f;
        lastTime = now;

        // Eventos (igual que siempre en SDL3)
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
        }

        // Actualizar la logica de todos los objetos
        scene.update(dt);

        // Dibujar
        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255); // fondo
        SDL_RenderClear(renderer);

        scene.render(); // dibuja todos los sprites

        SDL_RenderPresent(renderer);
    }

    // ---- Limpieza ----
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
