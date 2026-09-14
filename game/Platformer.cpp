#include "Platformer.h"

#include <SDL3/SDL.h>

#include "../engine/Scene.h"
#include "../engine/GameObject.h"
#include "../engine/Component.h"
#include "../engine/Transform.h"
#include "../engine/Input.h"
#include "../engine/SpriteRenderer.h"
#include "../engine/SpriteAnimator.h"
#include "../engine/RigidBody2D.h"
#include "../engine/BoxCollider.h"
#include "../engine/TilemapRenderer.h"
#include "../engine/Camera.h"
#include "../engine/FollowCamera.h"

// Izquierda/derecha + salto con espacio. El flanco de la tecla ("se acaba de
// presionar") lo da Input::wasPressed: el componente ya no necesita acordarse del
// estado del frame anterior ni hablar con SDL.
class PlatformerController : public Component {
public:
    float speed = 250.0f, jump = 650.0f;

    // start() corre UNA vez, al comienzo del primer frame, con el objeto ya completo.
    // Por eso aqui se pueden resolver los componentes hermanos de una sola vez, en
    // lugar de buscarlos con getComponent en cada update.
    void start() override {
        rb     = gameObject->getComponent<RigidBody2D>();
        sprite = gameObject->getComponent<SpriteRenderer>();
        anim   = gameObject->getComponent<SpriteAnimator>();
    }

    void update(float dt) override {
        float moveX = Input::axis(Key::Left, Key::Right); // -1, 0 o +1
        if (rb) rb->velocityX = moveX * speed;

        // "Coyote time": ventana de gracia para saltar apenas despues de dejar el
        // borde de una plataforma. Es DISENO (el salto se siente mas permisivo), no
        // un parche: el parpadeo del grounded que habia antes venia de medir el dt en
        // milisegundos, y eso ya se corrigio en el bucle de main (SDL_GetTicksNS).
        if (rb && rb->grounded) coyote = coyoteTime;
        else if (coyote > 0.0f) coyote -= dt;

        if (rb && Input::wasPressed(Key::Space) && coyote > 0.0f) {
            rb->velocityY = -jump;
            coyote = 0.0f; // consumir la ventana: evita doble salto en el mismo apoyo
        }

        if (sprite) { if (moveX < 0) sprite->flipX = true; else if (moveX > 0) sprite->flipX = false; }

        // Animacion segun el estado fisico. Evaluamos PRIMERO si esta en el suelo: si
        // lo esta, solo elegimos entre run/idle sin mirar velocityY (la gravedad lo
        // deja ligeramente positivo cada frame y dispararia "fall" por error). Se usa
        // el coyote como suelo SUAVIZADO para que un microcorte del apoyo no reinicie
        // la animacion de correr (play() reinicia el clip al cambiar de nombre).
        if (anim && rb) {
            bool onGround = coyote > 0.0f;
            if (onGround) anim->play(moveX != 0.0f ? "run" : "idle");
            else          anim->play(rb->velocityY < 0.0f ? "jump" : "fall");
        }
    }

private:
    RigidBody2D*    rb     = nullptr; // hermanos, resueltos en start()
    SpriteRenderer* sprite = nullptr;
    SpriteAnimator* anim   = nullptr;

    float coyote = 0.0f;                         // tiempo restante de la ventana de salto
    static constexpr float coyoteTime = 0.1f;    // segundos de gracia tras el ultimo contacto
};

void buildPlatformer(Scene& scene) {
    GameObject* player = scene.createGameObject("Player");
    player->transform->y = -150.0f;
    player->transform->scaleX = player->transform->scaleY = 4.0f;

    // ORDEN DE LOS COMPONENTES = orden de actualizacion. El controlador va PRIMERO
    // para que la velocidad que escribe la integre el RigidBody2D en este mismo
    // frame (si fuera al reves, el input llegaria con un frame de retraso), y el
    // SpriteAnimator va DESPUES para que dibuje el estado recien decidido.
    player->addComponent<PlatformerController>();
    player->addComponent<RigidBody2D>(); // con gravedad
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
