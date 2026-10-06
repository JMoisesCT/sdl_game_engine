#include "TopDown.h"

#include <SDL3/SDL.h>
#include <string>
#include <vector>

#include "../engine/Scene.h"
#include "../engine/GameObject.h"
#include "../engine/Component.h"
#include "../engine/Transform.h"
#include "../engine/Input.h"
#include "../engine/SpriteRenderer.h"
#include "../engine/SpriteAnimator.h"
#include "../engine/TilemapRenderer.h"
#include "../engine/TilemapCollider.h"
#include "../engine/LevelData.h"
#include "../engine/RigidBody2D.h"
#include "../engine/BoxCollider.h"
#include "../engine/Collectible.h"
#include "../engine/TextRenderer.h"
#include "../engine/Camera.h"
#include "../engine/FollowCamera.h"

// --- Contenido del nivel (rutas del lado del JUEGO, no del motor) ---------------
// El nivel son DOS archivos: el mapa de Tiled (el piso) y el .level.json (objetos y
// camara), que ademas dice que mapa usa. Por eso aqui solo se nombra el segundo.
const char* TOPDOWN_LEVEL_FILE = "assets/maps/topdown_level1.level.json";
static const char* NINJA_DIR = "assets/ninja_adventure/Actor/CharacterAnimated/NinjaGreen/Separate/";
static const char* FOOD_DIR  = "assets/ninja_adventure/Items/Food/";
static const char* HUD_FONT  = "assets/ninja_adventure/Ui/Font/NormalFont.ttf";
static const int   HUD_SIZE  = 32;

static const char* TAG_PLAYER = "Player";
static const char* TAG_PICKUP = "Pickup";

// Capas de dibujo (GameObject::sortingOrder). Menor = mas al fondo.
enum Layer { LAYER_TILEMAP = -10, LAYER_ITEMS = 0, LAYER_PLAYER = 10, LAYER_HUD = 100 };

// --- Estado de la partida -------------------------------------------------------
// Cuantos objetos hay y cuantos se recogieron. Lo modifican los objetos (cada Item al
// armarse y al recogerse) y el HUD lo muestra, sin punteros entre ellos. Se reinicia
// cada vez que se construye el nivel (al empezar, y cada Stop/Play del editor).
struct TopDownState {
    int items      = 0; // recogidos
    int totalItems = 0; // los que hay en el nivel
};
static TopDownState state;

// Movimiento libre en 4 direcciones sin gravedad (vista cenital), estilo Zelda:
// el personaje recuerda hacia donde camino por ultimo y queda en idle mirando alli.
class TopDownController : public Component {
public:
    float speed = 160.0f;
    std::string lastDir = "down"; // ultima direccion mirada (arranca mirando abajo)

    // Componentes hermanos resueltos una sola vez (start corre con el objeto ya completo).
    void start() override {
        rb   = gameObject->getComponent<RigidBody2D>();
        anim = gameObject->getComponent<SpriteAnimator>();
    }

    void update(float) override {
        float mx = Input::axis(Key::Left, Key::Right);
        float my = Input::axis(Key::Up,   Key::Down);
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

private:
    RigidBody2D*    rb   = nullptr;
    SpriteAnimator* anim = nullptr;
};

// --- HUD ------------------------------------------------------------------------
// Solo LEE el estado de la partida y refresca el texto cuando cambia.
class TopDownHud : public Component {
public:
    TextRenderer* label = nullptr;

