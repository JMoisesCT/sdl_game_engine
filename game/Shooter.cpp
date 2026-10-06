#include "Shooter.h"

#include <SDL3/SDL.h>
#include <cmath>
#include <string>

#include "../engine/Scene.h"
#include "../engine/GameObject.h"
#include "../engine/Component.h"
#include "../engine/Transform.h"
#include "../engine/Input.h"
#include "../engine/SpriteRenderer.h"
#include "../engine/TilemapRenderer.h"
#include "../engine/TilemapCollider.h"
#include "../engine/LevelData.h"
#include "../engine/RigidBody2D.h"
#include "../engine/BoxCollider.h"
#include "../engine/Camera.h"
#include "../engine/Lifetime.h"
#include "../engine/TextRenderer.h"

// --- Contenido del nivel (rutas del lado del JUEGO, no del motor) ---------------
// El nivel son DOS archivos: el mapa de Tiled (el fondo) y el .level.json (objetos y
// camara), que ademas dice que mapa usa. Por eso aqui solo se nombra el segundo.
const char* SHOOTER_LEVEL_FILE = "assets/maps/shmup_level1.level.json";

// --- Asset pack: Kenney Pixel Shmup (instalado a mano en assets/) -------------
// Hoja de naves: grilla de 4 columnas x 6 filas, cada celda de 32x32 px. Las naves
// apuntan hacia ARRIBA (shmup vertical: no se rota el sprite). Filas 0,1,2 = naves
// a color; filas 3,4,5 = las mismas naves en gris. La celda se elige por (columna,
// fila) y se recorta con setSourceRect; el indice NO es lineal, se calcula a mano.
static const char* SHIPS_SHEET = "assets/kenney_pixelshmup/Tilemap/ships_packed.png";
static const int   SHIP_CELL   = 32; // tamano de cada celda de nave en la hoja

// Tileset tiles_packed.png (mismo del fondo): grilla de 12x10 celdas de 16x16.
// Los TRES PRIMEROS tiles (indices 0,1,2 = columnas 0,1,2 de la fila 0) son balas.
// OJO: esta ruta debe ser EXACTAMENTE la que arma loadFromTiledJson (carpeta del
// .json + el "image" relativo), porque el AssetManager cachea por cadena de ruta.
// Usando la misma cadena compartimos la textura ya cargada por el TilemapRenderer
// (no se carga ni se duplica una segunda vez).
static const char* TILES_SHEET = "assets/maps/../kenney_pixelshmup/Tilemap/tiles_packed.png";
static const int   TILE_CELL   = 16; // tamano de cada celda del tileset

// Tags: clasifican objetos para que los filtros no dependan del nombre de una
// instancia concreta.
static const char* TAG_PLAYER = "Player";
static const char* TAG_ENEMY  = "Enemy";
static const char* TAG_BULLET = "Bullet";
static const char* TAG_PICKUP = "Pickup";

// Fuente del HUD. Ruta manual, misma convencion que las texturas. Tamanio potencia
// de 2 para que la fuente pixel se vea nitida (sin reescalado feo).
static const char* HUD_FONT    = "assets/ninja_adventure/Ui/Font/NormalFont.ttf";
static const int   HUD_SIZE    = 32;

// Capas de dibujo (GameObject::sortingOrder). Menor = mas al fondo.
enum Layer { LAYER_TILEMAP = -10, LAYER_ITEMS = 0, LAYER_SHIPS = 10, LAYER_HUD = 100 };

// Escala del mundo: tile de 16 px -> 48 px. Las naves y balas usan su propia escala.
static const float WORLD_SCALE = 3.0f;
// Borde superior del mapa en el mundo (el mapa se centra en X; ver buildShooterLevel).
static const float MAP_TOP_Y   = -300.0f;

// --- Estado de la partida -------------------------------------------------------
// El puntaje es una regla de ESTE juego: vive aqui, no en el motor ni en los objetos.
// Los enemigos lo suben al morir y el HUD lo muestra, sin punteros entre ellos. Se
// reinicia cada vez que se construye el nivel (al empezar, y cada Stop/Play del editor).
struct ShooterState {
    int score = 0;
};
static ShooterState state;

