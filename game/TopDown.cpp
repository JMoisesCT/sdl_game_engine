#include "TopDown.h"

#include <SDL3/SDL.h>

#include "../engine/Scene.h"
#include "../engine/GameObject.h"
#include "../engine/Component.h"
#include "../engine/Transform.h"
#include "../engine/SpriteRenderer.h"
#include "../engine/SpriteAnimator.h"
#include "../engine/RigidBody2D.h"
#include "../engine/BoxCollider.h"
#include "../engine/Camera.h"
#include "../engine/FollowCamera.h"

// Movimiento libre en 4 direcciones sin gravedad (vista cenital).
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

void buildTopDown(Scene& scene) {
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
