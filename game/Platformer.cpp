#include "Platformer.h"

#include <SDL3/SDL.h>

#include "../engine/Scene.h"
#include "../engine/GameObject.h"
#include "../engine/Component.h"
#include "../engine/Transform.h"
#include "../engine/Input.h"
#include "../engine/SpriteRenderer.h"
#include "../engine/SpriteAnimator.h"
#include "../engine/AnimatorStateMachine.h"
#include "../engine/RigidBody2D.h"
#include "../engine/BoxCollider.h"
#include "../engine/PlatformerMotor.h"
#include "../engine/TilemapRenderer.h"
#include "../engine/Camera.h"
#include "../engine/FollowCamera.h"

// Traduce TECLAS a INTENCION para el PlatformerMotor. Eso es todo lo que hace: no sabe
// de gravedad, de coyote time ni de saltos dobles; de eso se ocupa el motor, que es del
// engine y sirve para cualquier plataformas. Cambiar los controles (o enchufarle un
// mando, o una IA) es cambiar solo este componente.
class PlatformerController : public Component {
public:
    void start() override {
        motor  = gameObject->getComponent<PlatformerMotor>();
        sprite = gameObject->getComponent<SpriteRenderer>();
    }

    void update(float) override {
        if (!motor) return;
        motor->moveInput   = Input::axis(Key::Left, Key::Right); // -1, 0 o +1
        motor->jumpPressed = Input::wasPressed(Key::Space);      // flanco: un solo frame
        motor->jumpHeld    = Input::isDown(Key::Space);          // para el salto corto

        // Hacia donde mira el sprite. Es presentacion, no fisica: por eso vive aqui y
        // no en el motor.
        if (sprite) {
            if (motor->moveInput < 0.0f)      sprite->flipX = true;
            else if (motor->moveInput > 0.0f) sprite->flipX = false;
        }
    }

private:
    PlatformerMotor* motor  = nullptr;
    SpriteRenderer*  sprite = nullptr;
};

void buildPlatformer(Scene& scene) {
    GameObject* player = scene.createGameObject("Player");
    player->transform->y = -150.0f;
    player->transform->scaleX = player->transform->scaleY = 4.0f;

    // ORDEN DE LOS COMPONENTES = orden de actualizacion (el GameObject los recorre en
    // orden de insercion). La cadena de cada frame es:
    //   controlador (teclas -> intencion)
    //     -> PlatformerMotor (intencion -> velocidad)
    //       -> RigidBody2D (velocidad -> posicion)
    //         -> [fase de fisica de la Scene: choques y 'grounded']
    // Si el controlador fuera despues del motor, el input llegaria un frame tarde.
    player->addComponent<PlatformerController>();

    // Aqui estan TODAS las perillas del "game feel". Vale la pena tocarlas y ver como
    // cambia el personaje: es el ejercicio mas barato y mas ilustrativo del motor.
    auto motor = player->addComponent<PlatformerMotor>();
    motor->maxSpeed   = 250.0f;  // velocidad de carrera
    motor->accel      = 2200.0f; // que tan rapido llega a esa velocidad (arranque)
    motor->decel      = 2600.0f; // que tan rapido se detiene al soltar (frenado)
    motor->airAccel   = 1400.0f; // menos control en el aire que en el suelo
    motor->airDecel   = 700.0f;
    motor->jumpHeight = 200.0f;  // px de mundo: ~3 tiles de 64 px
    motor->coyoteTime = 0.10f;   // gracia para saltar tras dejar el borde
    motor->jumpBufferTime = 0.12f;    // gracia para saltar justo antes de aterrizar
    motor->cutJumpMultiplier = 0.45f; // soltar el boton = salto corto (1.0 lo desactiva)
    motor->maxFallSpeed = 900.0f;
    motor->maxAirJumps  = 0;     // pon 1 y tienes doble salto

    player->addComponent<RigidBody2D>(); // con gravedad; el motor le escribe la velocidad
    auto col = player->addComponent<BoxCollider>();
    col->width = 64.0f; col->height = 110.0f; col->offsetY = 8.0f; // ajustado al cuerpo

    // Sin textura inicial: cada animacion del Mask Dude es un .png aparte (una tira
    // horizontal de frames de 32x32), y el animator decide cual dibujar segun el estado.
    player->addComponent<SpriteRenderer>();
    auto anim = player->addComponent<SpriteAnimator>(32, 32, 1);
    const std::string mask = "assets/pixel_adventure/Main Characters/Mask Dude/";
    anim->addStripAnimation("idle", mask + "Idle (32x32).png", 32, 32, 20.0f);
    anim->addStripAnimation("run",  mask + "Run (32x32).png",  32, 32, 20.0f);
    anim->addStripAnimation("jump", mask + "Jump (32x32).png", 32, 32, 20.0f);
    anim->addStripAnimation("fall", mask + "Fall (32x32).png", 32, 32, 20.0f);
    anim->play("idle");

    // Que animacion suena, como REGLAS en vez de una cascada de if. Gana la PRIMERA
    // regla que se cumple, asi que el orden es la prioridad: primero lo del aire (mas
    // especifico), despues correr, y si no se cumple ninguna queda "idle".
    // Va al final para que lea el estado que el motor acaba de calcular en este frame.
    auto fsm = player->addComponent<AnimatorStateMachine>();
    fsm->setDefaultState("idle");
    fsm->addState("jump", [motor] { return !motor->isGrounded() && motor->isRising(); });
    fsm->addState("fall", [motor] { return !motor->isGrounded(); });
    fsm->addState("run",  [motor] { return motor->moveInput != 0.0f; });

    // Suelo y plataformas con un TilemapRenderer real (reemplaza el cuadrado estirado).
    // El mapa se carga desde un archivo de texto (contenido del juego, en assets/);
    // se puede editar a mano sin recompilar. El tileset, tile, columnas y tiles solidos
    // van en la cabecera del .map. Ver assets/maps/level1.map.
    GameObject* tilemap = scene.createGameObject("Tilemap");
    // El Transform marca el ORIGEN del mapa (esquina superior izquierda de la celda 0,0).
    tilemap->transform->x = -960.0f;
    tilemap->transform->y = -262.0f; // colocado para que el suelo quede en pantalla (~y=250)
    tilemap->transform->scaleX = tilemap->transform->scaleY = 4.0f; // 16px -> 64px por celda
    auto tm = tilemap->addComponent<TilemapRenderer>(); // modo archivo: el tileset lo da el mapa
    // Nivel exportado desde Tiled (JSON, capa de tiles + tileset embebido). El .json
    // se espera en assets/maps/ y su "image" (relativa al .json) debe apuntar al tileset
    // accesible desde ahi (p.ej. ../pixel_adventure/Terrain/...). Los tiles solidos se
    // marcan en Tiled con una propiedad booleana "solid"=true en el tileset.
    if (!tm->loadFromTiledJson("assets/maps/platformer_level1.json"))
        SDL_Log("buildPlatformer: no se pudo cargar assets/maps/platformer_level1.json");
    // Alternativa: nuestro formato .map propio (queda como referencia).
    // if (!tm->loadFromFile("assets/maps/level1.map"))
    //     SDL_Log("buildPlatformer: no se pudo cargar assets/maps/level1.map");

    GameObject* cam = scene.createGameObject("MainCamera");
    cam->addComponent<Camera>();
    auto f = cam->addComponent<FollowCamera>();
    f->setTarget(player);
    f->deadZoneWidth = 200.0f; f->deadZoneHeight = 200.0f;
}
