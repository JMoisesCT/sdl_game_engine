#include "Shooter.h"

#include <SDL3/SDL.h>
#include <cstdlib>

#include "../engine/Scene.h"
#include "../engine/GameObject.h"
#include "../engine/Component.h"
#include "../engine/Transform.h"
#include "../engine/SpriteRenderer.h"
#include "../engine/TilemapRenderer.h"
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

void buildShooter(Scene& scene) {
    // --- Fondo: nivel cargado desde Tiled (JSON) --------------------------------
    // OJO AL ORDEN: el dibujo sigue el orden de creacion, asi que el tilemap se crea
    // PRIMERO para que las naves (creadas despues) queden ENCIMA del fondo.
    //
    // El nivel se exporta desde Tiled y vive en assets/maps/. Su "image" apunta al
    // tiles_packed.png del pack (tileset 12x10 de tiles de 16x16). El tile, columnas
    // y solidos los define el propio .json: aqui no se tocan. Valores leidos del JSON:
    // tile 16x16, mapa 10x100 (vertical), firstgid 1.
    const int TILE  = 16;          // tile del tileset (lo confirma el .json)
    const int MAP_W = 10;          // ancho del mapa de Tiled (para centrarlo en X)

    GameObject* world = scene.createGameObject("World");
    // El Transform marca el ORIGEN del mapa (esquina superior izquierda de la celda 0,0).
    world->transform->scaleX = world->transform->scaleY = 3.0f; // tile 16 -> 48 px
    auto map = world->addComponent<TilemapRenderer>(); // modo archivo: el tileset lo da el mapa

    if (!map->loadFromTiledJson("assets/maps/shmup_level1.json"))
        SDL_Log("buildShooter: no se pudo cargar assets/maps/shmup_level1.json");

    // Centrar el mapa en X alrededor del origen (donde se mueve el player). En Y el
    // mapa es muy alto (100 tiles): lo apoyamos en el borde superior de la vista.
    world->transform->x = -(MAP_W * TILE * 3.0f) * 0.5f;
    world->transform->y = -300.0f;

    // --- Jugador: nave a color, celda (col 0, fila 0) ---------------------------
    GameObject* player = scene.createGameObject("Player");
    player->transform->y = 250.0f;
    player->transform->scaleX = player->transform->scaleY = 3.0f;
    auto sr = player->addComponent<SpriteRenderer>(SHIPS_SHEET);
    setShipCell(sr, 0, 0); // nave del jugador: columna 0, fila 0 (a color)
    auto rb = player->addComponent<RigidBody2D>();
    rb->gravityScale = 0.0f;
    auto col = player->addComponent<BoxCollider>();
    col->width = 60.0f; col->height = 60.0f;
    player->addComponent<ShooterController>();

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
