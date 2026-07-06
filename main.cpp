#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <memory>

#include "engine/Scene.h"
#include "engine/Debugger.h"

#include "game/Platformer.h"
#include "game/TopDown.h"
#include "game/Shooter.h"

int main(int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Error al inicializar SDL: %s", SDL_GetError());
        return 1;
    }
    // SDL3_ttf necesita inicializarse una vez (SDL3_image no lo requiere). El HUD del
    // shooter usa fuentes; sin esto TTF_OpenFont fallaria.
    if (!TTF_Init()) {
        SDL_Log("Error al inicializar SDL_ttf: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("Ejemplo 1: Platformer  (1/2/3 cambia, F1 debug)", 1280, 720, 0);
    if (!window) { SDL_Quit(); return 1; }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) { SDL_DestroyWindow(window); SDL_Quit(); return 1; }

    auto scene = std::make_unique<Scene>(renderer);
    buildPlatformer(*scene);
    int current = 1;

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
                if (event.key.scancode == SDL_SCANCODE_F1) Debug::toggle();

                int sel = 0;
                if (event.key.scancode == SDL_SCANCODE_1) sel = 1;
                if (event.key.scancode == SDL_SCANCODE_2) sel = 2;
                if (event.key.scancode == SDL_SCANCODE_3) sel = 3;

                if (sel != 0 && sel != current) {
                    current = sel;
                    scene = std::make_unique<Scene>(renderer);
                    if (sel == 1) { buildPlatformer(*scene); SDL_SetWindowTitle(window, "Ejemplo 1: Platformer  (1/2/3 cambia, F1 debug)"); }
                    if (sel == 2) { buildTopDown(*scene);    SDL_SetWindowTitle(window, "Ejemplo 2: Top-down  (1/2/3 cambia, F1 debug)"); }
                    if (sel == 3) { buildShooter(*scene);    SDL_SetWindowTitle(window, "Ejemplo 3: Shooter  (1/2/3 cambia, F1 debug)"); }
                }
            }
        }

        scene->update(dt);

        SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
        SDL_RenderClear(renderer);
        scene->render();
        Debug::drawColliders(*scene);
        SDL_RenderPresent(renderer);
    }

    // Liberamos la escena (y con ella AssetManager: texturas y fuentes) ANTES de
    // cerrar TTF/SDL, para que TTF_CloseFont y SDL_DestroyTexture corran con las
    // librerias aun vivas.
    scene.reset();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