    void update(float) override {
        if (!label || state.items == shownItems) return;
        shownItems = state.items;
        label->setText("OBJETOS: " + std::to_string(state.items) + "/" +
                       std::to_string(state.totalItems));
    }

private:
    int shownItems = -1; // distinto del valor real: fuerza el primer refresco
};

// ================================================================================
// OBJETOS DEL NIVEL
// Cada type del .level.json tiene aqui su funcion setup: la RECETA del objeto. El motor
// (spawnLevelObjects) ya creo el GameObject, lo puso en su posicion y le dio el id que
// usa el editor; el setup solo le agrega componentes, tag, escala y capa de dibujo.
// Para agregar un objeto nuevo al juego: escribir su setup y sumarlo al catalogo
// (topDownObjects, mas abajo). Despues se coloca con el editor (F2 -> Nuevo).
// ================================================================================

// PlayerStart: el ninja.
//
// NinjaGreen: Walk.png e Idle.png miden 128x128 = grilla 4x4 de cuadros de 32x32. En
// este pack la DIRECCION es la COLUMNA y los FRAMES de cada animacion van por FILAS (4
// frames por columna). Por eso se usa addLineAnimation con StripAxis::Column.
// El orden columna -> direccion NO esta garantizado: VERIFICALO abriendo el sprite.
// Si el ninja mira al lado equivocado, esto es LO PRIMERO a corregir.
static void setupPlayer(GameObject* player, const LevelObject&) {
    const int   FRAME     = 32; // tamano de cada cuadro en el sheet (128 / 4 = 32)
    const int   COL_DOWN  = 0;
    const int   COL_UP    = 1;
    const int   COL_LEFT  = 2;
    const int   COL_RIGHT = 3;
    const float ANIM_FPS  = 9.0f;
    const std::string WALK = std::string(NINJA_DIR) + "Walk.png";
    const std::string IDLE = std::string(NINJA_DIR) + "Idle.png";

    player->name = "Player";
    player->tag = TAG_PLAYER;
    player->sortingOrder = LAYER_PLAYER;
    player->transform->scaleX = player->transform->scaleY = 2.0f; // 32px -> 64px en mundo

    // El controlador va ANTES del RigidBody2D: asi la velocidad que escribe se integra
    // en el mismo frame (los componentes se actualizan en orden de insercion).
    player->addComponent<TopDownController>();

    auto rb = player->addComponent<RigidBody2D>();
    rb->gravityScale = 0.0f; // cenital: sin gravedad

    auto col = player->addComponent<BoxCollider>();
    // El ninja no llena el cuadro de 32x32: el cuerpo ocupa el centro-bajo. En unidades
    // de mundo (el cuadro completo mide 32*2 = 64 px). Ajustado al cuerpo y corrido
    // hacia los pies; calibrar con F1.
    col->width = 28.0f; col->height = 40.0f; col->offsetY = 8.0f;

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
}

// Item: comida que se recoge. Propiedad "item" = nombre del PNG de Items/Food (16x16).
static void setupItem(GameObject* it, const LevelObject& o) {
    it->tag = TAG_PICKUP;
    it->sortingOrder = LAYER_ITEMS;
    it->transform->scaleX = it->transform->scaleY = 3.0f; // 16 px -> 48 px, como un tile

    const std::string kind = o.getString("item", "Sushi");
    it->addComponent<SpriteRenderer>(std::string(FOOD_DIR) + kind + ".png");

    auto col = it->addComponent<BoxCollider>();
    col->width = 36.0f; col->height = 36.0f;
    col->isTrigger = true; // avisa al tocarlo, pero no frena al jugador

    // Collectible es del MOTOR y no lleva cuentas: que significa recoger lo dice el juego.
    auto c = it->addComponent<Collectible>();
    c->collectorTag = TAG_PLAYER;
    c->onCollect = [](GameObject*) { ++state.items; };

    ++state.totalItems; // cada item que se arma cuenta para el total del HUD
}

// --- Catalogo: los objetos que existen en este juego ------------------------------
// La MISMA lista la usan la fabrica (para crear cada objeto del archivo) y el editor
// (para ofrecerlos en "Nuevo", con sus propiedades por defecto).
ObjectCatalog topDownObjects() {
    ObjectCatalog c;
    c.add("PlayerStart", setupPlayer, "Donde aparece el ninja al empezar").playerSpawn();
    c.add("Item", setupItem, "Comida que se recoge (cuenta en el HUD)")
        .choice("item", { "Sushi", "Onigiri", "Yakitori", "Noodle", "FortuneCookie",
                          "Honey", "Fish", "Meat" });
    return c;
}

// ================================================================================
// LO FIJO DEL NIVEL (no viene del archivo: es igual en cualquier nivel del juego)
// ================================================================================

void buildTopDown(Scene& scene) {
    // Si el archivo falla, el nivel sale vacio (sin mapa ni objetos) pero el juego no se
    // cae: queda el log para saber por que.
    LevelData level;
    if (!loadLevel(TOPDOWN_LEVEL_FILE, level))
        SDL_Log("buildTopDown: no se pudo cargar %s", TOPDOWN_LEVEL_FILE);
    buildTopDownLevel(scene, level);
}

void buildTopDownLevel(Scene& scene, const LevelData& level) {
    state = TopDownState(); // partida nueva: nada recogido

    // --- Mundo: piso cargado desde Tiled (JSON) ---------------------------------
    // Su "image" apunta al TilesetFloor de Ninja Adventure. El tile, las columnas y los
    // tiles solidos los define el propio .json: aqui no se tocan.
    GameObject* world = scene.createGameObject("World");
    world->sortingOrder = LAYER_TILEMAP;
    // El Transform marca el ORIGEN del mapa (esquina superior izquierda de la celda 0,0).
    world->transform->scaleX = world->transform->scaleY = 3.0f; // tile 16 -> 48 px
    auto map = world->addComponent<TilemapRenderer>(); // modo archivo: el tileset lo da el mapa
    // El TilemapRenderer solo dibuja: el TilemapCollider es lo que hace que los
    // tiles marcados como solidos frenen al personaje.
    world->addComponent<TilemapCollider>();
    if (!map->loadFromTiledJson(level.mapPath))
        SDL_Log("buildTopDown: no se pudo cargar el mapa '%s'", level.mapPath.c_str());

    // Centrar el mapa alrededor del origen del mundo, con su tamanio leido del propio
    // mapa. Va ANTES de crear los objetos: la conversion mapa -> mundo usa este Transform.
    world->transform->x = -map->getWorldWidth()  * 0.5f;
    world->transform->y = -map->getWorldHeight() * 0.5f;

    // --- Objetos del nivel ---------------------------------------------------------
    spawnLevelObjects(scene, level, map, topDownObjects());

    GameObject* player = scene.findWithTag(TAG_PLAYER);
    if (!player)
        SDL_Log("buildTopDown: el nivel no trae ningun PlayerStart; no hay jugador.");

    // --- HUD ---------------------------------------------------------------------
    // Anclado a la IZQUIERDA: el contador cambia de longitud.
    GameObject* hudObj = scene.createGameObject("HUD");
    hudObj->sortingOrder = LAYER_HUD;
    hudObj->transform->x = 24.0f;
    hudObj->transform->y = 32.0f;
    auto label = hudObj->addComponent<TextRenderer>();
    label->screenSpace = true;
    label->align = TextAlign::Left;
    label->setFont(scene.getAssets().loadFont(HUD_FONT, HUD_SIZE));
    label->setColor(TextColor{ 255, 255, 255, 255 });
    hudObj->addComponent<TopDownHud>()->label = label;

    // --- Camara: sigue al player con zona muerta --------------------------------
    GameObject* cam = scene.createGameObject("MainCamera");
    auto camera = cam->addComponent<Camera>();
    auto f = cam->addComponent<FollowCamera>();
    f->setTarget(player);
    // Valores POR DEFECTO del juego; el archivo del nivel puede cambiar cualquiera.
    f->deadZoneWidth = 120.0f; f->deadZoneHeight = 120.0f;
    applyCameraSettings(level, camera, f);
}
