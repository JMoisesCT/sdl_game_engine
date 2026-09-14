#pragma once
#include <functional>
#include <string>
#include "Component.h"

class GameObject;
class SpriteAnimator;

// Bandera del mundo que, al tocarla, le fija al jugador su punto de reaparicion.
// Necesita un BoxCollider con isTrigger = true. El jugador tiene que llevar un
// componente Respawn: este componente no guarda nada, solo se lo dice.
//
//     auto cp = bandera->addComponent<Checkpoint>();
//     cp->targetTag = "Player";
//     cp->activateAnimation = "flagOut";   // opcional
//     cp->idleAnimation     = "flagIdle";  // opcional, tras la de activacion
//
// Se activa UNA sola vez. El punto que fija es el del propio checkpoint mas el
// desplazamiento indicado, por si la bandera esta dibujada con el pie en otro sitio.

class Checkpoint : public Component {
public:
    std::string targetTag = "Player";

    float offsetX = 0.0f; // corrimiento del punto respecto al centro del checkpoint
    float offsetY = 0.0f;

    std::string activateAnimation; // clip SIN loop que suena al activarlo
    std::string idleAnimation;     // clip en loop al que se pasa cuando acaba el anterior

    std::function<void()> onActivate; // por si el juego quiere sonido o HUD

    bool isActivated() const { return activated; }

    void start() override;
    void onCollision(GameObject* other) override;

private:
    SpriteAnimator* anim = nullptr;
    bool activated = false;
};
