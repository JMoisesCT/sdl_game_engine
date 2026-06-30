#include "TopDown.h"

#include <SDL3/SDL.h>
#include <string>
#include <vector>

#include "../engine/Scene.h"
#include "../engine/GameObject.h"
#include "../engine/Component.h"
#include "../engine/Transform.h"
#include "../engine/SpriteRenderer.h"
#include "../engine/SpriteAnimator.h"
#include "../engine/TilemapRenderer.h"
#include "../engine/RigidBody2D.h"
#include "../engine/BoxCollider.h"
#include "../engine/Camera.h"
#include "../engine/FollowCamera.h"

// Movimiento libre en 4 direcciones sin gravedad (vista cenital), estilo Zelda:
// el personaje recuerda hacia donde camino por ultimo y queda en idle mirando alli.
class TopDownController : public Component {
public:
    float speed = 160.0f;
    std::string lastDir = "down"; // ultima direccion mirada (arranca mirando abajo)

    void update(float) override {
        const bool* keys = SDL_GetKeyboardState(nullptr);
        auto rb   = gameObject->getComponent<RigidBody2D>();
        auto anim = gameObject->getComponent<SpriteAnimator>();

        float mx = 0.0f, my = 0.0f;
        if (keys[SDL_SCANCODE_LEFT])  mx -= 1.0f;
        if (keys[SDL_SCANCODE_RIGHT]) mx += 1.0f;
        if (keys[SDL_SCANCODE_UP])    my -= 1.0f;
        if (keys[SDL_SCANCODE_DOWN])  my += 1.0f;
        if (rb) { rb->velocityX = mx * speed; rb->velocityY = my * speed; }

        bool moving = (mx != 0.0f || my != 0.0f);
        if (moving) {
            // Eje dominante para elegir la animacion: si hay componente horizontal,
            // manda el horizontal (asi una diagonal no queda ambigua); si no, el
            // vertical. Elegimos una sola direccion por frame.
            if (mx < 0)      lastDir = "left";
            else if (mx > 0) lastDir = "right";
            else if (my < 0) lastDir = "up";
            else             lastDir = "down";
        }

        // walk_<dir> si se mueve; idle_<ultima dir> si esta quieto. Sin flipX: cada
        // direccion es un sprite distinto, no un volteado.
        if (anim) anim->play((moving ? "walk_" : "idle_") + lastDir);
    }
};

