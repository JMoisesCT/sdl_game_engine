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
#include "../engine/Health.h"
#include "../engine/Hazard.h"
#include "../engine/Collectible.h"
#include "../engine/Checkpoint.h"
#include "../engine/Respawn.h"
#include "../engine/KillZone.h"
#include "../engine/Camera.h"
#include "../engine/FollowCamera.h"

// --- Contenido del nivel (rutas del lado del JUEGO, no del motor) ---------------
static const char* LEVEL_JSON = "assets/maps/platformer_level1.json";
static const char* MASK_DUDE  = "assets/pixel_adventure/Main Characters/Mask Dude/";
static const char* FRUITS_DIR = "assets/pixel_adventure/Items/Fruits/";
static const char* END_DIR    = "assets/pixel_adventure/Items/Checkpoints/End/";
static const char* CHECK_DIR  = "assets/pixel_adventure/Items/Checkpoints/Checkpoint/";
static const char* TRAPS_DIR  = "assets/pixel_adventure/Traps/";
static const char* BG_IMAGE   = "assets/pixel_adventure/Background/Blue.png";
static const char* HUD_FONT   = "assets/ninja_adventure/Ui/Font/NormalFont.ttf";
static const int   HUD_SIZE   = 32;

// Tags: clasifican objetos para que los componentes GENERICOS del motor (Hazard,
// Collectible, Checkpoint, KillZone) sepan a quien afectan sin conocer este juego.
static const char* TAG_PLAYER = "Player";
static const char* TAG_PICKUP = "Pickup";
static const char* TAG_HAZARD = "Hazard";

// Capas de dibujo (GameObject::sortingOrder). Menor = mas al fondo.
enum Layer { LAYER_BG = -100, LAYER_TILEMAP = -10, LAYER_ITEMS = 0,
             LAYER_PLAYER = 10, LAYER_HUD = 100 };

// Donde aparece el jugador si el mapa NO trae un objeto PlayerStart.
static const float FALLBACK_SPAWN_X = 0.0f;
static const float FALLBACK_SPAWN_Y = -150.0f;

static const int PLAYER_MAX_HP = 3;

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
        health = gameObject->getComponent<Health>();
    }

    void update(float) override {
        if (!motor) return;

        // Muerto no se controla: se deja de mandar intencion y el Respawn hace el resto.
        if (health && !health->isAlive()) {
            motor->moveInput = 0.0f;
            motor->jumpPressed = false;
            motor->jumpHeld = false;
            return;
        }

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
    Health*          health = nullptr;
};

// --- HUD ------------------------------------------------------------------------
// Vida y frutas son REGLAS DE JUEGO, por eso el conteo vive aqui y no en el motor: el
// TextRenderer solo pinta el string que le pasamos. Cada dato tiene SU PROPIO
// TextRenderer: si se concatenaran en una sola cadena, al cambiar de longitud el
// renglon se recolocaria y parecerian saltar de sitio.
class PlatformerHud : public Component {
public:
    TextRenderer* livesLabel = nullptr;
    TextRenderer* fruitLabel = nullptr;
    TextRenderer* banner     = nullptr;
    Health*       playerHealth = nullptr;

    int collected = 0;
    int total     = 0;

    void add(int n) { collected += n; }
    void setMessage(const std::string& m) { if (banner) banner->setText(m); }

    void update(float) override {
        if (playerHealth && livesLabel && playerHealth->getHP() != shownHP) {
            shownHP = playerHealth->getHP();
            livesLabel->setText("VIDA: " + std::to_string(shownHP) + "/" +
                                std::to_string(playerHealth->maxHP));
        }
        if (fruitLabel && collected != shownFruits) {
            shownFruits = collected;
            fruitLabel->setText("FRUTAS: " + std::to_string(collected) + "/" +
                                std::to_string(total));
        }
    }

private:
    int shownHP     = -1; // distintos de los valores reales: fuerzan el primer refresco
    int shownFruits = -1;
};

