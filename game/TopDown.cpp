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
    // --- NinjaGreen: mapeo de filas del sheet a direcciones --------------------
    // Walk.png e Idle.png miden 128x128 = grilla 8x8 de cuadros de 16x16 (verificado
    // abriendo los PNG). Las 4 primeras filas son las direcciones; las demas no se usan.
    // El orden de filas SUELE ser este en Ninja Adventure, pero VERIFICALO abriendo el
    // sprite: si el ninja mira al lado equivocado, esto es LO PRIMERO a corregir.
    const int   FRAME     = 16; // tamano de cada cuadro en el sheet (128 / 8 = 16)
    const int   ROW_DOWN  = 0;
    const int   ROW_UP    = 1;
    const int   ROW_LEFT  = 2;
    const int   ROW_RIGHT = 3;
    const float ANIM_FPS  = 9.0f;

    const std::string WALK = "assets/ninja_adventure/Actor/CharacterAnimated/NinjaGreen/Separate/Walk.png";
    const std::string IDLE = "assets/ninja_adventure/Actor/CharacterAnimated/NinjaGreen/Separate/Idle.png";

    GameObject* player = scene.createGameObject("Player");
    player->transform->x = 0.0f;
    player->transform->y = 0.0f;
    player->transform->scaleX = player->transform->scaleY = 3.0f; // 16px -> 48px en mundo
    player->addComponent<SpriteRenderer>(); // sin textura: la pone el SpriteAnimator

    // El tercer parametro (columnas de hoja) no se usa en modo addRowAnimation.
    auto anim = player->addComponent<SpriteAnimator>(FRAME, FRAME, 8);
    anim->addRowAnimation("walk_down",  WALK, FRAME, FRAME, ROW_DOWN,  ANIM_FPS);
    anim->addRowAnimation("walk_up",    WALK, FRAME, FRAME, ROW_UP,    ANIM_FPS);
    anim->addRowAnimation("walk_left",  WALK, FRAME, FRAME, ROW_LEFT,  ANIM_FPS);
    anim->addRowAnimation("walk_right", WALK, FRAME, FRAME, ROW_RIGHT, ANIM_FPS);
    anim->addRowAnimation("idle_down",  IDLE, FRAME, FRAME, ROW_DOWN,  ANIM_FPS);
    anim->addRowAnimation("idle_up",    IDLE, FRAME, FRAME, ROW_UP,    ANIM_FPS);
    anim->addRowAnimation("idle_left",  IDLE, FRAME, FRAME, ROW_LEFT,  ANIM_FPS);
    anim->addRowAnimation("idle_right", IDLE, FRAME, FRAME, ROW_RIGHT, ANIM_FPS);
    anim->play("idle_down");

    auto rb = player->addComponent<RigidBody2D>();
    rb->gravityScale = 0.0f; // cenital: sin gravedad

    auto col = player->addComponent<BoxCollider>();
    // El ninja no llena los 16x16: cuerpo mas chico y corrido hacia los pies. En
    // unidades de mundo (el sprite completo mide 16*3 = 48 px). Calibrar con F1.
    col->width = 22.0f; col->height = 26.0f; col->offsetY = 8.0f;

    player->addComponent<TopDownController>();

    // --- Mundo: piso cargado desde Tiled (JSON) ---------------------------------
    // El nivel se exporta desde Tiled (capa de tiles + tileset embebido) y vive en
    // assets/maps/. Su "image" apunta al TilesetFloor de Ninja Adventure. El tile,
    // las columnas y los tiles solidos los define el propio .json: aqui no se tocan.
    const int TILE  = 16;     // tile del TilesetFloor (lo confirma tambien el .json)
    const int MAP_W = 20, MAP_H = 15; // dimensiones del mapa de Tiled (para centrarlo)

    GameObject* world = scene.createGameObject("World");
    // El Transform marca el ORIGEN del mapa (esquina superior izquierda de la celda 0,0).
    world->transform->scaleX = world->transform->scaleY = 3.0f; // tile 16 -> 48 px (igual que el player)
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

    // --- Camara: sigue al player con zona muerta --------------------------------
    GameObject* cam = scene.createGameObject("MainCamera");
    cam->addComponent<Camera>();
    auto f = cam->addComponent<FollowCamera>();
    f->setTarget(player);
    f->deadZoneWidth = 120.0f; f->deadZoneHeight = 120.0f;
}
