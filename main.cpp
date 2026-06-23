#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <memory>
#include <cstdlib>

#include "engine/Scene.h"
#include "engine/GameObject.h"
#include "engine/SpriteRenderer.h"
#include "engine/SpriteAnimator.h"
#include "engine/Transform.h"
#include "engine/Camera.h"
#include "engine/FollowCamera.h"
#include "engine/RigidBody2D.h"
#include "engine/BoxCollider.h"
#include "engine/Lifetime.h"
#include "engine/Spawner.h"
#include "engine/Debugger.h"

// ============================================================================
//  Componentes de juego (especificos del ejemplo, NO del motor).
//  Detectan el flanco de tecla con el estado del teclado + memoria del frame
//  anterior, asi no dependen de los eventos del bucle.
// ============================================================================

// Destruye su objeto (y al otro) cuando choca con algo de cierto nombre.
class DestroyOnHit : public Component {
public:
    std::string targetName;
    void onCollision(GameObject* other) override {
        if (other->name == targetName) {
            gameObject->scene->destroy(gameObject);
            gameObject->scene->destroy(other);
        }
    }
};

// --- Platformer: izquierda/derecha + salto con espacio ---
class PlatformerController : public Component {
public:
    float speed = 250.0f, jump = 650.0f;
    void update(float) override {
        const bool* keys = SDL_GetKeyboardState(nullptr);
        auto rb     = gameObject->getComponent<RigidBody2D>();
        auto sprite = gameObject->getComponent<SpriteRenderer>();
        auto anim   = gameObject->getComponent<SpriteAnimator>();

        float moveX = 0.0f;
        if (keys[SDL_SCANCODE_LEFT])  moveX -= 1.0f;
        if (keys[SDL_SCANCODE_RIGHT]) moveX += 1.0f;
        if (rb) rb->velocityX = moveX * speed;

        bool jumpNow = keys[SDL_SCANCODE_SPACE];
        if (rb && jumpNow && !jumpPrev && rb->grounded) rb->velocityY = -jump;
        jumpPrev = jumpNow;

        if (sprite) { if (moveX < 0) sprite->flipX = true; else if (moveX > 0) sprite->flipX = false; }
        if (anim) anim->play(moveX != 0.0f ? "walk" : "idle");
    }
private:
    bool jumpPrev = false;
};

// --- Top-down: movimiento libre en 4 direcciones (sin gravedad) ---
class TopDownController : public Component {
public:
    float speed = 220.0f;
    void update(float) override {
        const bool* keys = SDL_GetKeyboardState(nullptr);
        auto rb     = gameObject->getComponent<RigidBody2D>();
        auto sprite = gameObject->getComponent<SpriteRenderer>();
        auto anim   = gameObject->getComponent<SpriteAnimator>();

        float mx = 0.0f, my = 0.0f;
        if (keys[SDL_SCANCODE_LEFT])  mx -= 1.0f;
        if (keys[SDL_SCANCODE_RIGHT]) mx += 1.0f;
        if (keys[SDL_SCANCODE_UP])    my -= 1.0f;
        if (keys[SDL_SCANCODE_DOWN])  my += 1.0f;
        if (rb) { rb->velocityX = mx * speed; rb->velocityY = my * speed; }

        if (sprite) { if (mx < 0) sprite->flipX = true; else if (mx > 0) sprite->flipX = false; }
        if (anim) anim->play((mx != 0.0f || my != 0.0f) ? "walk" : "idle");
    }
};

// --- Shooter: mover horizontal + disparar balas que suben ---
class ShooterController : public Component {
public:
    float speed = 260.0f;
    void update(float) override {
        const bool* keys = SDL_GetKeyboardState(nullptr);
        auto rb = gameObject->getComponent<RigidBody2D>();

        float mx = 0.0f;
        if (keys[SDL_SCANCODE_LEFT])  mx -= 1.0f;
        if (keys[SDL_SCANCODE_RIGHT]) mx += 1.0f;
        if (rb) rb->velocityX = mx * speed;

        bool shootNow = keys[SDL_SCANCODE_SPACE];
        if (shootNow && !shootPrev) shoot();
        shootPrev = shootNow;
    }
private:
    bool shootPrev = false;
    void shoot() {
        Scene* scene = gameObject->scene;
        GameObject* bala = scene->createGameObject("Bala");
        bala->transform->x = gameObject->transform->x;
        bala->transform->y = gameObject->transform->y - 40.0f;
        bala->transform->scaleX = bala->transform->scaleY = 1.5f;

        auto s = bala->addComponent<SpriteRenderer>("assets/cuadrado.png");
        s->setSourceRect(0, 0, 32, 32);
        auto rb = bala->addComponent<RigidBody2D>();
        rb->gravityScale = 0.0f;
        rb->velocityY = -500.0f; // sube
        auto c = bala->addComponent<BoxCollider>();
        c->width = c->height = 24.0f;
        c->isTrigger = true;
        bala->addComponent<Lifetime>()->seconds = 2.0f;            // se autodestruye
        bala->addComponent<DestroyOnHit>()->targetName = "Enemigo";
    }
};

// ============================================================================
//  Constructores de cada ejemplo: pueblan una escena vacia.
// ============================================================================

