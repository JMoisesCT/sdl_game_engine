#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <memory>

#include "engine/Scene.h"
#include "engine/Debugger.h"
#include "engine/Input.h"

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

    // Reloj en NANOsegundos. SDL_GetTicks() tiene resolucion de MILIsegundos y, con la
    // ventana corriendo a framerate alto, muchos frames salian con dt == 0: el
    // RigidBody2D no integraba, no habia penetracion contra el suelo y la fase de
    // fisica no detectaba el apoyo (de ahi el parpadeo del "grounded").
    Uint64 lastTime = SDL_GetTicksNS();

    // Techo del paso de tiempo. Si un frame tarda mas que esto (arrastrar la ventana,
    // parar en el depurador, minimizar), lo tratamos como 50 ms: preferimos un instante
    // en camara lenta antes que un salto de cientos de pixeles que atraviese las paredes.
    const float MAX_DT = 0.05f;

    while (running) {
        Uint64 now = SDL_GetTicksNS();
        float dt = static_cast<float>(now - lastTime) / 1000000000.0f;
        lastTime = now;
        if (dt > MAX_DT) dt = MAX_DT;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
        }

        // Estado de teclado y raton para ESTE frame. Va despues del bucle de eventos
        // (SDL_PollEvent es lo que refresca el estado interno de SDL) y antes de
        // actualizar la escena, para que todos los componentes lean lo mismo.
        Input::update();

        if (Input::wasPressed(Key::F1)) Debug::toggle();

        int sel = 0;
        if (Input::wasPressed(Key::Num1)) sel = 1;
        if (Input::wasPressed(Key::Num2)) sel = 2;
        if (Input::wasPressed(Key::Num3)) sel = 3;

        if (sel != 0 && sel != current) {
            current = sel;
            scene = std::make_unique<Scene>(renderer);
            if (sel == 1) { buildPlatformer(*scene); SDL_SetWindowTitle(window, "Ejemplo 1: Platformer  (1/2/3 cambia, F1 debug)"); }
            if (sel == 2) { buildTopDown(*scene);    SDL_SetWindowTitle(window, "Ejemplo 2: Top-down  (1/2/3 cambia, F1 debug)"); }
            if (sel == 3) { buildShooter(*scene);    SDL_SetWindowTitle(window, "Ejemplo 3: Shooter  (1/2/3 cambia, F1 debug)"); }
            // La escena nueva arranca sin flancos pendientes: si no, la tecla que
            // todavia esta apretada se leeria como "recien presionada" ahi tambien.
            Input::reset();
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
