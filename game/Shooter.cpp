#include "Shooter.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "../engine/Scene.h"
#include "../engine/GameObject.h"
#include "../engine/Component.h"
#include "../engine/Transform.h"
#include "../engine/SpriteRenderer.h"
#include "../engine/TilemapRenderer.h"
#include "../engine/TiledObjectLayer.h"
#include "../engine/RigidBody2D.h"
#include "../engine/BoxCollider.h"
#include "../engine/Camera.h"
#include "../engine/Lifetime.h"
#include "../engine/TextRenderer.h"

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

// Fuente del HUD. Ruta manual, misma convencion que las texturas. Tamanio potencia
// de 2 para que la fuente pixel se vea nitida (sin reescalado feo).
static const char* HUD_FONT    = "assets/ninja_adventure/Ui/Font/NormalFont.ttf";
static const int   HUD_SIZE    = 32;

// Recorte (x,y,w,h) de la celda (col,fil) de la hoja de naves.
static void setShipCell(SpriteRenderer* sr, int col, int row) {
    sr->setSourceRect(col * SHIP_CELL, row * SHIP_CELL, SHIP_CELL, SHIP_CELL);
}

// Bordes del viewport en coordenadas de MUNDO, a partir de la camara activa. La
// camara marca el centro; sumamos/restamos media pantalla escalada por el zoom.
// Lo usan el player (clamp) y el streamer de enemigos (borde superior). Devuelve
// false si aun no hay camara activa.
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
// El puntaje es de ESTE juego, por eso vive en Shooter.cpp y no en el motor. Este
// componente guarda el numero y, cuando cambia, formatea "SCORE: N" y se lo pasa al
// TextRenderer (que solo dibuja el string). El formateo del numero ocurre aqui, en
// game/, no en el motor.
class HudScore : public Component {
public:
    TextRenderer* label = nullptr; // el renderer del texto (screenSpace)
    int score = 0;

    void bump(int points) { score += points; }

    void update(float) override {
        // Solo tocamos el texto cuando el puntaje cambia (el TextRenderer ademas
        // tiene su propio dirty flag; aqui evitamos incluso rearmar el string).
        if (score == lastShown) return;
        lastShown = score;
        if (label) label->setText("SCORE: " + std::to_string(score));
    }
private:
    int lastShown = -1; // distinto de score al inicio: fuerza el primer refresco
};

// Destruye su objeto (y al otro) cuando choca con algo de cierto nombre. Si se le da
// un HudScore, suma 'points' al puntaje en ese choque (lo usamos solo en el enemigo,
// para contar una vez por nave derribada por una bala).
class DestroyOnHit : public Component {
public:
    std::string targetName;
    HudScore*   scoreOnKill = nullptr; // opcional: a quien sumarle puntos
    int         points      = 100;

    void onCollision(GameObject* other) override {
        if (other->name == targetName) {
            if (scoreOnKill) scoreOnKill->bump(points);
            gameObject->scene->destroy(gameObject);
            gameObject->scene->destroy(other);
        }
    }
};

// --- Enemigos ----------------------------------------------------------------
// Crea una nave enemiga gris en la posicion de MUNDO dada. La nave queda quieta
// (salvo 'speed'): "baja" en pantalla porque la camara sube. 'speed' agrega
// velocidad extra en +y (mundo hacia abajo). Reusado por el streamer.
static GameObject* spawnEnemy(Scene& scene, float wx, float wy,
                              int shipCol, int shipRow, float speed,
                              HudScore* hud) {
    GameObject* e = scene.createGameObject("Enemigo");
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
    // Al recibir una bala se destruye y suma puntos al HUD (una vez por nave).
    auto hit = e->addComponent<DestroyOnHit>();
    hit->targetName  = "Bala";
    hit->scoreOnKill = hud;
    // Red de seguridad: si escapa por abajo sin recibir bala, se limpia solo.
    e->addComponent<Lifetime>()->seconds = 12.0f;
    return e;
}

// Un EnemySpawn del nivel, ya convertido a mundo y pendiente de soltarse.
struct PendingEnemy {
    float worldX  = 0.0f;
    float worldY  = 0.0f;
    int   shipCol = 0;
    int   shipRow = 3;      // 3,4,5 = naves grises
    float speed   = 60.0f;
};

