#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "LevelData.h"
#include "ObjectCatalog.h"

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
//     editor.open("assets/maps/nivel.level.json", buildMiNivel, miCatalogo);
//     editor.buildInto(*scene);
//     // ...dentro de while (SDL_PollEvent(&e)):
//     if (e.type == SDL_EVENT_QUIT && editor.confirmDiscard("salir")) running = false;
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
//   F2                   jugar (desde el PlayerStart)  Shift+F2  jugar desde el cursor
//   clic izq.           seleccionar / arrastrar       G    grilla (medio tile) on/off
//   clic der. o flechas  mover la vista                C    ver celdas solidas on/off
//   rueda                zoom (hacia el cursor)        F5   recargar (mapa y nivel)
//   Ctrl+S               guardar el .level.json        Esc  quitar la seleccion
//   Ctrl+D               duplicar el seleccionado      Supr borrar el seleccionado
//   Ctrl+Z               deshacer                      Ctrl+Y / Ctrl+Shift+Z  rehacer
//
// DESHACER: como el modelo son DATOS, cada paso guarda una copia entera del LevelData
// (unos pocos KB). Un paso es un GESTO completo: todo un arrastre, todo lo escrito en un
// campo hasta salir de el. Deshacer reconstruye la escena desde la copia.
//
// AVISOS: un objeto cuyo centro cae fuera del mapa o dentro de un tile solido se marca
// en naranja (en el mundo, en la lista y en el Inspector). No se corrige solo: a veces es
// a proposito (una zona que empieza fuera de la pantalla).
//
// RECARGA AUTOMATICA: editando, si el mapa de Tiled cambia en disco (se guardo en Tiled),
// la escena se reconstruye sola con el mapa nuevo. Los objetos no se tocan.
//
// CATALOGO (opcional, ver ObjectCatalog.h): la lista de type que entiende la fabrica del
// juego. Con el, "Nuevo" ofrece todos los type del juego con sus propiedades por defecto,
// el Inspector muestra combos para los valores fijos (y la ayuda de cada type), y los
// avisos incluyen type desconocidos, valores no validos y objetos unicos repetidos.
//
// JUGAR DESDE EL CURSOR (Shift+F2): juega una COPIA del nivel con el punto de aparicion
// (el type marcado playerSpawn en el catalogo) movido al raton. El modelo no cambia: al
// volver al editor el PlayerStart sigue donde estaba. Sirve para probar el final de un
// nivel largo sin recorrerlo entero.
//
// CAMBIOS SIN GUARDAR: antes de cerrar la ventana o cambiar de ejemplo, main llama a
// confirmDiscard(), que pregunta Guardar / Descartar / Cancelar.
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
    // fabrica y su catalogo de type (opcional). Si el archivo no se puede leer devuelve
    // false, el editor queda sin nivel (F2 no hace nada) y buildInto construye igual, con
    // un nivel vacio.
    bool open(const std::string& levelPath, BuildFn build, ObjectCatalog catalog = {});

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

    // Antes de algo que tiraria los cambios sin guardar (cerrar la ventana, cambiar de
    // ejemplo): si los hay, pregunta con un cuadro de dialogo Guardar / Descartar /
    // Cancelar. Devuelve true si se puede seguir (no habia cambios, se guardaron o se
    // descartaron) y false si hay que cancelar (o si guardar fallo). 'action' completa
    // la pregunta: "salir", "cambiar de ejemplo"...
    bool confirmDiscard(const char* action);

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
    // Lo que TODO cambio del modelo hace antes de tocarlo: si empieza un gesto, recuerda
    // la seleccion de ese momento. Lo llaman markChanged y setObjectPosition.
    void beginChange();

    // --- Deshacer / rehacer ---------------------------------------------------------
    // Un paso de la historia: el modelo entero, el objeto seleccionado y la version (para
    // saber si coincide con lo guardado en disco).
    struct Snapshot {
        LevelData level;
        int selectedId = 0;
        int version = 0;
    };
    // Si hay un cambio sin anotar y el gesto termino (sin arrastre ni campo activo), la
    // copia anterior pasa a la pila de deshacer. Se llama al final de cada update.
    void commitIfIdle();
    void commitNow();          // anota ya, aunque el gesto no haya terminado (al guardar)
    void undo();
    void redo();
    void resetHistory();       // al abrir o recargar desde disco: historia vacia
    void refreshDirty() { dirty = uncommitted || version != savedVersion; }

    // --- Catalogo -------------------------------------------------------------------
    const ObjectTypeSpec* specOf(const TiledObject& o) const { return findObjectType(catalog, o.type); }
    const ObjectTypeSpec* spawnSpec() const; // el type playerSpawn (nullptr si no hay)
    int  countType(const std::string& type) const;
    // Agrega las propiedades que el catalogo declara para su type y el objeto no tiene
    // (con su valor por defecto). No quita las que sobran.
    void addMissingProperties(TiledObject& o) const;

    // --- Jugar desde el cursor (Shift+F2) ------------------------------------------------
    // Guarda la posicion del raton (px del mapa) como punto de aparicion de la proxima
    // partida. false si no se puede (sin type playerSpawn, raton en una pared...).
    bool preparePlayFromCursor(Scene& scene);
    // Copia del modelo con el punto de aparicion movido: lo que se juega con Shift+F2.
    LevelData levelForPlay() const;

    // --- Avisos y recarga del mapa ----------------------------------------------------
    // "" si el objeto esta bien; si no, el problema en pocas palabras.
    std::string objectWarning(const TilemapRenderer* map, const TiledObject& o) const;
    // Lo mismo para un punto: "fuera del mapa", "dentro de un tile solido" o "".
    std::string positionWarning(const TilemapRenderer* map, float mx, float my) const;
    // Fecha de modificacion del mapa de Tiled en disco (0 si no se puede leer).
    int64_t mapFileTime() const;
    void watchMapFile(float dt);

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
    ObjectCatalog catalog;  // los type del juego (puede ir vacio)
    LevelData   level;      // EL MODELO: lo que se edita y se guarda
    bool levelOpen = false;
    bool editing   = false;
    bool dirty     = false;
    bool rebuildRequested = false;

    // Shift+F2: la partida en curso empezo desde el cursor, no desde el punto de aparicion.
    bool  playFromCursor = false;
    float playFromX = 0.0f, playFromY = 0.0f; // px del mapa

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

    // Historia. 'committed' es el modelo tal como quedo tras el ultimo paso anotado; el
    // modelo vivo (level) puede ir por delante mientras dura un gesto (uncommitted).
    std::vector<Snapshot> undoStack, redoStack;
    LevelData committed;
    bool uncommitted = false;
    int  gestureSelectedId = 0; // seleccion al EMPEZAR el gesto (deshacer la vuelve a elegir)
    int  version = 0;        // version de 'committed' (cada paso anotado recibe una nueva)
    int  lastVersion = 0;    // la ultima repartida (las versiones no se repiten)
    int  savedVersion = 0;   // la version que esta en disco (-1: ninguna de la historia)

    // Recarga automatica del mapa: fecha del archivo al construir la escena, y un
    // cambio visto que espera a que el archivo deje de cambiar antes de releerlo.
    int64_t mapTime = 0;
    int64_t mapTimeSeen = 0;
    float   mapPollTimer = 0.0f;

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
