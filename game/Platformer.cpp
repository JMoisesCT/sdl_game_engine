#include "Platformer.h"

#include <SDL3/SDL.h>
#include <string>

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
#include "../engine/LevelData.h"
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
// El nivel son DOS archivos: el mapa de Tiled (terreno) y el .level.json (objetos y
// camara), que ademas dice que mapa usa. Por eso aqui solo se nombra el segundo.
const char* PLATFORMER_LEVEL_FILE = "assets/maps/platformer_level1.level.json";
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

static const int PLAYER_MAX_HP = 3;

// --- Estado de la partida -------------------------------------------------------
// Las REGLAS de este juego (cuantas frutas hay, cuantas se recogieron, si se llego a la
// meta) viven aqui, no en el motor ni en los objetos. Los objetos lo modifican (la fruta
// al recogerse, la meta al tocarla) y el HUD lo muestra. Asi ningun objeto necesita un
// puntero a otro: la fruta no conoce al HUD. Se reinicia cada vez que se construye el
// nivel (al empezar, y cada Stop/Play del editor).
struct PlatformerState {
    int  fruits      = 0;     // recogidas
    int  totalFruits = 0;     // las que hay en el nivel (las cuenta el setup de cada fruta)
    bool levelDone   = false; // se toco la meta
};
static PlatformerState state;

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
// Muestra el estado de la partida y la vida del jugador. No lleva cuentas: solo LEE
// (state y Health) y refresca el texto cuando algo cambia. Cada dato tiene SU PROPIO
// TextRenderer: si se concatenaran en una sola cadena, al cambiar de longitud el
// renglon se recolocaria y parecerian saltar de sitio.
class PlatformerHud : public Component {
public:
    TextRenderer* livesLabel = nullptr;
    TextRenderer* fruitLabel = nullptr;
    TextRenderer* banner     = nullptr;
    Health*       playerHealth = nullptr;

    void update(float) override {
        if (playerHealth && livesLabel && playerHealth->getHP() != shownHP) {
            shownHP = playerHealth->getHP();
            livesLabel->setText("VIDA: " + std::to_string(shownHP) + "/" +
                                std::to_string(playerHealth->maxHP));
        }
        if (fruitLabel && state.fruits != shownFruits) {
            shownFruits = state.fruits;
            fruitLabel->setText("FRUTAS: " + std::to_string(state.fruits) + "/" +
                                std::to_string(state.totalFruits));
        }
        if (banner && state.levelDone && !shownDone) {
            shownDone = true;
            banner->setText("NIVEL COMPLETADO");
        }
    }

private:
    int  shownHP     = -1; // distintos de los valores reales: fuerzan el primer refresco
    int  shownFruits = -1;
    bool shownDone   = false;
};

// --- Meta del nivel -------------------------------------------------------------
// Condicion de victoria: es de ESTE juego, asi que se queda en game/.
class LevelEnd : public Component {
public:
    void start() override { anim = gameObject->getComponent<SpriteAnimator>(); }

    void onCollision(GameObject* other) override {
        if (state.levelDone || !other->compareTag(TAG_PLAYER)) return;
        state.levelDone = true;
        if (anim) anim->play("pressed"); // clip de un solo uso: se queda en el ultimo cuadro
    }

private:
    SpriteAnimator* anim = nullptr;
};

// ================================================================================
// OBJETOS DEL NIVEL
// Cada type del .level.json tiene aqui su funcion setup: la RECETA del objeto. El motor
// (spawnLevelObjects) ya creo el GameObject, lo puso en su posicion y le dio el id que
// usa el editor; el setup solo le agrega componentes, tag, escala y capa de dibujo.
// Para agregar un objeto nuevo al juego: escribir su setup y sumarlo al catalogo
// (platformerObjects, mas abajo). Despues se coloca con el editor (F2 -> Nuevo).
// ================================================================================

// PlayerStart: el propio jugador. Arrastrarlo en el editor mueve su punto de aparicion.
static void setupPlayer(GameObject* player, const LevelObject&) {
    player->name = "Player";
    player->tag = TAG_PLAYER;   // los componentes del motor filtran por ESTO, no por name
    player->sortingOrder = LAYER_PLAYER;
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
}

