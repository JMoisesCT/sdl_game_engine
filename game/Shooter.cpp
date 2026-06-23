#include "Shooter.h"

#include <SDL3/SDL.h>
#include <cstdlib>

#include "../engine/Scene.h"
#include "../engine/GameObject.h"
#include "../engine/Component.h"
#include "../engine/Transform.h"
#include "../engine/SpriteRenderer.h"
#include "../engine/RigidBody2D.h"
#include "../engine/BoxCollider.h"
#include "../engine/Camera.h"
#include "../engine/Lifetime.h"
#include "../engine/Spawner.h"

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

// Mueve la nave horizontalmente y dispara balas con espacio.
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
        bala->addComponent<Lifetime>()->seconds = 2.0f;
        bala->addComponent<DestroyOnHit>()->targetName = "Enemigo";
    }
};

void buildShooter(Scene& scene) {
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
        e->addComponent<Lifetime>()->seconds = 6.0f;
        e->addComponent<DestroyOnHit>()->targetName = "Bala";
    };

    // Camara fija: la vista no se mueve, estilo shoot'em up.
    GameObject* cam = scene.createGameObject("MainCamera");
    cam->addComponent<Camera>();
}
