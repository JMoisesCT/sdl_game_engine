#pragma once
#include <functional>
#include <string>
#include "Component.h"

class GameObject;
class SpriteAnimator;

// Objeto que se recoge al tocarlo: monedas, frutas, llaves, vidas extra. Necesita un
// BoxCollider con isTrigger = true (avisa pero no frena).
//
// El motor NO lleva la cuenta de nada: que significa recoger esto lo decide el juego
// con el callback. Asi el mismo componente vale para sumar puntos, abrir una puerta o
// curar al jugador.
//
//     auto c = fruta->addComponent<Collectible>();
//     c->collectorTag = "Player";
//     c->collectAnimation = "collected";      // clip SIN loop; opcional
//     c->onCollect = [hud](GameObject*) { hud->add(1); };
//
// Si se le da 'collectAnimation' y el objeto tiene SpriteAnimator, el objeto no se
// destruye en el acto: reproduce ese clip y se destruye cuando termina (via
// setOnComplete). Si no, se destruye inmediatamente.

class Collectible : public Component {
public:
    std::string collectorTag = "Player";   // quien puede recogerlo
    std::string collectAnimation;          // clip no-loop a reproducir antes de morir
    bool        destroyOnCollect = true;   // false = solo avisa y se queda (interruptores)

    std::function<void(GameObject* collector)> onCollect;

    bool isTaken() const { return taken; }

    void start() override;
    void onCollision(GameObject* other) override;

private:
    SpriteAnimator* anim = nullptr;
    bool taken = false; // una sola vez, aunque el solape dure varios frames
};