// Streaming de EnemySpawn: guarda los pendientes ORDENADOS por worldY DESCENDENTE
// (mayor y = mas cercano al inicio abajo). Como el borde superior de la camara
// decrece de forma monotona (la camara sube), cada frame suelta en orden los que ya
// entran por arriba y avanza un puntero. Asume que los enemigos estan colocados por
// ENCIMA del view inicial (y menor que el borde superior de arranque).
class EnemyStreamer : public Component {
public:
    std::vector<PendingEnemy> pending; // ordenados por worldY DESCENDENTE
    float margin = 60.0f;              // adelanto: los suelta un poco antes de asomar
    HudScore* hud = nullptr;           // se lo pasamos a cada enemigo para el puntaje

    void update(float) override {
        float left, right, top, bottom;
        if (!cameraViewBounds(*gameObject->scene, left, right, top, bottom)) return;
        // 'top' es el borde superior (y minima visible) y solo decrece. Soltamos, en
        // orden, los pendientes cuya y ya quedo a la vista (o a 'margin' de asomar).
        while (next < pending.size() && pending[next].worldY >= top - margin) {
            const PendingEnemy& p = pending[next];
            spawnEnemy(*gameObject->scene, p.worldX, p.worldY, p.shipCol, p.shipRow, p.speed, hud);
            ++next;
        }
    }

private:
    size_t next = 0; // proximo pendiente a soltar
};

// --- Jugador -----------------------------------------------------------------
// Mueve la nave con input DENTRO del viewport, la mantiene pegada a la camara y
// dispara balas hacia arriba. Sin gravedad ni fisica: se mueve por Transform.
class ShooterController : public Component {
public:
    float speed = 260.0f; // px/seg del movimiento dentro de la pantalla
    void update(float dt) override {
        const bool* keys = SDL_GetKeyboardState(nullptr);
        Transform* t = gameObject->transform;

        // Input: 4 direcciones, movimiento por Transform directo (sin RigidBody).
        float mx = 0.0f, my = 0.0f;
        if (keys[SDL_SCANCODE_LEFT])  mx -= 1.0f;
        if (keys[SDL_SCANCODE_RIGHT]) mx += 1.0f;
        if (keys[SDL_SCANCODE_UP])    my -= 1.0f;
        if (keys[SDL_SCANCODE_DOWN])  my += 1.0f;
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

        bool shootNow = keys[SDL_SCANCODE_SPACE];
        if (shootNow && !shootPrev) shoot();
        shootPrev = shootNow;
    }
private:
    bool shootPrev = false;
    void shoot() {
        Scene* scene = gameObject->scene;
        GameObject* bala = scene->createGameObject("Bala");
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
        bala->addComponent<DestroyOnHit>()->targetName = "Enemigo";
    }
};

// Crea la nave del jugador (celda a color col 0, fila 0) en la posicion de mundo
// dada. Se reusa desde la fabrica (PlayerStart) y desde el fallback de buildShooter.
static GameObject* makePlayer(Scene& scene, float x, float y) {
    GameObject* player = scene.createGameObject("Player");
    player->transform->x = x;
    player->transform->y = y;
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
    return player;
}

// --- Fabrica: TiledObject -> GameObject (la SEMANTICA vive del lado del juego) ---
// El motor solo entrega datos planos; aqui decidimos, segun "type", que objeto del
// juego crear y con que componentes. PlayerStart y EnemySpawn se manejan directo en
// buildShooter (el jugador se captura para la camara; los enemigos van al streaming),
// asi que aqui solo quedan PowerUp, TriggerZone y los type desconocidos.
//
// Tiled da el centro en pixeles del mapa (sin escalar). Lo llevamos al MUNDO con el
// mismo origen y escala que uso el tilemap, para que objeto y fondo queden alineados.
static void spawnFromTiledObject(Scene& scene, const TiledObject& o,
                                 float originX, float originY, float worldScale) {
    float wx = originX + o.cx * worldScale;
    float wy = originY + o.cy * worldScale;

    if (o.type == "PowerUp") {
        // Power-up en cx,cy. Propiedad kind (string) = tipo de efecto. Por ahora
        // solo el GameObject con un sprite placeholder y un collider trigger.
        std::string kind = o.getString("kind", "");
        GameObject* p = scene.createGameObject("PowerUp");
        p->transform->x = wx;
        p->transform->y = wy;
        p->transform->scaleX = p->transform->scaleY = 2.5f;
        // Placeholder visual: un tile cualquiera del tiles_packed.png (elige otra
        // celda cuando haya arte). Reusa la textura del fondo (misma cadena de ruta).
        auto sr = p->addComponent<SpriteRenderer>(TILES_SHEET);
        sr->setSourceRect(6 * TILE_CELL, 5 * TILE_CELL, TILE_CELL, TILE_CELL);
        auto c = p->addComponent<BoxCollider>();
        c->width = c->height = 32.0f;
        c->isTrigger = true;
        // TODO: aplicar el efecto segun 'kind' cuando la nave lo recoja.
        (void)kind;
    }
    else if (o.type == "TriggerZone") {
        // Rectangulo: zona invisible con un collider trigger del tamano w,h de Tiled
        // (escalado al mundo). Propiedad event (string), p. ej. "boss"/"levelend".
        std::string event = o.getString("event", "");
        GameObject* z = scene.createGameObject("TriggerZone");
        z->transform->x = wx;
        z->transform->y = wy;
        auto c = z->addComponent<BoxCollider>();
        c->width  = o.w * worldScale;
        c->height = o.h * worldScale;
        c->isTrigger = true;
        // TODO: disparar el evento 'event' cuando la nave entre en la zona.
        (void)event;
    }
    else {
        // type desconocido: avisamos (sin tildes) y seguimos, sin crashear.
        SDL_Log("buildShooter: objeto de Tiled con type desconocido '%s' (name='%s'), ignorado",
                o.type.c_str(), o.name.c_str());
    }
}