static void buildPlatformer(Scene& scene) {
    GameObject* player = scene.createGameObject("Player");
    player->transform->y = -150.0f;
    player->transform->scaleX = player->transform->scaleY = 4.0f;
    player->addComponent<SpriteRenderer>("assets/personaje.png");
    auto anim = player->addComponent<SpriteAnimator>(32, 32, 8);
    anim->addAnimation("idle", {0, 1, 2, 3}, 6.0f);
    anim->addAnimation("walk", {8, 9, 10, 11, 12, 13, 14, 15}, 10.0f);
    player->addComponent<RigidBody2D>(); // con gravedad
    auto col = player->addComponent<BoxCollider>();
    col->width = 64.0f; col->height = 110.0f; col->offsetY = 8.0f; // ajustado al cuerpo
    player->addComponent<PlatformerController>();

    GameObject* suelo = scene.createGameObject("Suelo");
    suelo->transform->y = 300.0f;
    suelo->transform->scaleX = 60.0f; suelo->transform->scaleY = 3.0f;
    auto ss = suelo->addComponent<SpriteRenderer>("assets/cuadrado.png");
    ss->setSourceRect(0, 0, 32, 32);
    auto sc = suelo->addComponent<BoxCollider>();
    sc->width = 32.0f * 60.0f; sc->height = 32.0f * 3.0f;

    GameObject* cam = scene.createGameObject("MainCamera");
    cam->addComponent<Camera>();
    auto f = cam->addComponent<FollowCamera>();
    f->setTarget(player);
    f->deadZoneWidth = 200.0f; f->deadZoneHeight = 200.0f;
}

static void buildTopDown(Scene& scene) {
    GameObject* player = scene.createGameObject("Player");
    player->transform->scaleX = player->transform->scaleY = 4.0f;
    player->addComponent<SpriteRenderer>("assets/personaje.png");
    auto anim = player->addComponent<SpriteAnimator>(32, 32, 8);
    anim->addAnimation("idle", {0, 1, 2, 3}, 6.0f);
    anim->addAnimation("walk", {8, 9, 10, 11, 12, 13, 14, 15}, 10.0f);
    auto rb = player->addComponent<RigidBody2D>();
    rb->gravityScale = 0.0f; // cenital: sin gravedad
    auto col = player->addComponent<BoxCollider>();
    col->width = 64.0f; col->height = 100.0f; col->offsetY = 10.0f;
    player->addComponent<TopDownController>();

    for (int i = 0; i < 4; ++i) {
        GameObject* w = scene.createGameObject("Pared");
        w->transform->x = -300.0f + i * 200.0f;
        w->transform->y = 150.0f;
        w->transform->scaleX = w->transform->scaleY = 3.0f;
        auto sr = w->addComponent<SpriteRenderer>("assets/cuadrado.png");
        sr->setSourceRect(0, 0, 32, 32);
        auto c = w->addComponent<BoxCollider>();
        c->width = c->height = 96.0f;
    }

    GameObject* cam = scene.createGameObject("MainCamera");
    cam->addComponent<Camera>();
    auto f = cam->addComponent<FollowCamera>();
    f->setTarget(player);
    f->deadZoneWidth = 150.0f; f->deadZoneHeight = 150.0f;
}

static void buildShooter(Scene& scene) {
    GameObject* player = scene.createGameObject("Player");
    player->transform->y = 250.0f;
    player->transform->scaleX = player->transform->scaleY = 3.0f;
    auto sr = player->addComponent<SpriteRenderer>("assets/personaje.png");
    sr->setSourceRect(0, 0, 32, 32); // un frame fijo, como nave
    auto rb = player->addComponent<RigidBody2D>();
    rb->gravityScale = 0.0f;
    auto col = player->addComponent<BoxCollider>();
    col->width = 60.0f; col->height = 60.0f;
    player->addComponent<ShooterController>();

    GameObject* spawner = scene.createGameObject("EnemySpawner");
    auto sp = spawner->addComponent<Spawner>();
    sp->interval = 1.0f;
    sp->spawn = [](Scene& s) {
        GameObject* e = s.createGameObject("Enemigo");
        e->transform->x = (float)((std::rand() % 800) - 400);
        e->transform->y = -300.0f;
        e->transform->scaleX = e->transform->scaleY = 2.5f;
        auto sr = e->addComponent<SpriteRenderer>("assets/cuadrado.png");
        sr->setSourceRect(0, 0, 32, 32);
        auto rb = e->addComponent<RigidBody2D>();
        rb->gravityScale = 0.4f; // caen lento
        auto c = e->addComponent<BoxCollider>();
        c->width = c->height = 80.0f;
        c->isTrigger = true;
        e->addComponent<Lifetime>()->seconds = 6.0f;            // se limpian al salir
        e->addComponent<DestroyOnHit>()->targetName = "Bala";
    };

    // Camara fija (sin FollowCamera): la vista no se mueve, estilo shoot'em up.
    GameObject* cam = scene.createGameObject("MainCamera");
    cam->addComponent<Camera>();
}

// ============================================================================
//  main: un solo bucle que carga cualquiera de los tres ejemplos.
// ============================================================================

int main(int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Error al inicializar SDL: %s", SDL_GetError());
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
                    scene = std::make_unique<Scene>(renderer); // recrear de cero
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

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