// Recorte (x,y,w,h) de la celda (col,fil) de la hoja de naves.
static void setShipCell(SpriteRenderer* sr, int col, int row) {
    sr->setSourceRect(col * SHIP_CELL, row * SHIP_CELL, SHIP_CELL, SHIP_CELL);
}

// Bordes del viewport en coordenadas de MUNDO, a partir de la camara activa. La
// camara marca el centro; sumamos/restamos media pantalla escalada por el zoom.
// Lo usan el player (clamp) y los EnemySpawn (para despertar). Devuelve false si aun
// no hay camara activa.
static bool cameraViewBounds(Scene& scene, float& left, float& right,
                             float& top, float& bottom) {
    Camera* cam = scene.getActiveCamera();
    if (!cam) return false;
    int w = 0, h = 0;
    SDL_GetCurrentRenderOutputSize(scene.getRenderer(), &w, &h);
    float zoom  = cam->getZoom();
    float camx  = cam->gameObject->transform->x;
    float camy  = cam->gameObject->transform->y;
    float halfW = (w * 0.5f) / zoom;
    float halfH = (h * 0.5f) / zoom;
    left   = camx - halfW; right  = camx + halfW;
    top    = camy - halfH; bottom = camy + halfH;
    return true;
}

// --- Scroll del shmup vertical -----------------------------------------------
// NO es un componente del motor: es configuracion y logica de ESTE juego, por eso
// vive en Shooter.cpp. Mueve la camara a velocidad constante en -y (la vista sube
// por el mapa hacia el final del nivel, que esta en y chico). El TilemapRenderer ya
// dibuja a traves de la camara con culling, asi que el fondo se desplaza solo al
// mover la camara: no hace falta ningun ScrollingBackground.
//
// Posible componente futuro del MOTOR: un ScrollCamera generico con velocidad y eje
// configurables. No se crea aun para no meter logica de juego en engine/.
static const float SCROLL_SPEED = 90.0f; // px/seg de mundo; la camara avanza en -y

class CameraScroll : public Component {
public:
    float speed = SCROLL_SPEED;
    void update(float dt) override {
        gameObject->transform->y -= speed * dt; // sube por el mapa (hacia y=0)
    }
};

// --- HUD: contador de puntaje (LOGICA DE JUEGO, no del motor) ----------------
// Solo LEE el puntaje del estado de la partida y, cuando cambia, formatea
// "SCORE: N" y se lo pasa al TextRenderer (que solo dibuja el string).
class HudScore : public Component {
public:
    TextRenderer* label = nullptr; // el renderer del texto (screenSpace)

    void update(float) override {
        // Solo tocamos el texto cuando el puntaje cambia (el TextRenderer ademas
        // tiene su propio dirty flag; aqui evitamos incluso rearmar el string).
        if (state.score == lastShown) return;
        lastShown = state.score;
        if (label) label->setText("SCORE: " + std::to_string(state.score));
    }
private:
    int lastShown = -1; // distinto del puntaje al inicio: fuerza el primer refresco
};

// Destruye su objeto (y al otro) cuando choca con algo de cierto TAG. Si 'points' es
// mayor que 0, los suma al puntaje en ese choque (lo usamos solo en el enemigo, para
// contar una vez por nave derribada por una bala).
//
// Antes filtraba por 'name', que identifica una instancia; el tag clasifica. Con tags,
// dos tipos de bala distintos siguen contando como "bala" sin tocar este componente.
class DestroyOnHit : public Component {
public:
    std::string targetTag;
    int         points = 0;

    void onCollision(GameObject* other) override {
        if (other->compareTag(targetTag)) {
            state.score += points;
            gameObject->scene->destroy(gameObject);
            gameObject->scene->destroy(other);
        }
    }
};

