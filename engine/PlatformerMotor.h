#pragma once
#include "Component.h"

class RigidBody2D;

// Movimiento de personaje de PLATAFORMAS: aceleracion y frenado horizontal, salto con
// altura variable, coyote time, jump buffer y doble salto. Es GENERICO (un genero, no
// un juego): no sabe que teclas existen ni toca SDL.
//
// COMO SE USA: el motor no lee el teclado. Cada frame el juego le escribe la INTENCION
// (moveInput, jumpPressed, jumpHeld) y el motor la traduce en velocidad. Asi el mismo
// componente sirve con teclado, con mando o con una IA que "pulsa" botones:
//
//     // en el update de un componente del juego:
//     motor->moveInput   = Input::axis(Key::Left, Key::Right);
//     motor->jumpPressed = Input::wasPressed(Key::Space);   // flanco, un solo frame
//     motor->jumpHeld    = Input::isDown(Key::Space);
//
// ORDEN DE COMPONENTES (importa: el GameObject actualiza en orden de insercion):
//     controlador del juego  ->  PlatformerMotor  ->  RigidBody2D  ->  BoxCollider
// El motor ESCRIBE velocityX/velocityY del RigidBody2D; el RigidBody2D las integra
// despues, y la fase de fisica de la Scene resuelve los choques y marca 'grounded'.
//
// Requiere un RigidBody2D en el mismo objeto, con gravedad (gravityScale > 0).

class PlatformerMotor : public Component {
public:
    // --- Intencion: la escribe el juego CADA frame -----------------------------
    float moveInput   = 0.0f;   // -1 izquierda, 0 quieto, +1 derecha (valen intermedios)
    bool  jumpPressed = false;  // el boton de salto SE ACABA de pulsar en este frame
    bool  jumpHeld    = false;  // el boton de salto sigue pulsado

    // --- Ajustes de sensacion: son las perillas del "game feel" ----------------
    float maxSpeed  = 250.0f;   // px/seg a los que se corre
    float accel     = 2200.0f;  // px/seg^2 para llegar a maxSpeed en el suelo
    float decel     = 2600.0f;  // px/seg^2 para frenar en el suelo al soltar
    float airAccel  = 1400.0f;  // idem en el aire (menos control = mas "peso")
    float airDecel  = 700.0f;

    // Altura del salto EN PIXELES (no una velocidad magica): el motor calcula la
    // velocidad inicial que hace falta a partir de la gravedad del RigidBody2D.
    float jumpHeight = 150.0f;

    // Ventana para saltar justo DESPUES de dejar el borde de una plataforma.
    float coyoteTime = 0.10f;
    // Ventana para que un salto pulsado justo ANTES de aterrizar se ejecute al tocar
    // el suelo, en vez de perderse.
    float jumpBufferTime = 0.12f;
    // Al SOLTAR el boton mientras sube, la velocidad se multiplica por esto: mantener
    // pulsado = salto alto, tocar y soltar = salto corto. 1.0 desactiva el salto corto.
    float cutJumpMultiplier = 0.45f;

    float maxFallSpeed = 900.0f; // velocidad terminal de caida (0 = sin limite)
    int   maxAirJumps  = 0;      // saltos extra en el aire (1 = doble salto)

    // --- Consulta: la usa la animacion -----------------------------------------
    // Suelo SUAVIZADO por el coyote time: es lo que conviene para elegir animacion,
    // porque ignora microcortes del apoyo (cambiar de clip reinicia la animacion).
    bool isGrounded() const { return coyote > 0.0f; }
    bool isGroundedRaw() const;      // el 'grounded' crudo de la fisica, sin suavizar
    bool isRising()  const;          // sube (velocidad vertical negativa)
    bool isFalling() const;          // cae
    bool justLanded() const { return landed; }  // solo en el frame del aterrizaje
    bool justJumped() const { return jumped; }  // solo en el frame del despegue
    int  airJumpsLeft() const { return airJumps; }

    // Velocidad inicial que el motor usara para el proximo salto completo, deducida de
    // jumpHeight y de la gravedad del RigidBody2D.
    float jumpSpeed() const;

    void start() override;          // resuelve el RigidBody2D hermano
    void update(float dt) override;

private:
    RigidBody2D* rb = nullptr;

    float coyote   = 0.0f;  // tiempo que queda de la ventana de coyote
    float buffer   = 0.0f;  // tiempo que queda del salto "adelantado"
    int   airJumps = 0;     // saltos de aire disponibles ahora mismo
    bool  jumping  = false; // hay un salto en curso al que se le puede cortar la altura
    bool  wasGrounded = false;
    bool  landed = false;
    bool  jumped = false;
};
