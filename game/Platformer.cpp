#include "Platformer.h"

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
#include "../engine/AnimatorStateMachine.h"
#include "../engine/RigidBody2D.h"
#include "../engine/BoxCollider.h"
#include "../engine/PlatformerMotor.h"
#include "../engine/TilemapRenderer.h"
#include "../engine/TilemapCollider.h"
#include "../engine/TiledObjectLayer.h"
#include "../engine/ParallaxBackground.h"
#include "../engine/TextRenderer.h"
#include "../engine/Camera.h"
#include "../engine/FollowCamera.h"

// --- Contenido del nivel (rutas del lado del JUEGO, no del motor) ---------------
static const char* LEVEL_JSON = "assets/maps/platformer_level1.json";
static const char* MASK_DUDE  = "assets/pixel_adventure/Main Characters/Mask Dude/";
static const char* FRUITS_DIR = "assets/pixel_adventure/Items/Fruits/";
static const char* END_DIR    = "assets/pixel_adventure/Items/Checkpoints/End/";
static const char* BG_IMAGE   = "assets/pixel_adventure/Background/Blue.png";
static const char* HUD_FONT   = "assets/ninja_adventure/Ui/Font/NormalFont.ttf";
static const int   HUD_SIZE   = 32;

// Capas de dibujo (GameObject::sortingOrder). Menor = mas al fondo.
enum Layer { LAYER_BG = -100, LAYER_TILEMAP = -10, LAYER_ITEMS = 0,
             LAYER_PLAYER = 10, LAYER_HUD = 100 };

// Donde aparece el jugador si el mapa NO trae un objeto PlayerStart.
static const float FALLBACK_SPAWN_X = 0.0f;
static const float FALLBACK_SPAWN_Y = -150.0f;

// --- Controles ------------------------------------------------------------------
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

// --- HUD: frutas recogidas ------------------------------------------------------
// El conteo es REGLA DE JUEGO, por eso vive aqui y no en el motor: el TextRenderer
// solo pinta el string que le pasamos. Mismo patron que el HudScore del shooter.
class FruitHud : public Component {
public:
    TextRenderer* label  = nullptr; // contador, pegado a la esquina
    TextRenderer* banner = nullptr; // cartel de "nivel completado", centrado
    int collected = 0;
    int total     = 0;

    void add(int n) { collected += n; }

    // El aviso de meta NO se concatena al contador: va en su propio TextRenderer. Si
    // se pegaran en la misma cadena, al crecer el texto el renglon entero se recentra
    // y el contador parece saltar de sitio.
    void setMessage(const std::string& m) { if (banner) banner->setText(m); }

    void update(float) override {
        if (collected == shown) return;
        shown = collected;
        if (label)
            label->setText("FRUTAS: " + std::to_string(collected) + "/" + std::to_string(total));
    }

private:
    int shown = -1; // distinto de collected: fuerza el primer refresco
};

// --- Coleccionable --------------------------------------------------------------
// Al tocarlo reproduce la animacion "collected" (que NO hace loop) y se destruye
// cuando esa animacion termina, usando el aviso setOnComplete del SpriteAnimator.
// La version generica (un componente Collectible del motor) llega en la fase 3.
class Fruit : public Component {
public:
    FruitHud* hud = nullptr;

    void start() override { anim = gameObject->getComponent<SpriteAnimator>(); }

    void onCollision(GameObject* other) override {
        if (taken || other->name != "Player") return; // una sola vez, y solo el jugador
        taken = true;
        if (hud) hud->add(1);

        if (!anim) { gameObject->scene->destroy(gameObject); return; }
        GameObject* self = gameObject;
        anim->setOnComplete([self](const std::string&) { self->scene->destroy(self); });
        anim->play("collected");
    }

private:
    SpriteAnimator* anim = nullptr;
    bool taken = false;
};

// --- Meta del nivel -------------------------------------------------------------
class LevelEnd : public Component {
public:
    FruitHud* hud = nullptr;

    void start() override { anim = gameObject->getComponent<SpriteAnimator>(); }

    void onCollision(GameObject* other) override {
        if (done || other->name != "Player") return;
        done = true;
        if (anim) anim->play("pressed"); // clip de un solo uso: se queda en el ultimo cuadro
        if (hud) hud->setMessage("NIVEL COMPLETADO");
    }

private:
    SpriteAnimator* anim = nullptr;
    bool done = false;
};

// --- Utilidades del nivel -------------------------------------------------------
// Tiled entrega coordenadas en pixeles DEL MAPA (tiles de 16 px). El mundo esta
// escalado por el Transform del tilemap, asi que hay que multiplicar por esa escala y
// sumarle el origen del mapa. Dividir el tamano de celda en el mundo entre el de la
// imagen da exactamente esa escala, sin cablear el 4.
static void tiledToWorld(const TilemapRenderer* map, float tx, float ty,
                         float& wx, float& wy) {
    float sx = map->getTileWorldWidth()  / (float)map->getTileWidth();
    float sy = map->getTileWorldHeight() / (float)map->getTileHeight();
    wx = map->getOriginX() + tx * sx;
    wy = map->getOriginY() + ty * sy;
}

