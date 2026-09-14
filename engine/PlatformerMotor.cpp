#include "PlatformerMotor.h"

#include <SDL3/SDL.h>
#include <cmath>

#include "GameObject.h"
#include "RigidBody2D.h"

namespace {
    // Acerca 'current' a 'target' como mucho 'maxDelta', sin pasarse. Es la aceleracion
    // y el frenado de toda la vida, pero sin escribir dos veces la misma cuenta.
    float moveToward(float current, float target, float maxDelta) {
        if (current < target) { current += maxDelta; return current > target ? target : current; }
        if (current > target) { current -= maxDelta; return current < target ? target : current; }
        return target;
    }
}

void PlatformerMotor::start() {
    rb = gameObject->getComponent<RigidBody2D>();
    if (!rb)
        SDL_Log("PlatformerMotor: el objeto %s no tiene RigidBody2D; el motor no hara nada.",
                gameObject->name.c_str());
}

bool PlatformerMotor::isGroundedRaw() const { return rb && rb->grounded; }
bool PlatformerMotor::isRising()  const { return rb && rb->velocityY < 0.0f; }
bool PlatformerMotor::isFalling() const { return rb && rb->velocityY > 0.0f; }

float PlatformerMotor::jumpSpeed() const {
    if (!rb) return 0.0f;
    // Tiro vertical: para subir una altura h con gravedad g hay que salir a
    // v = raiz(2 * g * h). Por eso el ajuste es una ALTURA en pixeles y no un numero
    // magico de velocidad: si cambias la gravedad, el salto sigue llegando igual de alto.
    float g = rb->gravity * rb->gravityScale;
    if (g <= 0.0f || jumpHeight <= 0.0f) return 0.0f;
    return std::sqrt(2.0f * g * jumpHeight);
}

void PlatformerMotor::update(float dt) {
    if (!rb) return;

    // --- Estado del apoyo ------------------------------------------------------
    bool groundedNow = rb->grounded;
    landed = groundedNow && !wasGrounded;  // solo el frame en que toca suelo
    wasGrounded = groundedNow;

    if (groundedNow) {
        coyote   = coyoteTime;   // recarga la ventana mientras este apoyado
        airJumps = maxAirJumps;  // y devuelve los saltos de aire
        jumping  = false;
    } else if (coyote > 0.0f) {
        coyote -= dt;
    }

    // --- Memoria del boton de salto -------------------------------------------
    // Si se pulso poco ANTES de aterrizar, el salto no se pierde: espera en el buffer
    // hasta que haya suelo (o hasta que se acabe la ventana).
    if (jumpPressed)        buffer = jumpBufferTime;
    else if (buffer > 0.0f) buffer -= dt;

    // --- Horizontal: acelerar / frenar hacia la velocidad deseada --------------
    // Sin esto el personaje arranca y para de golpe (velocidad = input * speed), que es
    // justo lo que hace que un plataformas se sienta rigido.
    float target = moveInput * maxSpeed;
    bool  wantsToMove = (moveInput != 0.0f);
    float rate = groundedNow ? (wantsToMove ? accel   : decel)
                             : (wantsToMove ? airAccel : airDecel);
    rb->velocityX = moveToward(rb->velocityX, target, rate * dt);

    // --- Salto -----------------------------------------------------------------
    jumped = false;
    if (buffer > 0.0f) {
        bool fromGround = (coyote > 0.0f);
        bool fromAir    = (!fromGround && airJumps > 0);
        if (fromGround || fromAir) {
            rb->velocityY = -jumpSpeed();
            buffer = 0.0f;   // consumido
            coyote = 0.0f;   // y la ventana tambien, para no encadenar dos saltos
            if (fromAir) --airJumps;
            jumping = true;
            jumped  = true;
        }
    }

    // --- Altura variable: soltar el boton corta la subida ----------------------
    if (jumping && !jumpHeld && rb->velocityY < 0.0f) {
        rb->velocityY *= cutJumpMultiplier;
        jumping = false; // ya se corto: no volver a cortarlo en este mismo salto
    }
    if (jumping && rb->velocityY >= 0.0f) jumping = false; // ya empezo a caer

    // --- Velocidad terminal ----------------------------------------------------
    // Va aqui, antes de que el RigidBody2D sume la gravedad de este frame, asi que el
    // limite se aplica con un frame de retraso: irrelevante a 60 fps, y evita meterle
    // una regla de plataformas al RigidBody2D, que es generico.
    if (maxFallSpeed > 0.0f && rb->velocityY > maxFallSpeed)
        rb->velocityY = maxFallSpeed;
}