// --- Enemigos ----------------------------------------------------------------
// Crea una nave enemiga gris en la posicion de MUNDO dada. La nave queda quieta
// (salvo 'speed'): "baja" en pantalla porque la camara sube. 'speed' agrega
// velocidad extra en +y (mundo hacia abajo).
static GameObject* spawnEnemy(Scene& scene, float wx, float wy,
                              int shipCol, int shipRow, float speed) {
    GameObject* e = scene.createGameObject("Enemigo");
    e->tag = TAG_ENEMY;
    e->sortingOrder = LAYER_SHIPS;
    e->transform->x = wx;
    e->transform->y = wy;
    e->transform->scaleX = e->transform->scaleY = 2.5f;
    auto sr = e->addComponent<SpriteRenderer>(SHIPS_SHEET);
    setShipCell(sr, shipCol, shipRow);
    auto rb = e->addComponent<RigidBody2D>();
    rb->gravityScale = 0.0f;   // shmup: sin gravedad
    rb->velocityY = speed;     // caida extra sobre el desplazamiento de la camara
    auto c = e->addComponent<BoxCollider>();
    c->width = c->height = 80.0f;
    c->isTrigger = true;
    // Al recibir una bala se destruye y suma puntos (una vez por nave).
    auto hit = e->addComponent<DestroyOnHit>();
    hit->targetTag = TAG_BULLET;
    hit->points    = 100;
    // Red de seguridad: si escapa por abajo sin recibir bala, se limpia solo.
    e->addComponent<Lifetime>()->seconds = 12.0f;
    return e;
}

// Un EnemySpawn del nivel: la nave enemiga DORMIDA. Existe desde que se construye el
// nivel (asi el editor la muestra y se puede arrastrar), pero no hace nada hasta que la
// camara llega a ella: entonces suelta la nave de verdad en su sitio y desaparece. Si
// ya quedo por DEBAJO de la vista (jugando desde el cursor, mas arriba en el nivel), se
// va sin soltar nada: esa parte del nivel ya paso.
class EnemySpawnPoint : public Component {
public:
    int   shipCol = 0;
    int   shipRow = 3;       // 3,4,5 = naves grises
    float speed   = 60.0f;
    float margin  = 60.0f;   // adelanto: la suelta un poco antes de que asome

    void update(float) override {
        Scene& scene = *gameObject->scene;
        float left, right, top, bottom;
        if (!cameraViewBounds(scene, left, right, top, bottom)) return;
        float y = gameObject->transform->y;
        if (y < top - margin) return; // todavia no asoma por arriba
        if (y <= bottom + margin)
            spawnEnemy(scene, gameObject->transform->x, y, shipCol, shipRow, speed);
        scene.destroy(gameObject);
    }
};

// --- Jugador -----------------------------------------------------------------
// Mueve la nave con input DENTRO del viewport, la mantiene pegada a la camara y
// dispara balas hacia arriba. Sin gravedad ni fisica: se mueve por Transform.
class ShooterController : public Component {
public:
    float speed = 260.0f; // px/seg del movimiento dentro de la pantalla
    void update(float dt) override {
        Transform* t = gameObject->transform;

        // Input: 4 direcciones, movimiento por Transform directo (sin RigidBody).
        float mx = Input::axis(Key::Left, Key::Right);
        float my = Input::axis(Key::Up,   Key::Down);
        t->x += mx * speed * dt;
        t->y += my * speed * dt;

        // La nave acompana a la camara: le sumo el MISMO delta que CameraScroll (-y).
        // Sin esto la camara subiria y la dejaria atras, saliendo por abajo.
        t->y -= SCROLL_SPEED * dt;

        // Clamp al viewport (derivado de la camara) para que no salga de pantalla.
        // El margen es ~medio sprite del player para que quepa completo.
        float left, right, top, bottom;
        if (cameraViewBounds(*gameObject->scene, left, right, top, bottom)) {
            const float m = 48.0f; // ~ medio sprite del player (32 * escala 3 / 2)
            if (t->x < left + m)   t->x = left + m;
            if (t->x > right - m)  t->x = right - m;
            if (t->y < top + m)    t->y = top + m;
            if (t->y > bottom - m) t->y = bottom - m;
        }

        // Un disparo por pulsacion: el flanco lo da Input, no una copia local del
        // estado del frame anterior.
        if (Input::wasPressed(Key::Space)) shoot();
    }
private:
    void shoot() {
        Scene* scene = gameObject->scene;
        GameObject* bala = scene->createGameObject("Bala");
        bala->tag = TAG_BULLET;
        bala->sortingOrder = LAYER_SHIPS;
        bala->transform->x = gameObject->transform->x;
        bala->transform->y = gameObject->transform->y - 40.0f;
        bala->transform->scaleX = bala->transform->scaleY = 2.5f; // 16px -> 40px en mundo

        // Bala del jugador: tile indice 0 (columna 0, fila 0) del tiles_packed.png,
        // recorte de 16x16 anclado al centro como el resto de sprites. Reusa la
        // textura ya cacheada del fondo (misma cadena de ruta, ver TILES_SHEET).
        // Los indices 1 y 2 (columnas 1 y 2 de la fila 0) son las otras dos balas:
        // quedan disponibles para balas alternativas o de enemigos.
        auto s = bala->addComponent<SpriteRenderer>(TILES_SHEET);
        s->setSourceRect(0, 0, TILE_CELL, TILE_CELL);
        auto rb = bala->addComponent<RigidBody2D>();
        rb->gravityScale = 0.0f;
        rb->velocityY = -500.0f; // sube en mundo (y hacia el final del nivel)
        auto c = bala->addComponent<BoxCollider>();
        c->width = c->height = 24.0f;
        c->isTrigger = true;
        // Se autodestruye al salir del viewport: como sube mas rapido que la camara,
        // el Lifetime alcanza para limpiarla tras cruzar el borde superior.
        bala->addComponent<Lifetime>()->seconds = 2.0f;
        bala->addComponent<DestroyOnHit>()->targetTag = TAG_ENEMY;
    }
};