static GameObject* createPlayer(Scene& scene, float x, float y) {
    GameObject* player = scene.createGameObject("Player");
    player->sortingOrder = LAYER_PLAYER;
    player->transform->x = x;
    player->transform->y = y;
    player->transform->scaleX = player->transform->scaleY = 4.0f;

    // ORDEN DE LOS COMPONENTES = orden de actualizacion (el GameObject los recorre en
    // orden de insercion). La cadena de cada frame es:
    //   controlador (teclas -> intencion)
    //     -> PlatformerMotor (intencion -> velocidad)
    //       -> RigidBody2D (velocidad -> posicion)
    //         -> [fase de fisica de la Scene: tilemap y pares]
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
    const std::string mask = MASK_DUDE;
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

    return player;
}

static void createFruit(Scene& scene, float x, float y,
                        const std::string& kind, FruitHud* hud) {
    GameObject* f = scene.createGameObject("Fruit");
    f->sortingOrder = LAYER_ITEMS;
    f->transform->x = x;
    f->transform->y = y;
    f->transform->scaleX = f->transform->scaleY = 2.0f; // 32 px -> 64 px

    f->addComponent<SpriteRenderer>();
    auto anim = f->addComponent<SpriteAnimator>(32, 32, 1);
    // Cada fruta es una tira de 17 cuadros de 32x32; "Collected" es el efecto comun
    // de 6 cuadros, y va SIN loop para poder destruir el objeto cuando termina.
    anim->addStripAnimation("idle", std::string(FRUITS_DIR) + kind + ".png", 32, 32, 20.0f);
    anim->addStripAnimation("collected", std::string(FRUITS_DIR) + "Collected.png",
                            32, 32, 20.0f, false);
    anim->play("idle");

    auto col = f->addComponent<BoxCollider>();
    col->width = 48.0f; col->height = 48.0f;
    col->isTrigger = true; // avisa al tocarlo, pero no frena al jugador

    f->addComponent<Fruit>()->hud = hud;
}

static void createLevelEnd(Scene& scene, float x, float y, FruitHud* hud) {
    GameObject* e = scene.createGameObject("LevelEnd");
    e->sortingOrder = LAYER_ITEMS;
    e->transform->x = x;
    e->transform->y = y;
    e->transform->scaleX = e->transform->scaleY = 2.0f; // 64 px -> 128 px

    e->addComponent<SpriteRenderer>();
    auto anim = e->addComponent<SpriteAnimator>(64, 64, 1);
    anim->addStripAnimation("idle",    std::string(END_DIR) + "End (Idle).png", 64, 64, 1.0f);
    anim->addStripAnimation("pressed", std::string(END_DIR) + "End (Pressed) (64x64).png",
                            64, 64, 20.0f, false);
    anim->play("idle");

    auto col = e->addComponent<BoxCollider>();
    col->width = 64.0f; col->height = 96.0f;
    col->isTrigger = true;

    e->addComponent<LevelEnd>()->hud = hud;
}