// Fruit: propiedad "fruit" = nombre del PNG (Apple, Bananas, Cherries...).
static void setupFruit(GameObject* f, const LevelObject& o) {
    f->tag = TAG_PICKUP;
    f->sortingOrder = LAYER_ITEMS;
    f->transform->scaleX = f->transform->scaleY = 2.0f; // 32 px -> 64 px

    f->addComponent<SpriteRenderer>();
    auto anim = f->addComponent<SpriteAnimator>(32, 32, 1);
    // Cada fruta es una tira de 17 cuadros de 32x32; "Collected" es el efecto comun
    // de 6 cuadros, y va SIN loop para poder destruir el objeto cuando termina.
    const std::string kind = o.getString("fruit", "Apple");
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
    c->onCollect = [](GameObject*) { ++state.fruits; };

    ++state.totalFruits; // cada fruta que se arma cuenta para el total del HUD
}

// Spikes: un solo cuadro de 16x16, apoyado en el suelo. Trigger, para que el jugador
// los atraviese en vez de quedarse frenado contra ellos.
static void setupSpikes(GameObject* s, const LevelObject&) {
    s->tag = TAG_HAZARD;
    s->sortingOrder = LAYER_ITEMS;
    s->transform->scaleX = s->transform->scaleY = 4.0f; // 16 px -> 64 px

    s->addComponent<SpriteRenderer>(std::string(TRAPS_DIR) + "Spikes/Idle.png");

    auto col = s->addComponent<BoxCollider>();
    col->width = 56.0f; col->height = 32.0f; col->offsetY = 16.0f; // solo la punta
    col->isTrigger = true;

    auto h = s->addComponent<Hazard>();
    h->damage = 1;
    h->targetTag = TAG_PLAYER;
}