// ================================================================================
// OBJETOS DEL NIVEL
// Cada type del .level.json tiene aqui su funcion setup: la RECETA del objeto. El motor
// (spawnLevelObjects) ya creo el GameObject, lo puso en su posicion y le dio el id que
// usa el editor; el setup solo le agrega componentes, tag, escala y capa de dibujo.
// Para agregar un objeto nuevo al juego: escribir su setup y sumarlo al catalogo
// (shooterObjects, mas abajo). Despues se coloca con el editor (F2 -> Nuevo).
// ================================================================================

// PlayerStart: la nave del jugador (celda a color col 0, fila 0).
static void setupPlayer(GameObject* player, const LevelObject&) {
    player->name = "Player";
    player->tag = TAG_PLAYER;
    player->sortingOrder = LAYER_SHIPS;
    player->transform->scaleX = player->transform->scaleY = 3.0f;
    auto sr = player->addComponent<SpriteRenderer>(SHIPS_SHEET);
    setShipCell(sr, 0, 0); // nave del jugador: columna 0, fila 0 (a color)
    // Sin RigidBody2D: el player se mueve por Transform (input + scroll + clamp),
    // no por fisica. El collider es TRIGGER para que los tiles solidos del fondo no
    // lo empujen al scrollear (en shmup el fondo no bloquea la nave).
    auto col = player->addComponent<BoxCollider>();
    col->width = 60.0f; col->height = 60.0f;
    col->isTrigger = true;
    player->addComponent<ShooterController>();
}

// EnemySpawn: nave enemiga gris (filas 3-5 de la hoja). Propiedades:
//   shipCol, shipRow -> celda 32x32 de la hoja de naves.
//   speed            -> velocidad extra de caida.
// Se ve igual que la nave que va a soltar, para que en el editor se vea lo que vendra.
static void setupEnemySpawn(GameObject* e, const LevelObject& o) {
    e->sortingOrder = LAYER_SHIPS;
    e->transform->scaleX = e->transform->scaleY = 2.5f; // la misma escala que spawnEnemy

    auto spawn = e->addComponent<EnemySpawnPoint>();
    spawn->shipCol = (int)o.getNumber("shipCol", 0.0);
    spawn->shipRow = (int)o.getNumber("shipRow", 3.0);
    spawn->speed   = (float)o.getNumber("speed", 60.0);

    auto sr = e->addComponent<SpriteRenderer>(SHIPS_SHEET);
    setShipCell(sr, spawn->shipCol, spawn->shipRow);

    // Del tamanio de la nave, para que en el editor se seleccione por su tamanio real.
    // Sin tag: dormida, nada reacciona a ella (las balas buscan TAG_ENEMY).
    auto c = e->addComponent<BoxCollider>();
    c->width = c->height = 80.0f;
    c->isTrigger = true;
}