void buildTopDown(Scene& scene) {
    // --- Mundo: piso cargado desde Tiled (JSON) ---------------------------------
    // OJO AL ORDEN: el dibujo sigue el orden de creacion de los GameObjects, asi que
    // el tilemap se crea PRIMERO para que el player (creado despues) quede ENCIMA del
    // piso. Si se crea despues, el piso tapa al personaje.
    //
    // El nivel se exporta desde Tiled (capa de tiles + tileset embebido) y vive en
    // assets/maps/. Su "image" apunta al TilesetFloor de Ninja Adventure. El tile,
    // las columnas y los tiles solidos los define el propio .json: aqui no se tocan.
    const int TILE  = 16;     // tile del TilesetFloor (lo confirma tambien el .json)
    const int MAP_W = 20, MAP_H = 15; // dimensiones del mapa de Tiled (para centrarlo)

    GameObject* world = scene.createGameObject("World");
    // El Transform marca el ORIGEN del mapa (esquina superior izquierda de la celda 0,0).
    world->transform->scaleX = world->transform->scaleY = 3.0f; // tile 16 -> 48 px
    auto map = world->addComponent<TilemapRenderer>(); // modo archivo: el tileset lo da el mapa

    if (!map->loadFromTiledJson("assets/maps/topdown_level1.json"))
        SDL_Log("buildTopDown: no se pudo cargar assets/maps/topdown_level1.json");

    // Centrar el mapa alrededor del origen, donde arranca el player. El mapa mide
    // MAP_W*TILE*scale x MAP_H*TILE*scale en mundo.
    world->transform->x = -(MAP_W * TILE * 3.0f) * 0.5f;
    world->transform->y = -(MAP_H * TILE * 3.0f) * 0.5f;

    // Referencia: el mapa equivalente definido en codigo (antes de pasar a Tiled).
    // const int TILESET_COLS = 22; // 352 / 16
    // const std::string TILESET = "assets/ninja_adventure/Backgrounds/Tilesets/TilesetFloor.png";
    // auto map = world->addComponent<TilemapRenderer>(TILESET, TILE, TILE, TILESET_COLS);
    // const int TILE_GRASS = 0, TILE_BORDER = 44; // borde solido (fila 2)
    // std::vector<int> tiles(MAP_W * MAP_H, TILE_GRASS);
    // for (int y = 0; y < MAP_H; ++y)
    //     for (int x = 0; x < MAP_W; ++x)
    //         if (x == 0 || y == 0 || x == MAP_W - 1 || y == MAP_H - 1)
    //             tiles[y * MAP_W + x] = TILE_BORDER;
    // map->setMap(tiles, MAP_W, MAP_H);
    // map->setSolid(TILE_BORDER);

    // --- NinjaGreen: mapeo de COLUMNAS del sheet a direcciones -----------------
    // Walk.png e Idle.png miden 128x128 = grilla 4x4 de cuadros de 32x32 (VERIFICADO
    // abriendo los PNG: 4 columnas x 4 filas, NO 16x16). En este pack la DIRECCION es la
    // COLUMNA y los FRAMES de cada animacion van por FILAS (4 frames por columna). Por
    // eso se usa addLineAnimation con StripAxis::Column.
    // El orden columna -> direccion NO esta garantizado: VERIFICALO abriendo el sprite.
    // Si el ninja mira al lado equivocado, esto es LO PRIMERO a corregir.
    const int   FRAME     = 32; // tamano de cada cuadro en el sheet (128 / 4 = 32)
    const int   COL_DOWN  = 0;
    const int   COL_UP    = 1;
    const int   COL_LEFT  = 2;
    const int   COL_RIGHT = 3;
    const float ANIM_FPS  = 9.0f;

    const std::string WALK = "assets/ninja_adventure/Actor/CharacterAnimated/NinjaGreen/Separate/Walk.png";
    const std::string IDLE = "assets/ninja_adventure/Actor/CharacterAnimated/NinjaGreen/Separate/Idle.png";

    GameObject* player = scene.createGameObject("Player");
    player->transform->x = 0.0f;
    player->transform->y = 0.0f;
    player->transform->scaleX = player->transform->scaleY = 2.0f; // 32px -> 64px en mundo
    player->addComponent<SpriteRenderer>(); // sin textura: la pone el SpriteAnimator

    // El tercer parametro (columnas de hoja) no lo usa addLineAnimation; aqui refleja
    // las 4 columnas reales del sheet.
    auto anim = player->addComponent<SpriteAnimator>(FRAME, FRAME, 4);
    anim->addLineAnimation("walk_down",  WALK, FRAME, FRAME, COL_DOWN,  StripAxis::Column, ANIM_FPS);
    anim->addLineAnimation("walk_up",    WALK, FRAME, FRAME, COL_UP,    StripAxis::Column, ANIM_FPS);
    anim->addLineAnimation("walk_left",  WALK, FRAME, FRAME, COL_LEFT,  StripAxis::Column, ANIM_FPS);
    anim->addLineAnimation("walk_right", WALK, FRAME, FRAME, COL_RIGHT, StripAxis::Column, ANIM_FPS);
    anim->addLineAnimation("idle_down",  IDLE, FRAME, FRAME, COL_DOWN,  StripAxis::Column, ANIM_FPS);
    anim->addLineAnimation("idle_up",    IDLE, FRAME, FRAME, COL_UP,    StripAxis::Column, ANIM_FPS);
    anim->addLineAnimation("idle_left",  IDLE, FRAME, FRAME, COL_LEFT,  StripAxis::Column, ANIM_FPS);
    anim->addLineAnimation("idle_right", IDLE, FRAME, FRAME, COL_RIGHT, StripAxis::Column, ANIM_FPS);
    anim->play("idle_down");

    auto rb = player->addComponent<RigidBody2D>();
    rb->gravityScale = 0.0f; // cenital: sin gravedad

    auto col = player->addComponent<BoxCollider>();
    // El ninja no llena el cuadro de 32x32: el cuerpo ocupa el centro-bajo. En unidades
    // de mundo (el cuadro completo mide 32*2 = 64 px). Ajustado al cuerpo y corrido
    // hacia los pies; calibrar con F1.
    col->width = 28.0f; col->height = 40.0f; col->offsetY = 8.0f;

    player->addComponent<TopDownController>();

    // --- Camara: sigue al player con zona muerta --------------------------------
    GameObject* cam = scene.createGameObject("MainCamera");
    cam->addComponent<Camera>();
    auto f = cam->addComponent<FollowCamera>();
    f->setTarget(player);
    f->deadZoneWidth = 120.0f; f->deadZoneHeight = 120.0f;
}