void buildShooter(Scene& scene) {
    // --- Fondo: nivel cargado desde Tiled (JSON) --------------------------------
    // OJO AL ORDEN: el dibujo sigue el orden de creacion, asi que el tilemap se crea
    // PRIMERO para que las naves (creadas despues) queden ENCIMA del fondo.
    //
    // El nivel se exporta desde Tiled y vive en assets/maps/. Su "image" apunta al
    // tiles_packed.png del pack (tileset 12x10 de tiles de 16x16). El tile, columnas
    // y solidos los define el propio .json: aqui no se tocan. Valores leidos del JSON:
    // tile 16x16, mapa 10x100 (vertical), firstgid 1.
    const int   TILE  = 16;         // tile del tileset (lo confirma el .json)
    const int   MAP_W = 10;         // ancho del mapa de Tiled (para centrarlo en X)
    const float WORLD_SCALE = 3.0f; // tile 16 -> 48 px

    GameObject* world = scene.createGameObject("World");
    // El Transform marca el ORIGEN del mapa (esquina superior izquierda de la celda 0,0).
    world->transform->scaleX = world->transform->scaleY = WORLD_SCALE;
    auto map = world->addComponent<TilemapRenderer>(); // modo archivo: el tileset lo da el mapa

    if (!map->loadFromTiledJson("assets/maps/shmup_level1.json"))
        SDL_Log("buildShooter: no se pudo cargar assets/maps/shmup_level1.json");

    // Centrar el mapa en X alrededor del origen (donde se mueve el player). En Y el
    // mapa es muy alto (100 tiles): lo apoyamos en el borde superior de la vista.
    const float ORIGIN_X = -(MAP_W * TILE * WORLD_SCALE) * 0.5f;
    const float ORIGIN_Y = -300.0f;
    world->transform->x = ORIGIN_X;
    world->transform->y = ORIGIN_Y;

    // --- Objetos de la(s) capa(s) de objetos de Tiled ---------------------------
    // El parser generico (engine) devuelve datos planos; aqui les damos semantica:
    //   PlayerStart -> coloca la nave (se captura para posicionar la camara).
    //   EnemySpawn  -> NO se instancia ya: se acumula para el streaming por scroll.
    //   resto       -> spawnFromTiledObject (PowerUp/TriggerZone placeholder).
    // Reabrimos el .json por ahora (el TilemapRenderer lo abre por su cuenta): no
    // reescribimos su loader; convivir con una segunda lectura esta bien.
    std::vector<TiledObject> objs = loadTiledObjectLayers("assets/maps/shmup_level1.json");
    GameObject* player = nullptr;
    std::vector<PendingEnemy> pendingEnemies;
    int playerCount = 0;
    for (const TiledObject& o : objs) {
        float wx = ORIGIN_X + o.cx * WORLD_SCALE;
        float wy = ORIGIN_Y + o.cy * WORLD_SCALE;
        if (o.type == "PlayerStart") {
            ++playerCount;
            player = makePlayer(scene, wx, wy);
        }
        else if (o.type == "EnemySpawn") {
            // Nave enemiga gris (filas 3-5 de la hoja). Propiedades de Tiled:
            //   shipCol (int), shipRow (int) -> celda 32x32 de la hoja de naves.
            //   speed   (float)             -> velocidad extra de caida.
            // Si faltan, se usan valores por defecto sensatos. Se suelta luego, por
            // scroll, cuando su banda entra por arriba (ver EnemyStreamer).
            PendingEnemy p;
            p.worldX  = wx;
            p.worldY  = wy;
            p.shipCol = (int)o.getNumber("shipCol", 0.0);
            p.shipRow = (int)o.getNumber("shipRow", 3.0); // 3,4,5 = naves grises
            p.speed   = (float)o.getNumber("speed", 60.0);
            pendingEnemies.push_back(p);
        }
        else {
            spawnFromTiledObject(scene, o, ORIGIN_X, ORIGIN_Y, WORLD_SCALE);
        }
    }
    // El mapa debe traer EXACTAMENTE un PlayerStart. Si aun no lo tiene (la capa de
    // objetos se anade despues en Tiled), colocamos una nave por defecto para que el
    // ejemplo siga siendo jugable; si hay mas de uno, avisamos.
    if (playerCount == 0) {
        SDL_Log("buildShooter: no hay PlayerStart en el mapa; uso una nave por defecto");
        player = makePlayer(scene, 0.0f, 250.0f);
    } else if (playerCount > 1) {
        SDL_Log("buildShooter: hay %d PlayerStart en el mapa (deberia haber exactamente uno)",
                playerCount);
    }

    // --- Streaming de EnemySpawn ------------------------------------------------
    // Ordenados por y DESCENDENTE (mayor y = mas cercano al inicio abajo): el
    // EnemyStreamer los suelta en orden a medida que la camara sube y su borde
    // superior baja hasta cada uno.
    std::sort(pendingEnemies.begin(), pendingEnemies.end(),
              [](const PendingEnemy& a, const PendingEnemy& b) { return a.worldY > b.worldY; });
    GameObject* streamer = scene.createGameObject("EnemyStreamer");
    auto streamComp = streamer->addComponent<EnemyStreamer>();
    streamComp->pending = std::move(pendingEnemies);

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

    // Margen (px de pantalla) entre el player y el borde INFERIOR del viewport: a
    // menor margen, mas pegado abajo queda (mas espacio arriba para ver enemigos
    // entrando, propio de un shmup vertical). Ajustable.
    const float PLAYER_BOTTOM_MARGIN = 80.0f;

    // Trap del zoom: el viewport EFECTIVO se divide por el zoom de la camara. Con
    // zoom != 1, usar winH crudo dejaria el encuadre corrido.
    float zoom = camera->getZoom();
    float halfViewportEff = (winH / zoom) * 0.5f;

    // Eje x: CENTRO del mapa en el mundo = origen del mapa (Transform del World) mas
    // medio ancho, leyendo el ancho del propio mapa cargado (getWorldWidth), sin
    // cablear el ancho aqui. Centra el mapa horizontalmente.
    // Eje y: derivada del player, dejandolo en la zona baja del viewport (con espacio
    // arriba). Alternativa simple para centrarlo vertical: cam.y = player->transform->y.
    // Redondeo a entero (pixel art: evitar bleeding sub-pixel desde el primer frame).
    cam->transform->x = std::round(world->transform->x + map->getWorldWidth() * 0.5f);
    cam->transform->y = std::round(player->transform->y - halfViewportEff + PLAYER_BOTTOM_MARGIN);

    // --- HUD: puntaje -----------------------------------------------------------
    // Se crea de ULTIMO para que el texto quede por ENCIMA de todo (el dibujo sigue
    // el orden de creacion). El TextRenderer es screenSpace: coordenadas de pantalla
    // fijas, no scrollea con el fondo. El Transform marca el CENTRO del texto, asi que
    // ubicamos ese centro cerca de la esquina superior izquierda.
    GameObject* hudObj = scene.createGameObject("HUD");
    hudObj->transform->x = 130.0f; // centro del texto: deja margen a la izquierda
    hudObj->transform->y = 36.0f;
    auto label = hudObj->addComponent<TextRenderer>();
    label->screenSpace = true;
    label->setFont(scene.getAssets().loadFont(HUD_FONT, HUD_SIZE));
    label->setColor(TextColor{ 255, 255, 255, 255 }); // blanco
    auto hud = hudObj->addComponent<HudScore>();
    hud->label = label;
    // El streamer ya existe: le pasamos el HUD ahora (se lee recien en el primer
    // frame, cuando suelta enemigos, asi que el orden de creacion no importa).
    streamComp->hud = hud;
}