// --- Meta del nivel -------------------------------------------------------------
// Condicion de victoria: es de ESTE juego, asi que se queda en game/.
class LevelEnd : public Component {
public:
    PlatformerHud* hud = nullptr;

    void start() override { anim = gameObject->getComponent<SpriteAnimator>(); }

    void onCollision(GameObject* other) override {
        if (done || !other->compareTag(TAG_PLAYER)) return;
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
    player->tag = TAG_PLAYER;   // los componentes del motor filtran por ESTO, no por name
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
    anim->addStripAnimation("hit",  mask + "Hit (32x32).png",  32, 32, 20.0f, false);
    anim->play("idle");

    // Vida y reaparicion. Health no sabe que hacer al morir: se lo dice el juego.
    auto health = player->addComponent<Health>();
    health->maxHP = PLAYER_MAX_HP;
    health->invulnerabilityTime = 1.2f;

    auto resp = player->addComponent<Respawn>();
    resp->delay = 0.7f; // tiempo para que se vea la animacion de muerte
    health->onDeath = [resp] { resp->die(); };

    // Que animacion suena, como REGLAS en vez de una cascada de if. Gana la PRIMERA
    // regla que se cumple, asi que el orden es la prioridad: la muerte manda sobre
    // todo, luego lo del aire (mas especifico), luego correr, y si no se cumple
    // ninguna queda "idle".
    // Va al final para que lea el estado que el motor acaba de calcular en este frame.
    auto fsm = player->addComponent<AnimatorStateMachine>();
    fsm->setDefaultState("idle");
    fsm->addState("hit",  [health] { return !health->isAlive(); });
    fsm->addState("jump", [motor] { return !motor->isGrounded() && motor->isRising(); });
    fsm->addState("fall", [motor] { return !motor->isGrounded(); });
    fsm->addState("run",  [motor] { return motor->moveInput != 0.0f; });

    return player;
}

static void createFruit(Scene& scene, float x, float y,
                        const std::string& kind, PlatformerHud* hud) {
    GameObject* f = scene.createGameObject("Fruit");
    f->tag = TAG_PICKUP;
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

    // El componente Collectible es del MOTOR y no lleva ninguna cuenta: que significa
    // recoger esto lo decide el juego en este callback.
    auto c = f->addComponent<Collectible>();
    c->collectorTag = TAG_PLAYER;
    c->collectAnimation = "collected";
    c->onCollect = [hud](GameObject*) { if (hud) hud->add(1); };
}

// Pinchos: un solo cuadro de 16x16, apoyado en el suelo. Trigger, para que el jugador
// los atraviese en vez de quedarse frenado contra ellos.
static void createSpikes(Scene& scene, float x, float y) {
    GameObject* s = scene.createGameObject("Spikes");
    s->tag = TAG_HAZARD;
    s->sortingOrder = LAYER_ITEMS;
    s->transform->x = x;
    s->transform->y = y;
    s->transform->scaleX = s->transform->scaleY = 4.0f; // 16 px -> 64 px

    s->addComponent<SpriteRenderer>(std::string(TRAPS_DIR) + "Spikes/Idle.png");

    auto col = s->addComponent<BoxCollider>();
    col->width = 56.0f; col->height = 32.0f; col->offsetY = 16.0f; // solo la punta
    col->isTrigger = true;

    auto h = s->addComponent<Hazard>();
    h->damage = 1;
    h->targetTag = TAG_PLAYER;
}

// Sierra: tira de 8 cuadros de 38x38. De momento esta quieta; en la fase 4, con
// PathMover, podra ir y venir por un recorrido.
static void createSaw(Scene& scene, float x, float y) {
    GameObject* s = scene.createGameObject("Saw");
    s->tag = TAG_HAZARD;
    s->sortingOrder = LAYER_ITEMS;
    s->transform->x = x;
    s->transform->y = y;
    s->transform->scaleX = s->transform->scaleY = 2.0f; // 38 px -> 76 px

    s->addComponent<SpriteRenderer>();
    auto anim = s->addComponent<SpriteAnimator>(38, 38, 1);
    anim->addStripAnimation("spin", std::string(TRAPS_DIR) + "Saw/On (38x38).png",
                            38, 38, 20.0f);
    anim->play("spin");

    auto col = s->addComponent<BoxCollider>();
    col->width = 64.0f; col->height = 64.0f;
    col->isTrigger = true;

    auto h = s->addComponent<Hazard>();
    h->damage = 1;
    h->targetTag = TAG_PLAYER;
    h->knockbackX = 360.0f;
}

static void createCheckpoint(Scene& scene, float x, float y) {
    GameObject* c = scene.createGameObject("Checkpoint");
    c->sortingOrder = LAYER_ITEMS;
    c->transform->x = x;
    c->transform->y = y;
    c->transform->scaleX = c->transform->scaleY = 2.0f; // 64 px -> 128 px

    c->addComponent<SpriteRenderer>();
    auto anim = c->addComponent<SpriteAnimator>(64, 64, 1);
    anim->addStripAnimation("noFlag",  std::string(CHECK_DIR) + "Checkpoint (No Flag).png",
                            64, 64, 1.0f);
    anim->addStripAnimation("flagOut", std::string(CHECK_DIR) + "Checkpoint (Flag Out) (64x64).png",
                            64, 64, 20.0f, false);   // 26 cuadros, sin loop
    anim->addStripAnimation("flagIdle", std::string(CHECK_DIR) + "Checkpoint (Flag Idle)(64x64).png",
                            64, 64, 20.0f);          // 10 cuadros, en loop
    anim->play("noFlag");

    auto col = c->addComponent<BoxCollider>();
    col->width = 64.0f; col->height = 96.0f;
    col->isTrigger = true;

    auto cp = c->addComponent<Checkpoint>();
    cp->targetTag = TAG_PLAYER;
    cp->activateAnimation = "flagOut";
    cp->idleAnimation     = "flagIdle";
    // El sprite esta dibujado con el pie abajo; reaparecer un poco por encima del
    // centro evita que el jugador aparezca medio metido en el suelo.
    cp->offsetY = -16.0f;
}

static void createLevelEnd(Scene& scene, float x, float y, PlatformerHud* hud) {
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

// Franja ancha por debajo del nivel: caerse del mapa mata en vez de dejar al jugador
// cayendo para siempre. Se deriva del tamano del mapa, no de numeros cableados.
static void createFallKillZone(Scene& scene, const TilemapRenderer* map) {
    float bottom = map->getOriginY() + map->getWorldHeight();
    GameObject* kz = scene.createGameObject("FallKillZone");
    kz->transform->x = map->getOriginX() + map->getWorldWidth() * 0.5f;
    kz->transform->y = bottom + 200.0f;

    auto col = kz->addComponent<BoxCollider>();
    col->width  = map->getWorldWidth() * 3.0f; // de sobra por los lados
    col->height = 300.0f;
    col->isTrigger = true;

    kz->addComponent<KillZone>()->targetTag = TAG_PLAYER;
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
    // Tamano real de la ventana: el cartel de meta se centra en pantalla y los
    // contadores se pegan a la esquina, sin cablear 1280x720.
    int screenW = 0, screenH = 0;
    SDL_GetCurrentRenderOutputSize(scene.getRenderer(), &screenW, &screenH);

    GameObject* hudObj = scene.createGameObject("HUD");
    hudObj->sortingOrder = LAYER_HUD;
    hudObj->transform->x = 24.0f;  // coordenadas de PANTALLA (screenSpace)
    hudObj->transform->y = 32.0f;
    auto livesLabel = hudObj->addComponent<TextRenderer>();
    livesLabel->screenSpace = true;
    // Anclado a la IZQUIERDA: los contadores cambian de longitud, y con el anclaje al
    // centro se moverian solos.
    livesLabel->align = TextAlign::Left;
    livesLabel->setFont(scene.getAssets().loadFont(HUD_FONT, HUD_SIZE));
    livesLabel->setColor(TextColor{ 255, 255, 255, 255 });

    // Cada contador va en su propio objeto: un GameObject solo puede llevar UN
    // componente de cada tipo, y ademas cada uno necesita su propia posicion.
    GameObject* fruitObj = scene.createGameObject("HUDFruits");
    fruitObj->sortingOrder = LAYER_HUD;
    fruitObj->transform->x = 24.0f;
    fruitObj->transform->y = 76.0f;
    auto fruitLabel = fruitObj->addComponent<TextRenderer>();
    fruitLabel->screenSpace = true;
    fruitLabel->align = TextAlign::Left;
    fruitLabel->setFont(scene.getAssets().loadFont(HUD_FONT, HUD_SIZE));
    fruitLabel->setColor(TextColor{ 255, 255, 255, 255 });

    // Cartel de meta: centrado arriba, vacio hasta que haga falta (un TextRenderer sin
    // texto no dibuja nada).
    GameObject* bannerObj = scene.createGameObject("HUDBanner");
    bannerObj->sortingOrder = LAYER_HUD;
    bannerObj->transform->x = screenW * 0.5f;
    bannerObj->transform->y = 120.0f;
    auto banner = bannerObj->addComponent<TextRenderer>();
    banner->screenSpace = true;
    banner->align = TextAlign::Center;
    banner->setFont(scene.getAssets().loadFont(HUD_FONT, HUD_SIZE));
    banner->setColor(TextColor{ 255, 232, 96, 255 }); // amarillo, para que destaque

    auto hud = hudObj->addComponent<PlatformerHud>();
    hud->livesLabel = livesLabel;
    hud->fruitLabel = fruitLabel;
    hud->banner     = banner;

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
    hud->playerHealth = player->getComponent<Health>();

    // Segunda pasada: el resto del contenido.
    for (const TiledObject& o : objects) {
        float wx, wy;
        tiledToWorld(tm, o.cx, o.cy, wx, wy);

        if (o.type == "Fruit") {
            // Propiedad "fruit" del objeto en Tiled: Apple, Bananas, Cherries, Kiwi,
            // Melon, Orange, Pineapple o Strawberry. Si falta, cae en Apple.
            createFruit(scene, wx, wy, o.getString("fruit", "Apple"), hud);
            ++fruitCount;
        } else if (o.type == "Spikes") {
            createSpikes(scene, wx, wy);
        } else if (o.type == "Saw") {
            createSaw(scene, wx, wy);
        } else if (o.type == "Checkpoint") {
            createCheckpoint(scene, wx, wy);
        } else if (o.type == "LevelEnd") {
            createLevelEnd(scene, wx, wy, hud);
        } else if (o.type != "PlayerStart" && !o.type.empty()) {
            SDL_Log("buildPlatformer: objeto de Tiled con type '%s' sin fabrica; se ignora.",
                    o.type.c_str());
        }
    }
    hud->total = fruitCount;

    // Caerse del mapa mata (y el Respawn devuelve al ultimo checkpoint).
    createFallKillZone(scene, tm);

    // --- Camara ------------------------------------------------------------------
    GameObject* cam = scene.createGameObject("MainCamera");
    cam->addComponent<Camera>();
    auto f = cam->addComponent<FollowCamera>();
    f->setTarget(player);
    f->deadZoneWidth = 200.0f; f->deadZoneHeight = 200.0f;
    f->lookAhead = 120.0f;             // adelanta la vista hacia donde corre
    f->setBoundsFromTilemap(tm);       // y nunca se sale del nivel
}
