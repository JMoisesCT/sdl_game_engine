#include "Shooter.h"

#include <SDL3/SDL.h>
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
#include "../engine/Spawner.h"

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

// Recorte (x,y,w,h) de la celda (col,fil) de la hoja de naves.
static void setShipCell(SpriteRenderer* sr, int col, int row) {
    sr->setSourceRect(col * SHIP_CELL, row * SHIP_CELL, SHIP_CELL, SHIP_CELL);
}

// Destruye su objeto (y al otro) cuando choca con algo de cierto nombre.
class DestroyOnHit : public Component {
public:
    std::string targetName;
    void onCollision(GameObject* other) override {
        if (other->name == targetName) {
            gameObject->scene->destroy(gameObject);
            gameObject->scene->destroy(other);
        }
    }
};

// Mueve la nave horizontalmente y dispara balas con espacio.
class ShooterController : public Component {
public:
    float speed = 260.0f;
    void update(float) override {
        const bool* keys = SDL_GetKeyboardState(nullptr);
        auto rb = gameObject->getComponent<RigidBody2D>();

        float mx = 0.0f;
        if (keys[SDL_SCANCODE_LEFT])  mx -= 1.0f;
        if (keys[SDL_SCANCODE_RIGHT]) mx += 1.0f;
        if (rb) rb->velocityX = mx * speed;

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
        rb->velocityY = -500.0f; // sube
        auto c = bala->addComponent<BoxCollider>();
        c->width = c->height = 24.0f;
        c->isTrigger = true;
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
    auto rb = player->addComponent<RigidBody2D>();
    rb->gravityScale = 0.0f;
    auto col = player->addComponent<BoxCollider>();
    col->width = 60.0f; col->height = 60.0f;
    player->addComponent<ShooterController>();
    return player;
}

// --- Fabrica: TiledObject -> GameObject (la SEMANTICA vive del lado del juego) ---
// El motor solo entrega datos planos; aqui decidimos, segun "type", que objeto del
// juego crear y con que componentes. Un type desconocido se avisa y se ignora (no
// crashea). El motor NO conoce ninguno de estos types.
//
// Tiled da el centro en pixeles del mapa (sin escalar). Lo llevamos al MUNDO con el
// mismo origen y escala que uso el tilemap, para que objeto y fondo queden alineados.
static void spawnFromTiledObject(Scene& scene, const TiledObject& o,
                                 float originX, float originY, float worldScale) {
    float wx = originX + o.cx * worldScale;
    float wy = originY + o.cy * worldScale;

    if (o.type == "PlayerStart") {
        // Objeto punto: coloca la nave del jugador. Debe haber exactamente uno
        // (buildShooter valida el conteo y avisa si no).
        makePlayer(scene, wx, wy);
    }
    else if (o.type == "EnemySpawn") {
        // Nave enemiga gris (filas 3-5 de la hoja). Propiedades de Tiled:
        //   shipCol (int), shipRow (int) -> celda 32x32 de la hoja de naves.
        //   speed   (float)             -> velocidad de caida (Y crece hacia abajo).
        // Si faltan, se usan valores por defecto sensatos.
        int   shipCol = (int)o.getNumber("shipCol", 0.0);
        int   shipRow = (int)o.getNumber("shipRow", 3.0); // 3,4,5 = naves grises
        float speed   = (float)o.getNumber("speed", 60.0);

        GameObject* e = scene.createGameObject("Enemigo");
        e->transform->x = wx;
        e->transform->y = wy;
        e->transform->scaleX = e->transform->scaleY = 2.5f;
        auto sr = e->addComponent<SpriteRenderer>(SHIPS_SHEET);
        setShipCell(sr, shipCol, shipRow);
        auto rb = e->addComponent<RigidBody2D>();
        rb->gravityScale = 0.0f;
        rb->velocityY = speed; // baja por la pantalla
        auto c = e->addComponent<BoxCollider>();
        c->width = c->height = 80.0f;
        c->isTrigger = true;
        e->addComponent<DestroyOnHit>()->targetName = "Bala";
        // TODO(streaming): con scroll, activar el enemigo solo cuando su celda entra
        // en la vista y destruirlo (o reciclarlo) al salir por abajo.
    }
    else if (o.type == "PowerUp") {
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
    // VERSION 1: se instancia TODO al cargar la escena (sin streaming por scroll).
    // El parser generico (engine) devuelve datos planos; la fabrica de arriba les da
    // semantica. Reabrimos el .json por ahora (el TilemapRenderer lo abre por su
    // cuenta): no reescribimos su loader; convivir con una segunda lectura esta bien.
    // TODO(streaming): cuando haya scroll, instanciar cada objeto solo al entrar su
    // banda vertical en la vista, en vez de todos de golpe aqui.
    std::vector<TiledObject> objs = loadTiledObjectLayers("assets/maps/shmup_level1.json");
    int playerCount = 0;
    for (const TiledObject& o : objs) {
        if (o.type == "PlayerStart") ++playerCount;
        spawnFromTiledObject(scene, o, ORIGIN_X, ORIGIN_Y, WORLD_SCALE);
    }
    // El mapa debe traer EXACTAMENTE un PlayerStart. Si aun no lo tiene (la capa de
    // objetos se anade despues en Tiled), colocamos una nave por defecto para que el
    // ejemplo siga siendo jugable; si hay mas de uno, avisamos.
    if (playerCount == 0) {
        SDL_Log("buildShooter: no hay PlayerStart en el mapa; uso una nave por defecto");
        makePlayer(scene, 0.0f, 250.0f);
    } else if (playerCount > 1) {
        SDL_Log("buildShooter: hay %d PlayerStart en el mapa (deberia haber exactamente uno)",
                playerCount);
    }

    GameObject* spawner = scene.createGameObject("EnemySpawner");
    auto sp = spawner->addComponent<Spawner>();
    sp->interval = 1.0f;
    sp->spawn = [](Scene& s) {
        GameObject* e = s.createGameObject("Enemigo");
        e->transform->x = (float)((std::rand() % 800) - 400);
        e->transform->y = -300.0f;
        e->transform->scaleX = e->transform->scaleY = 2.5f;
        auto sr = e->addComponent<SpriteRenderer>(SHIPS_SHEET);
        // Enemigos: naves grises (filas 3,4,5). Variamos entre 3 celdas distintas.
        static const int ENEMY_COLS[3] = { 0, 1, 2 };
        static const int ENEMY_ROWS[3] = { 3, 4, 5 };
        int pick = std::rand() % 3;
        setShipCell(sr, ENEMY_COLS[pick], ENEMY_ROWS[pick]);
        auto rb = e->addComponent<RigidBody2D>();
        rb->gravityScale = 0.4f; // caen lento
        auto c = e->addComponent<BoxCollider>();
        c->width = c->height = 80.0f;
        c->isTrigger = true;
        e->addComponent<Lifetime>()->seconds = 6.0f;
        e->addComponent<DestroyOnHit>()->targetName = "Bala";
    };

    // Camara fija: la vista no se mueve, estilo shoot'em up.
    GameObject* cam = scene.createGameObject("MainCamera");
    cam->addComponent<Camera>();
}