// PowerUp: propiedad "kind" = tipo de efecto. Por ahora solo el sprite placeholder y
// un collider trigger.
static void setupPowerUp(GameObject* p, const LevelObject& o) {
    p->tag = TAG_PICKUP;
    p->sortingOrder = LAYER_ITEMS;
    p->transform->scaleX = p->transform->scaleY = 2.5f;
    // Placeholder visual: un tile cualquiera del tiles_packed.png (elige otra
    // celda cuando haya arte). Reusa la textura del fondo (misma cadena de ruta).
    auto sr = p->addComponent<SpriteRenderer>(TILES_SHEET);
    sr->setSourceRect(6 * TILE_CELL, 5 * TILE_CELL, TILE_CELL, TILE_CELL);
    auto c = p->addComponent<BoxCollider>();
    c->width = c->height = 32.0f;
    c->isTrigger = true;
    // TODO: aplicar el efecto segun 'kind' cuando la nave lo recoja.
    (void)o.getString("kind");
}

// TriggerZone: zona invisible con un collider trigger del tamanio del objeto (ya en el
// mundo). Propiedad "event", p. ej. "boss" / "levelend".
static void setupTriggerZone(GameObject* z, const LevelObject& o) {
    auto c = z->addComponent<BoxCollider>();
    c->width  = o.w;
    c->height = o.h;
    c->isTrigger = true;
    // TODO: disparar el evento 'event' cuando la nave entre en la zona.
    (void)o.getString("event");
}

// --- Catalogo: los objetos que existen en este juego ------------------------------
// La MISMA lista la usan la fabrica (para crear cada objeto del archivo) y el editor
// (para ofrecerlos en "Nuevo", con sus propiedades por defecto).
ObjectCatalog shooterObjects() {
    ObjectCatalog c;
    c.add("PlayerStart", setupPlayer, "Donde empieza la nave del jugador").playerSpawn();
    c.add("EnemySpawn", setupEnemySpawn, "Nave enemiga: aparece cuando la camara llega a ella")
        .number("shipCol", 0).number("shipRow", 3).number("speed", 60);
    c.add("PowerUp", setupPowerUp, "Mejora (aun sin efecto)")
        .choice("kind", { "DoubleAttack", "Shield", "Speed" });
    c.add("TriggerZone", setupTriggerZone, "Zona invisible que dispara un evento (aun sin efecto)")
        .size(100, 50)
        .choice("event", { "boss", "levelend" });
    return c;
}

// ================================================================================
// LO FIJO DEL NIVEL (no viene del archivo: es igual en cualquier nivel del juego)
// ================================================================================

void buildShooter(Scene& scene) {
    // Si el archivo falla, el nivel sale vacio (sin mapa ni objetos) pero el juego no se
    // cae: queda el log para saber por que.
    LevelData level;
    if (!loadLevel(SHOOTER_LEVEL_FILE, level))
        SDL_Log("buildShooter: no se pudo cargar %s", SHOOTER_LEVEL_FILE);
    buildShooterLevel(scene, level);
}