// Saw: tira de 8 cuadros de 38x38. De momento esta quieta; en la fase 4, con
// PathMover, podra ir y venir por un recorrido.
static void setupSaw(GameObject* s, const LevelObject&) {
    s->tag = TAG_HAZARD;
    s->sortingOrder = LAYER_ITEMS;
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

// Checkpoint: bandera; al tocarla, ahi reaparece el jugador.
static void setupCheckpoint(GameObject* c, const LevelObject&) {
    c->sortingOrder = LAYER_ITEMS;
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

// LevelEnd: la meta. Al tocarla se completa el nivel (ver LevelEnd y el HUD).
static void setupLevelEnd(GameObject* e, const LevelObject&) {
    e->sortingOrder = LAYER_ITEMS;
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

    e->addComponent<LevelEnd>();
}

// --- Catalogo: los objetos que existen en este juego ------------------------------
// Una linea por type: el nombre que se guarda en el .level.json, la funcion que lo arma
// y una ayuda. La MISMA lista la usan la fabrica (para crear cada objeto del archivo) y
// el editor (para ofrecerlos en "Nuevo", con sus propiedades por defecto).
ObjectCatalog platformerObjects() {
    ObjectCatalog c;
    c.add("PlayerStart", setupPlayer, "Donde aparece el jugador al empezar").playerSpawn();
    c.add("Fruit", setupFruit, "Fruta coleccionable (cuenta en el HUD)")
        .choice("fruit", { "Apple", "Bananas", "Cherries", "Kiwi",
                           "Melon", "Orange", "Pineapple", "Strawberry" });
    c.add("Spikes",     setupSpikes,     "Pinchos en el suelo: quitan 1 de vida y empujan");
    c.add("Saw",        setupSaw,        "Sierra giratoria: quita 1 de vida y empuja");
    c.add("Checkpoint", setupCheckpoint, "Bandera: al tocarla, ahi reaparece el jugador");
    c.add("LevelEnd",   setupLevelEnd,   "Meta del nivel").single();
    return c;
}

// ================================================================================
// LO FIJO DEL NIVEL (no viene del archivo: es igual en cualquier nivel del juego)
// ================================================================================

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

// Un texto del HUD en coordenadas de PANTALLA. Cada uno va en su propio objeto: un
// GameObject solo puede llevar UN componente de cada tipo, y ademas cada texto
// necesita su propia posicion.
static TextRenderer* createHudLabel(Scene& scene, const char* name, float x, float y,
                                    TextAlign align, TextColor color) {
    GameObject* obj = scene.createGameObject(name);
    obj->sortingOrder = LAYER_HUD;
    obj->transform->x = x;
    obj->transform->y = y;
    auto label = obj->addComponent<TextRenderer>();
    label->screenSpace = true;
    label->align = align;
    label->setFont(scene.getAssets().loadFont(HUD_FONT, HUD_SIZE));
    label->setColor(color);
    return label;
}

void buildPlatformer(Scene& scene) {
    // Si el archivo falla, el nivel sale vacio (sin mapa ni objetos) pero el juego no se
    // cae: queda el log para saber por que.
    LevelData level;
    if (!loadLevel(PLATFORMER_LEVEL_FILE, level))
        SDL_Log("buildPlatformer: no se pudo cargar %s", PLATFORMER_LEVEL_FILE);
    buildPlatformerLevel(scene, level);
}

void buildPlatformerLevel(Scene& scene, const LevelData& level) {
    state = PlatformerState(); // partida nueva: sin frutas, sin meta

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

    // --- Suelo y plataformas (el mapa de Tiled) ------------------------------------
    GameObject* tilemap = scene.createGameObject("Tilemap");
    tilemap->sortingOrder = LAYER_TILEMAP;
    // El Transform marca el ORIGEN del mapa (esquina superior izquierda de la celda 0,0).
    tilemap->transform->x = -960.0f;
    tilemap->transform->y = -262.0f;
    tilemap->transform->scaleX = tilemap->transform->scaleY = 4.0f; // 16px -> 64px por celda
    auto tm = tilemap->addComponent<TilemapRenderer>();
    // Mapa de Tiled (JSON, capas de tiles + tileset embebido). Los tiles solidos se
    // marcan en Tiled con una propiedad booleana "solid"=true en el tileset, y una capa
    // puramente decorativa lleva la propiedad de capa "collision"=false.
    if (!tm->loadFromTiledJson(level.mapPath))
        SDL_Log("buildPlatformer: no se pudo cargar el mapa '%s'", level.mapPath.c_str());
    // El renderer solo DIBUJA; este componente es lo que hace que los tiles frenen.
    tilemap->addComponent<TilemapCollider>();

    // --- Objetos del nivel ---------------------------------------------------------
    // Todo lo que esta en el .level.json (jugador, frutas, trampas, meta...), cada uno
    // armado por el setup de su type en el catalogo. Es lo que se coloca con el editor.
    spawnLevelObjects(scene, level, tm, platformerObjects());

    GameObject* player = scene.findWithTag(TAG_PLAYER);
    if (!player)
        SDL_Log("buildPlatformer: el nivel no trae ningun PlayerStart; no hay jugador.");

    // Caerse del mapa mata (y el Respawn devuelve al ultimo checkpoint).
    createFallKillZone(scene, tm);

    // --- HUD ---------------------------------------------------------------------
    // Tamano real de la ventana: el cartel de meta se centra en pantalla y los
    // contadores se pegan a la esquina, sin cablear 1280x720.
    int screenW = 0, screenH = 0;
    SDL_GetCurrentRenderOutputSize(scene.getRenderer(), &screenW, &screenH);

    const TextColor WHITE{ 255, 255, 255, 255 };
    const TextColor YELLOW{ 255, 232, 96, 255 }; // el cartel de meta, para que destaque
    GameObject* hudObj = scene.createGameObject("HUD");
    auto hud = hudObj->addComponent<PlatformerHud>();
    // Anclados a la IZQUIERDA: los contadores cambian de longitud, y con el anclaje al
    // centro se moverian solos. El cartel va centrado y vacio hasta que haga falta (un
    // TextRenderer sin texto no dibuja nada).
    hud->livesLabel = createHudLabel(scene, "HUDLives",  24.0f, 32.0f, TextAlign::Left, WHITE);
    hud->fruitLabel = createHudLabel(scene, "HUDFruits", 24.0f, 76.0f, TextAlign::Left, WHITE);
    hud->banner     = createHudLabel(scene, "HUDBanner", screenW * 0.5f, 120.0f,
                                     TextAlign::Center, YELLOW);
    if (player) hud->playerHealth = player->getComponent<Health>();

    // --- Camara ------------------------------------------------------------------
    GameObject* cam = scene.createGameObject("MainCamera");
    auto camera = cam->addComponent<Camera>();
    auto f = cam->addComponent<FollowCamera>();
    f->setTarget(player);
    // Valores POR DEFECTO del juego; el archivo del nivel puede cambiar cualquiera de
    // ellos (seccion "camera"), y lo que no traiga se queda como esta aqui.
    f->deadZoneWidth = 200.0f; f->deadZoneHeight = 200.0f;
    f->lookAhead = 120.0f;             // adelanta la vista hacia donde corre
    applyCameraSettings(level, camera, f);
    f->setBoundsFromTilemap(tm);       // y nunca se sale del nivel
}
