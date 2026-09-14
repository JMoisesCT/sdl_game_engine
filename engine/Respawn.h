#pragma once
#include <functional>
#include "Component.h"

class RigidBody2D;
class Health;

// Devuelve a su objeto al ultimo punto de reaparicion. Va en el JUGADOR (el
// Checkpoint del mundo es otro componente, y lo unico que hace es fijarle el punto).
//
// El punto inicial es la posicion que tenga el objeto en su start(), asi que si no hay
// ningun checkpoint el jugador reaparece donde empezo el nivel.
//
// MUERTE EN DOS TIEMPOS: die() no teletransporta en el acto; marca al objeto como
// "muriendo" y espera 'delay' segundos. Eso deja ver la animacion de muerte y evita que
// el jugador reaparezca en el mismo frame en que lo mataron. Mientras tanto se le
// congela la velocidad para que el cadaver no siga deslizandose.
//
//     auto resp = player->addComponent<Respawn>();
//     health->onDeath = [resp] { resp->die(); };   // asi se enchufa con Health

class Respawn : public Component {
public:
    float delay = 0.6f; // segundos entre la muerte y la reaparicion

    std::function<void()> onRespawn; // por si el juego quiere reaccionar (HUD, camara)

    // Cambia el punto de reaparicion (lo llama el Checkpoint).
    void setPoint(float x, float y);
    float getPointX() const { return pointX; }
    float getPointY() const { return pointY; }

    // Arranca la cuenta atras de reaparicion. Llamarlo desde Health::onDeath.
    void die();
    bool isDying() const { return dying; }

    // Teletransporta ya, sin esperar: coloca en el punto, pone la velocidad a cero y
    // devuelve la vida al maximo.
    void respawnNow();

    void start() override;
    void update(float dt) override;

private:
    RigidBody2D* rb = nullptr;
    Health*      health = nullptr;

    float pointX = 0.0f, pointY = 0.0f;
    bool  dying = false;
    float timer = 0.0f;
};
