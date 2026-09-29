#pragma once
#include <functional>
#include <map>
#include <string>

#include "LevelData.h"

class Scene;
class GameObject;
class TilemapRenderer;
struct SDL_Window;   // declaraciones adelantadas: SDL e ImGui solo aparecen en el .cpp
struct SDL_Renderer;
union  SDL_Event;

// Editor de niveles MINIMO, dentro del juego (F2). Edita lo que va ENCIMA del mapa: los
// objetos del .level.json. El mapa de Tiled se muestra, pero NUNCA se escribe (cada
// archivo tiene un solo escritor; ver LevelData.h).
//
// No es un componente: es un objeto que el bucle de main "bombea", igual que Input. El
// alumno sigue siendo dueno de su bucle y de su escena:
//
//     LevelEditor editor;
//     editor.initGui(window, renderer);                     // paneles (Dear ImGui)
//     editor.open("assets/maps/nivel.level.json", buildMiNivel);
//     editor.buildInto(*scene);
//     // ...dentro de while (SDL_PollEvent(&e)):
//     editor.processEvent(e);
//     // ...cada frame, despues de Input::update():
//     editor.update(*scene, dt);                           // F2, raton, Ctrl+S...
//     if (editor.wantsRebuild()) {                          // Stop/Play o recarga
//         scene = std::make_unique<Scene>(renderer);
//         editor.buildInto(*scene);
//     }
//     if (!editor.isEditing()) scene->update(dt);           // editando: escena congelada
//     // ...scene->render(); y al final:
//     editor.render(*scene);
//     // ...al salir, ANTES de destruir el renderer:
//     editor.shutdownGui();
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
//   Ctrl+D               duplicar el seleccionado      Supr borrar el seleccionado
//
// PANELES (Dear ImGui, solo en edicion): "Objetos" (lista, Nuevo/Duplicar/Borrar),
// "Inspector" (type, nombre, posicion, tamanio y propiedades del seleccionado) y
// "Camara" (ajustes efectivos y previsualizacion de lo que vera el jugador al empezar).
// Mover no reconstruye la escena; cambiar el type, el tamanio o una propiedad SI (es la
// fabrica del juego la que decide que significan), y se hace al terminar de editar el
// campo. Mientras el raton esta sobre un panel, o se escribe en un campo, los atajos y
// los clics del editor sobre el mundo se ignoran.
class LevelEditor {
public:
    // La fabrica del juego: construye en 'scene' el nivel descrito por 'level'.
    using BuildFn = std::function<void(Scene& scene, const LevelData& level)>;

    // Prepara los paneles (crea el contexto de Dear ImGui) y recuerda la ventana, que se
    // maximiza mientras se edita. Una sola vez, despues de crear la ventana y el renderer.
    // Sin esto el editor funciona igual, sin paneles y sin cambiar la ventana.
    bool initGui(SDL_Window* window, SDL_Renderer* renderer);
    // Libera ImGui. Llamarla ANTES de destruir el renderer (sus texturas son de el).
    void shutdownGui();
    // Pasa un evento de SDL a los paneles (teclas, texto, raton). Dentro del bucle de
    // SDL_PollEvent de main, igual que Input::processEvent.
    void processEvent(const SDL_Event& e);

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
    // Pone un objeto en (mx,my) del mapa y mueve sus GameObjects (sin reconstruir).
    void setObjectPosition(Scene& scene, const TilemapRenderer* map, TiledObject& o,
                           float mx, float my);
    // Paso de la grilla en pixeles del mapa (medio tile; 1 si la grilla esta apagada).
    void snapStep(const TilemapRenderer* map, float& stepX, float& stepY) const;
    void applyView(Scene& scene) const;  // la vista del editor -> camara activa
    void save();
    void reload();
    void showMessage(const std::string& text);

    // --- Edicion del modelo ------------------------------------------------------
    void createObject(const TilemapRenderer* map, const std::string& type);
    void duplicateSelected(const TilemapRenderer* map);
    void deleteSelected();
    // Hubo un cambio en el modelo. Si la fabrica tiene que volver a construir (type,
    // tamanio, propiedades, objetos nuevos o borrados), se pide la reconstruccion para
    // cuando el usuario termine de editar el campo activo.
    void markChanged(bool needsRebuild);

    // --- Paneles -------------------------------------------------------------------
    void panelObjects(Scene& scene, const TilemapRenderer* map);
    void panelInspector(Scene& scene, const TilemapRenderer* map);
    void panelCamera();
    void drawCameraPreview(Scene& scene);
    // Guarda los ajustes de camara EFECTIVOS de una escena recien construida (los
    // valores por defecto del juego mas lo que traiga el nivel), antes de que la vista
    // del editor pise la camara.
    void captureCameraSettings(Scene& scene);

    // --- Ventana ---------------------------------------------------------------------
    // Al editar, la ventana se maximiza (mas espacio para paneles y mundo); al volver a
    // jugar recupera EXACTAMENTE el tamanio del juego, para que el juego se vea igual.
    void growWindow();
    void restoreWindow();
    // Escala de los textos del editor dibujados con SDL_RenderDebugText: entera, para
    // que la fuente de 8x8 no se deforme (pantalla al 150% -> x2).
    float textScale() const;
    float barHeight() const;

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

    bool pendingRebuild = false; // reconstruir cuando no haya un campo en edicion

    // Ventana (la da initGui). Tamanio del JUEGO guardado al entrar a editar.
    SDL_Window* window = nullptr;
    float uiScale = 1.0f;        // escala de la pantalla (Windows al 150% -> 1.5)
    int  gameW = 0, gameH = 0;           // tamanio de la ventana del juego (logico)
    int  gamePixelW = 0, gamePixelH = 0; // lo mismo en pixeles (para la previsualizacion)
    bool gameWasResizable = false, gameWasMaximized = false;
    bool windowGrown = false;

    // Paneles (Dear ImGui).
    bool guiReady = false;       // initGui ya corrio
    bool guiFrame = false;       // hay un frame de ImGui abierto (lo cierra render)
    bool mouseOverGui = false;   // este frame el raton es de los paneles, no del mundo
    bool keysForGui = false;     // se esta escribiendo en un campo: sin atajos
    bool showCameraPreview = true;
    std::map<std::string, float> camSettings; // ajustes de camara efectivos de la escena
    std::string newType;         // "Nuevo": un type escrito a mano
    std::string newPropKey;      // "Agregar propiedad": su nombre
    bool newPropIsNumber = false;
};