void buildShooterLevel(Scene& scene, const LevelData& level) {
    state = ShooterState(); // partida nueva: puntaje en 0

    // --- Fondo: el mapa de Tiled ------------------------------------------------
    // Su "image" apunta al tiles_packed.png del pack (tileset 12x10 de tiles de 16x16).
    GameObject* world = scene.createGameObject("World");
    world->sortingOrder = LAYER_TILEMAP;
    // El Transform marca el ORIGEN del mapa (esquina superior izquierda de la celda 0,0).
    world->transform->scaleX = world->transform->scaleY = WORLD_SCALE;
    auto map = world->addComponent<TilemapRenderer>(); // modo archivo: el tileset lo da el mapa
    // El TilemapRenderer solo dibuja; el TilemapCollider da la colision de tiles.
    world->addComponent<TilemapCollider>();
    if (!map->loadFromTiledJson(level.mapPath))
        SDL_Log("buildShooter: no se pudo cargar el mapa '%s'", level.mapPath.c_str());

    // Centrar el mapa en X alrededor del origen, leyendo su ancho del propio mapa. En Y
    // es muy alto (100 tiles): su borde superior va en MAP_TOP_Y. Va ANTES de crear los
    // objetos: la conversion mapa -> mundo usa este Transform.
    world->transform->x = -map->getWorldWidth() * 0.5f;
    world->transform->y = MAP_TOP_Y;

    // --- Objetos del nivel ---------------------------------------------------------
    // Todo lo que esta en el .level.json (nave, enemigos, power-ups, zonas), cada uno
    // armado por el setup de su type en el catalogo. Es lo que se coloca con el editor.
    spawnLevelObjects(scene, level, map, shooterObjects());

    // El nivel deberia traer un PlayerStart. Si no lo tiene, una nave por defecto abajo
    // y al centro del mapa, para que el ejemplo siga siendo jugable.
    GameObject* player = scene.findWithTag(TAG_PLAYER);
    if (!player) {
        SDL_Log("buildShooter: el nivel no trae ningun PlayerStart; uso una nave por defecto");
        player = scene.createGameObject("Player");
        player->transform->x = map->getOriginX() + map->getWorldWidth() * 0.5f;
        player->transform->y = map->getOriginY() + map->getWorldHeight() - 200.0f;
        setupPlayer(player, LevelObject());
    }

    // --- Camara con scroll vertical --------------------------------------------
    // Posicionamiento INICIAL: solo el PUNTO DE PARTIDA; desde aqui el CameraScroll
    // sigue bajando la y y el player la acompana en su update. Se hace al FINAL del
    // setup a proposito, porque necesita el mapa ya cargado (para su ancho) y el
    // player ya ubicado (para la y). La Camera marca el CENTRO de la pantalla.
    int winW = 0, winH = 0;
    SDL_GetCurrentRenderOutputSize(scene.getRenderer(), &winW, &winH);
    GameObject* cam = scene.createGameObject("MainCamera");
    Camera* camera = cam->addComponent<Camera>();
    cam->addComponent<CameraScroll>();
    // El nivel puede cambiar el zoom (seccion "camera"); no hay FollowCamera.
    applyCameraSettings(level, camera, nullptr);

    // Margen (px de pantalla) entre el player y el borde INFERIOR del viewport: a
    // menor margen, mas pegado abajo queda (mas espacio arriba para ver enemigos
    // entrando, propio de un shmup vertical). Ajustable.
    const float PLAYER_BOTTOM_MARGIN = 80.0f;

    // Trap del zoom: el viewport EFECTIVO se divide por el zoom de la camara. Con
    // zoom != 1, usar winH crudo dejaria el encuadre corrido.
    float zoom = camera->getZoom();
    float halfViewportEff = (winH / zoom) * 0.5f;

    // Eje x: CENTRO del mapa en el mundo (origen + medio ancho).
    // Eje y: derivada del player, dejandolo en la zona baja del viewport (con espacio
    // arriba). Alternativa simple para centrarlo vertical: cam.y = player->transform->y.
    // Redondeo a entero (pixel art: evitar bleeding sub-pixel desde el primer frame).
    cam->transform->x = std::round(map->getOriginX() + map->getWorldWidth() * 0.5f);
    cam->transform->y = std::round(player->transform->y - halfViewportEff + PLAYER_BOTTOM_MARGIN);

    // --- HUD: puntaje -----------------------------------------------------------
    // El TextRenderer es screenSpace: coordenadas de pantalla fijas, no scrollea con el
    // fondo. Anclado a la IZQUIERDA: el numero cambia de longitud, y con el anclaje al
    // centro el texto se moveria solo.
    GameObject* hudObj = scene.createGameObject("HUD");
    hudObj->sortingOrder = LAYER_HUD;
    hudObj->transform->x = 24.0f;
    hudObj->transform->y = 36.0f;
    auto label = hudObj->addComponent<TextRenderer>();
    label->screenSpace = true;
    label->align = TextAlign::Left;
    label->setFont(scene.getAssets().loadFont(HUD_FONT, HUD_SIZE));
    label->setColor(TextColor{ 255, 255, 255, 255 }); // blanco
    hudObj->addComponent<HudScore>()->label = label;
}