void buildPlatformer(Scene& scene) {
    // --- Fondo con parallax ------------------------------------------------------
    // Se dibuja primero (sortingOrder mas bajo) y se mueve a un tercio de la camara,
    // asi el nivel parece tener profundidad. El PNG es tileable de 64x64.
    GameObject* bg = scene.createGameObject("Background");
    bg->sortingOrder = LAYER_BG;
    auto par = bg->addComponent<ParallaxBackground>(BG_IMAGE);
    par->factorX = 0.3f;
    par->factorY = 0.3f;
    par->scale   = 4.0f;          // mosaico de 256 px, a juego con los tiles
    par->scrollSpeedY = -12.0f;   // deriva lenta hacia arriba, como en Pixel Adventure

    // --- Suelo y plataformas -----------------------------------------------------
    GameObject* tilemap = scene.createGameObject("Tilemap");
    tilemap->sortingOrder = LAYER_TILEMAP;
    // El Transform marca el ORIGEN del mapa (esquina superior izquierda de la celda 0,0).
    tilemap->transform->x = -960.0f;
    tilemap->transform->y = -262.0f;
    tilemap->transform->scaleX = tilemap->transform->scaleY = 4.0f; // 16px -> 64px por celda
    auto tm = tilemap->addComponent<TilemapRenderer>();
    // Nivel exportado desde Tiled (JSON, capa de tiles + tileset embebido). Los tiles
    // solidos se marcan en Tiled con una propiedad booleana "solid"=true en el tileset.
    if (!tm->loadFromTiledJson(LEVEL_JSON))
        SDL_Log("buildPlatformer: no se pudo cargar %s", LEVEL_JSON);
    // El renderer solo DIBUJA; este componente es lo que hace que los tiles frenen.
    tilemap->addComponent<TilemapCollider>();

    // --- HUD ---------------------------------------------------------------------
    // Tamano real de la ventana: el cartel de meta se centra en pantalla y el contador
    // se pega a la esquina, sin cablear 1280x720.
    int screenW = 0, screenH = 0;
    SDL_GetCurrentRenderOutputSize(scene.getRenderer(), &screenW, &screenH);

    GameObject* hudObj = scene.createGameObject("HUD");
    hudObj->sortingOrder = LAYER_HUD;
    hudObj->transform->x = 24.0f;  // coordenadas de PANTALLA (screenSpace)
    hudObj->transform->y = 32.0f;
    auto label = hudObj->addComponent<TextRenderer>();
    label->screenSpace = true;
    // Anclado a la IZQUIERDA: el contador cambia de longitud al pasar de 0/3 a 3/3, y
    // con el anclaje al centro se moveria solo.
    label->align = TextAlign::Left;
    label->setFont(scene.getAssets().loadFont(HUD_FONT, HUD_SIZE));
    label->setColor(TextColor{ 255, 255, 255, 255 });

    // Cartel de meta: objeto aparte, centrado arriba, vacio hasta que haga falta (un
    // TextRenderer sin texto no dibuja nada).
    GameObject* bannerObj = scene.createGameObject("HUDBanner");
    bannerObj->sortingOrder = LAYER_HUD;
    bannerObj->transform->x = screenW * 0.5f;
    bannerObj->transform->y = 96.0f;
    auto banner = bannerObj->addComponent<TextRenderer>();
    banner->screenSpace = true;
    banner->align = TextAlign::Center;
    banner->setFont(scene.getAssets().loadFont(HUD_FONT, HUD_SIZE));
    banner->setColor(TextColor{ 255, 232, 96, 255 }); // amarillo, para que destaque

    auto hud = hudObj->addComponent<FruitHud>();
    hud->label  = label;
    hud->banner = banner;

    // --- Contenido desde la capa de objetos de Tiled -----------------------------
    // El motor NO sabe que significa cada "type": entrega los objetos como datos y la
    // fabrica de aqui decide que construir. Es el mismo patron que usa el shooter.
    // Asi el nivel se edita en Tiled, sin recompilar y sin numeros cableados aqui.
    std::vector<TiledObject> objects = loadTiledObjectLayers(LEVEL_JSON);

    float spawnX = FALLBACK_SPAWN_X, spawnY = FALLBACK_SPAWN_Y;
    bool  haveSpawn = false;
    int   fruitCount = 0;

    // Primera pasada: el punto de aparicion, porque el jugador se crea antes que nada
    // mas (asi la camara ya lo tiene a quien seguir).
    for (const TiledObject& o : objects) {
        if (o.type == "PlayerStart") {
            tiledToWorld(tm, o.cx, o.cy, spawnX, spawnY);
            haveSpawn = true;
            break;
        }
    }
    if (!haveSpawn)
        SDL_Log("buildPlatformer: el mapa no trae ningun objeto PlayerStart; "
                "se usa la posicion por defecto.");

    GameObject* player = createPlayer(scene, spawnX, spawnY);

    // Segunda pasada: el resto del contenido.
    for (const TiledObject& o : objects) {
        float wx, wy;
        tiledToWorld(tm, o.cx, o.cy, wx, wy);

        if (o.type == "Fruit") {
            // Propiedad "fruit" del objeto en Tiled: Apple, Bananas, Cherries, Kiwi,
            // Melon, Orange, Pineapple o Strawberry. Si falta, cae en Apple.
            createFruit(scene, wx, wy, o.getString("fruit", "Apple"), hud);
            ++fruitCount;
        } else if (o.type == "LevelEnd") {
            createLevelEnd(scene, wx, wy, hud);
        } else if (o.type != "PlayerStart" && !o.type.empty()) {
            SDL_Log("buildPlatformer: objeto de Tiled con type '%s' sin fabrica; se ignora.",
                    o.type.c_str());
        }
    }
    hud->total = fruitCount;

    // --- Camara ------------------------------------------------------------------
    GameObject* cam = scene.createGameObject("MainCamera");
    cam->addComponent<Camera>();
    auto f = cam->addComponent<FollowCamera>();
    f->setTarget(player);
    f->deadZoneWidth = 200.0f; f->deadZoneHeight = 200.0f;
    f->lookAhead = 120.0f;             // adelanta la vista hacia donde corre
    f->setBoundsFromTilemap(tm);       // y nunca se sale del nivel
}
