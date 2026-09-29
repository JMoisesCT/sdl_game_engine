#pragma once
#include <functional>
#include <string>

#include "LevelData.h"

class Scene;
class GameObject;
class TilemapRenderer;

// Editor de niveles MINIMO, dentro del juego (F2). Edita lo que va ENCIMA del mapa: los
// objetos del .level.json. El mapa de Tiled se muestra, pero NUNCA se escribe (cada
// archivo tiene un solo escritor; ver LevelData.h).
//
// No es un componente: es un objeto que el bucle de main "bombea", igual que Input. El
// alumno sigue siendo dueno de su bucle y de su escena:
//
//     LevelEditor editor;
//     editor.open("assets/maps/nivel.level.json", buildMiNivel);
//     editor.buildInto(*scene);
//     // ...cada frame, despues de Input::update():
//     editor.update(*scene, dt);                           // F2, raton, Ctrl+S...
//     if (editor.wantsRebuild()) {                          // Stop/Play o recarga
//         scene = std::make_unique<Scene>(renderer);
//         editor.buildInto(*scene);
//     }
//     if (!editor.isEditing()) scene->update(dt);           // editando: escena congelada
//     // ...scene->render(); y al final:
//     editor.render(*scene);
//
// MODELO = DATOS, VISTA = ESCENA: el editor modifica su copia del LevelData y la escena
// se reconstruye desde ella con la fabrica del JUEGO (la funcion que se le pasa a open).
// Asi el motor nunca necesita saber como se construye un "Fruit" o un "Enemy". Para
// saber que objeto de la escena corresponde a que entrada del archivo, la fabrica pone
// GameObject::levelObjectId.
//
// F2 funciona como Stop/Play de Unity: al entrar a editar, la escena se reconstruye en su
// estado INICIAL (vuelve lo recogido, el jugador a su inicio); al salir, se reconstruye
// con lo editado. Mientras se edita no corre update: no hay fisica ni triggers, y cada
// sprite muestra el primer cuadro de su animacion.
//
// Controles en edicion:
//   clic izq.            seleccionar / arrastrar       G    grilla (medio tile) on/off
//   clic der. o flechas  mover la vista                C    ver celdas solidas on/off
//   rueda                zoom (hacia el cursor)        F5   recargar (mapa y nivel)
//   Ctrl+S               guardar el .level.json        Esc  quitar la seleccion
class LevelEditor {
public:
    // La fabrica del juego: construye en 'scene' el nivel descrito por 'level'.
    using BuildFn = std::function<void(Scene& scene, const LevelData& level)>;

    // Abre un archivo de nivel: lo lee (sera el modelo que se edita) y recuerda la
    // fabrica. Si el archivo no se puede leer devuelve false, el editor queda sin nivel
    // (F2 no hace nada) y buildInto construye igual, con un nivel vacio.
    bool open(const std::string& levelPath, BuildFn build);

    // Olvida el nivel (p. ej. al cambiar a un ejemplo que no tiene archivo de nivel).
    // Si habia cambios sin guardar, lo avisa en el log: se pierden.
    void close();

    // Construye la escena desde el modelo con la fabrica. La escena debe estar recien
    // creada (vacia). Tambien atiende el pedido de wantsRebuild.
    void buildInto(Scene& scene);

    // true si el editor necesita una escena nueva (F2, F5). main la crea y llama a
    // buildInto; el editor no la crea el mismo porque la escena es de main.
    bool wantsRebuild() const { return rebuildRequested; }

    bool hasLevel() const  { return levelOpen; }
    bool isEditing() const { return editing; }
    bool isDirty() const   { return dirty; }   // hay cambios sin guardar
    const LevelData& getLevel() const { return level; }

    // Una vez por frame, despues de Input::update() y ANTES de scene->update().
    void update(Scene& scene, float dt);
    // Una vez por frame, DESPUES de scene->render(): dibuja marcas, grilla y la barra.
    void render(Scene& scene);

private:
    TiledObject* findObject(int id);
    const TiledObject* findObject(int id) const;
    // Pixeles del mapa <-> mundo. Sin tilemap en la escena, son el mismo espacio.
    void toWorld(const TilemapRenderer* map, const TiledObject& o, float& wx, float& wy) const;
    void toMap(const TilemapRenderer* map, float wx, float wy, float& mx, float& my) const;
    // Rectangulo de seleccion de un objeto, en el mundo: el BoxCollider de su
    // GameObject si tiene, o un cuadrado fijo en pantalla alrededor de su punto.
    void pickRect(Scene& scene, const TilemapRenderer* map, const TiledObject& o,
                  float& left, float& top, float& right, float& bottom) const;
    int  pickAt(Scene& scene, const TilemapRenderer* map, float wx, float wy) const;
    void moveSelected(Scene& scene, const TilemapRenderer* map, float wx, float wy);
    void applyView(Scene& scene) const;  // la vista del editor -> camara activa
    void save();
    void reload();
    void showMessage(const std::string& text);

    std::string path;       // .level.json abierto
    BuildFn     build;
    LevelData   level;      // EL MODELO: lo que se edita y se guarda
    bool levelOpen = false;
    bool editing   = false;
    bool dirty     = false;
    bool rebuildRequested = false;

    // Vista del editor (sobrevive a las reconstrucciones de la escena).
    float viewX = 0.0f, viewY = 0.0f, viewZoom = 1.0f;

    int   selectedId = 0;   // id del objeto seleccionado (0 = ninguno)
    bool  dragging = false;
    bool  dragMoved = false; // el cursor ya se alejo lo bastante del clic como para mover
    float pressX = 0.0f, pressY = 0.0f;     // donde se hizo el clic (pantalla)
    float dragOffX = 0.0f, dragOffY = 0.0f; // objeto - cursor, en el mundo
    bool  panning = false;
    float panLastX = 0.0f, panLastY = 0.0f; // ultima posicion del cursor (pantalla)

    bool snap = true;        // ajustar a medio tile al arrastrar
    bool showSolids = true;  // dibujar las celdas solidas del mapa

    std::string message;     // aviso temporal en la barra ("Guardado", ...)
    float messageTime = 0.0f;
};
