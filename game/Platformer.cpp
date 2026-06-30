#include "Platformer.h"

#include <SDL3/SDL.h>

#include "../engine/Scene.h"
#include "../engine/GameObject.h"
#include "../engine/Component.h"
#include "../engine/Transform.h"
#include "../engine/SpriteRenderer.h"
#include "../engine/SpriteAnimator.h"
#include "../engine/RigidBody2D.h"
#include "../engine/BoxCollider.h"
#include "../engine/TilemapRenderer.h"
#include "../engine/Camera.h"
#include "../engine/FollowCamera.h"

// Izquierda/derecha + salto con espacio. Detecta flanco de tecla con memoria
// del frame anterior para que el salto no se dispare multiples veces.
class PlatformerController : public Component {
public:
    float speed = 250.0f, jump = 650.0f;
    void update(float dt) override {
        const bool* keys = SDL_GetKeyboardState(nullptr);
        auto rb     = gameObject->getComponent<RigidBody2D>();
        auto sprite = gameObject->getComponent<SpriteRenderer>();
        auto anim   = gameObject->getComponent<SpriteAnimator>();

        float moveX = 0.0f;
        if (keys[SDL_SCANCODE_LEFT])  moveX -= 1.0f;
        if (keys[SDL_SCANCODE_RIGHT]) moveX += 1.0f;
        if (rb) rb->velocityX = moveX * speed;

        // "Coyote time": el grounded de la fisica parpadea porque el jugador queda
        // justo en el borde del suelo y la penetracion por frame es sub-pixel; a
        // framerate alto casi nunca se detecta el solape. Guardamos una ventana
        // corta desde el ultimo contacto real para que el salto no dependa de
        // acertar el frame exacto en que grounded vale true.
        if (rb && rb->grounded) coyote = coyoteTime;
        else if (coyote > 0.0f) coyote -= dt;

        bool jumpNow = keys[SDL_SCANCODE_SPACE];
        if (rb && jumpNow && !jumpPrev && coyote > 0.0f) {
            rb->velocityY = -jump;
            coyote = 0.0f; // consumir la ventana: evita doble salto en el mismo apoyo
        }
        jumpPrev = jumpNow;

        if (sprite) { if (moveX < 0) sprite->flipX = true; else if (moveX > 0) sprite->flipX = false; }

        // Animacion segun el estado fisico. Evaluamos PRIMERO si esta en el suelo: si
        // lo esta, solo elegimos entre run/idle sin mirar velocityY (la gravedad lo
        // deja ligeramente positivo cada frame y dispararia "fall" por error). Usamos
        // el "coyote" como suelo SUAVIZADO: el grounded crudo de la fisica parpadea
        // sub-pixel cada pocos frames, y como play() reinicia la animacion al cambiar
        // de nombre, ese parpadeo entrecortaba "run". El coyote ignora ese parpadeo.
        if (anim && rb) {
            bool onGround = coyote > 0.0f;
            if (onGround) anim->play(moveX != 0.0f ? "run" : "idle");
            else          anim->play(rb->velocityY < 0.0f ? "jump" : "fall");
        }
    }
private:
    bool  jumpPrev = false;
    float coyote = 0.0f;                         // tiempo restante de la ventana de salto
    static constexpr float coyoteTime = 0.1f;    // segundos de gracia tras el ultimo contacto
};

void buildPlatformer(Scene& scene) {
    GameObject* player = scene.createGameObject("Player");
    player->transform->y = -150.0f;
    player->transform->scaleX = player->transform->scaleY = 4.0f;
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
    player->addComponent<RigidBody2D>(); // con gravedad
    auto col = player->addComponent<BoxCollider>();
    col->width = 64.0f; col->height = 110.0f; col->offsetY = 8.0f; // ajustado al cuerpo
    player->addComponent<PlatformerController>();

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
    if (!tm->loadFromTiledJson("assets/maps/level1.json"))
        SDL_Log("buildPlatformer: no se pudo cargar assets/maps/level1.json");
    // Alternativa: nuestro formato .map propio (queda como referencia).
    // if (!tm->loadFromFile("assets/maps/level1.map"))
    //     SDL_Log("buildPlatformer: no se pudo cargar assets/maps/level1.map");

    GameObject* cam = scene.createGameObject("MainCamera");
    cam->addComponent<Camera>();
    auto f = cam->addComponent<FollowCamera>();
    f->setTarget(player);
    f->deadZoneWidth = 200.0f; f->deadZoneHeight = 200.0f;
}
