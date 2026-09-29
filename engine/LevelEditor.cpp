#include "LevelEditor.h"
#include "Scene.h"
#include "GameObject.h"
#include "Transform.h"
#include "Camera.h"
#include "FollowCamera.h"
#include "BoxCollider.h"
#include "TilemapRenderer.h"
#include "Input.h"

#include <SDL3/SDL.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_sdl3.h>
#include <imgui/imgui_impl_sdlrenderer3.h>
#include <imgui/imgui_stdlib.h> // InputText con std::string
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

namespace {
    const float PICK_HALF_SCREEN = 12.0f; // medio lado (px de pantalla) del area de clic sin collider
    const float DRAG_THRESHOLD   = 3.0f;  // px de pantalla antes de empezar a mover (un clic no mueve)
    const float PAN_SPEED        = 900.0f;// px de pantalla por segundo con las flechas
    const float ZOOM_STEP        = 1.15f; // factor por muesca de la rueda
    const float ZOOM_MIN = 0.25f, ZOOM_MAX = 4.0f;
    const float MESSAGE_SECONDS  = 3.0f;
    const float CHAR             = (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE; // 8 px
    const float SIDE_PANEL_W     = 360.0f; // ancho de los paneles de la derecha
    const size_t MAX_UNDO        = 200;    // pasos de deshacer que se guardan
    const float MAP_POLL_SECONDS = 0.5f;   // cada cuanto se mira si Tiled guardo el mapa

    TilemapRenderer* findMap(Scene& scene) {
        for (const auto& obj : scene.getObjects())
            if (TilemapRenderer* tm = obj->getComponent<TilemapRenderer>()) return tm;
        return nullptr;
    }

    FollowCamera* findFollow(Scene& scene) {
        for (const auto& obj : scene.getObjects())
            if (FollowCamera* f = obj->getComponent<FollowCamera>()) return f;
        return nullptr;
    }

    GameObject* findGameObject(Scene& scene, int levelId) {
        if (levelId <= 0) return nullptr;
        for (const auto& obj : scene.getObjects())
            if (obj->levelObjectId == levelId) return obj.get();
        return nullptr;
    }

    bool ctrlDown()  { return Input::isDown(Key::LCtrl)  || Input::isDown(Key::RCtrl); }
    bool shiftDown() { return Input::isDown(Key::LShift) || Input::isDown(Key::RShift); }

    float zoomOf(Scene& scene) {
        Camera* cam = scene.getActiveCamera();
        return cam ? cam->getZoom() : 1.0f;
    }

    void worldToScreen(Scene& scene, float wx, float wy, float& sx, float& sy) {
        if (Camera* cam = scene.getActiveCamera()) cam->worldToScreen(wx, wy, sx, sy);
        else { sx = wx; sy = wy; }
    }

    void screenToWorld(Scene& scene, float sx, float sy, float& wx, float& wy) {
        if (Camera* cam = scene.getActiveCamera()) cam->screenToWorld(sx, sy, wx, wy);
        else { wx = sx; wy = sy; }
    }

    // Un numero para mostrar: sin decimales si es entero.
    std::string num(float v) {
        char buf[32];
        if (std::floor(v) == v) std::snprintf(buf, sizeof(buf), "%d", (int)v);
        else                    std::snprintf(buf, sizeof(buf), "%.1f", v);
        return buf;
    }

    // Texto con la fuente de depuracion de SDL3 (8x8 px, solo ASCII), escalado.
    void text(SDL_Renderer* r, float x, float y, const std::string& s, float scale,
              Uint8 cr, Uint8 cg, Uint8 cb) {
        float oldX = 1.0f, oldY = 1.0f;
        SDL_GetRenderScale(r, &oldX, &oldY);
        SDL_SetRenderScale(r, scale, scale);
        SDL_SetRenderDrawColor(r, cr, cg, cb, 255); // SDL_RenderDebugText usa este color
        SDL_RenderDebugText(r, x / scale, y / scale, s.c_str());
        SDL_SetRenderScale(r, oldX, oldY);
    }

    // Texto con un fondo oscuro detras, para que se lea sobre cualquier cosa.
    void label(SDL_Renderer* r, float x, float y, const std::string& s, float scale,
               Uint8 cr, Uint8 cg, Uint8 cb) {
        SDL_FRect bg{ x - 2.0f, y - 2.0f, s.size() * CHAR * scale + 4.0f, CHAR * scale + 4.0f };
        SDL_SetRenderDrawColor(r, 0, 0, 0, 170);
        SDL_RenderFillRect(r, &bg);
        text(r, x, y, s, scale, cr, cg, cb);
    }

    // Rectangulo en coordenadas de MUNDO, centrado en (cx,cy).
    void worldRect(Scene& scene, SDL_Renderer* r, float cx, float cy, float w, float h) {
        float l, t, rr, b;
        worldToScreen(scene, cx - w * 0.5f, cy - h * 0.5f, l, t);
        worldToScreen(scene, cx + w * 0.5f, cy + h * 0.5f, rr, b);
        SDL_FRect box{ l, t, rr - l, b - t };
        SDL_RenderRect(r, &box);
    }
}

// --- Paneles: inicio y fin ------------------------------------------------------------

bool LevelEditor::initGui(SDL_Window* window_, SDL_Renderer* renderer) {
    if (guiReady) return true;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    // Sin imgui.ini: los paneles siempre arrancan en su sitio y no se ensucia el repo.
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();

    // La ventana del juego no se escala con la pantalla: con Windows al 150%, 13 px de
    // texto se ven diminutos. Los paneles se agrandan segun la escala de la pantalla.
    window = window_;
    uiScale = SDL_GetWindowDisplayScale(window);
    if (uiScale < 1.0f) uiScale = 1.0f;
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(uiScale);
    style.FontScaleDpi = uiScale;

    if (!ImGui_ImplSDL3_InitForSDLRenderer(window_, renderer)) {
        SDL_Log("LevelEditor: no se pudo iniciar ImGui (SDL3); el editor queda sin paneles.");
        ImGui::DestroyContext();
        window = nullptr;
        return false;
    }
    if (!ImGui_ImplSDLRenderer3_Init(renderer)) {
        SDL_Log("LevelEditor: no se pudo iniciar ImGui (renderer); el editor queda sin paneles.");
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        window = nullptr;
        return false;
    }
    guiReady = true;
    return true;
}

void LevelEditor::shutdownGui() {
    if (!guiReady) return;
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    guiReady = guiFrame = false;
}

void LevelEditor::processEvent(const SDL_Event& e) {
    // Solo mientras se edita: jugando no hay paneles, y ImGui acumularia los eventos en
    // su cola sin procesarlos nunca.
    if (guiReady && editing) ImGui_ImplSDL3_ProcessEvent(&e);
}

// --- Abrir / cerrar / construir ------------------------------------------------------

bool LevelEditor::open(const std::string& levelPath, BuildFn buildFn) {
    close();
    path  = levelPath;
    build = std::move(buildFn); // se guarda aunque falle la lectura: buildInto construye igual

    LevelData loaded;
    if (!loadLevel(levelPath, loaded)) {
        SDL_Log("LevelEditor: no se pudo abrir '%s'; el editor queda desactivado.",
                levelPath.c_str());
        return false;
    }
    level = std::move(loaded);
    levelOpen = true;
    resetHistory();
    return true;
}

void LevelEditor::close() {
    if (windowGrown) restoreWindow();
    if (levelOpen && dirty)
        SDL_Log("LevelEditor: se descartan los cambios sin guardar de '%s'.", path.c_str());
    path.clear();
    build = nullptr;
    level = LevelData();
    levelOpen = editing = dirty = rebuildRequested = pendingRebuild = false;
    selectedId = 0;
    dragging = dragMoved = panning = false;
    message.clear();
    messageTime = 0.0f;
    camSettings.clear();
    undoStack.clear();
    redoStack.clear();
    committed = LevelData();
    uncommitted = false;
    mapTime = mapTimeSeen = 0;
}

bool LevelEditor::confirmDiscard(const char* action) {
    if (!levelOpen || !dirty) return true;

    // Cuadro de dialogo nativo (SDL_ShowMessageBox): funciona jugando y editando, sin
    // depender de los paneles, y bloquea hasta que se responde.
    const SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Guardar" },
        { 0,                                       2, "Descartar" },
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancelar" },
    };
    std::string question = "El nivel tiene cambios sin guardar:\n" + path +
                           "\n\nGuardarlos antes de " + action + "?";
    SDL_MessageBoxData box{};
    box.flags = SDL_MESSAGEBOX_WARNING | SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT;
    box.window = window;
    box.title = "Cambios sin guardar";
    box.message = question.c_str();
    box.numbuttons = 3;
    box.buttons = buttons;

    int choice = 0;
    if (!SDL_ShowMessageBox(&box, &choice)) {
        // Sin dialogo no se puede preguntar. Se sigue (si no, no se podria ni cerrar la
        // ventana) y queda en el log que se pierden los cambios.
        SDL_Log("LevelEditor: no se pudo preguntar (%s); se descartan los cambios.", SDL_GetError());
        return true;
    }
    if (choice == 1) { save(); return !dirty; } // si guardar falla, se cancela
    return choice == 2;                         // Descartar sigue; Cancelar o cerrar, no
}

void LevelEditor::buildInto(Scene& scene) {
    rebuildRequested = false;
    dragging = panning = false;
    if (build) build(scene, level);
    // La fabrica acaba de leer el mapa de disco: esa es la version que se ve.
    mapTime = mapTimeSeen = mapFileTime();
    mapPollTimer = MAP_POLL_SECONDS;
    captureCameraSettings(scene); // antes de que la vista del editor pise la camara
    // Editando, la camara de la escena nueva toma la vista del editor desde el primer
    // frame (si no, se veria un frame con la camara del juego en su posicion inicial).
    if (editing) applyView(scene);
}

void LevelEditor::captureCameraSettings(Scene& scene) {
    camSettings.clear();
    if (Camera* cam = scene.getActiveCamera()) camSettings["zoom"] = cam->getZoom();
    if (FollowCamera* f = findFollow(scene)) {
        camSettings["deadZoneWidth"]  = f->deadZoneWidth;
        camSettings["deadZoneHeight"] = f->deadZoneHeight;
        camSettings["smoothSpeed"]    = f->smoothSpeed;
        camSettings["lookAhead"]      = f->lookAhead;
        camSettings["lookAheadSpeed"] = f->lookAheadSpeed;
    }
}

// --- Bucle ---------------------------------------------------------------------------

void LevelEditor::update(Scene& scene, float dt) {
    if (!levelOpen) return;
    if (messageTime > 0.0f) messageTime -= dt;

    // F2 = Stop/Play. En los dos sentidos la escena se reconstruye desde el MODELO: al
    // entrar, para editar el estado inicial; al salir, para jugar lo editado.
    if (Input::wasPressed(Key::F2)) {
        if (!editing) {
            // Se empieza a editar desde donde estaba mirando la camara del juego.
            if (Camera* cam = scene.getActiveCamera()) {
                viewX = cam->gameObject->transform->x;
                viewY = cam->gameObject->transform->y;
                viewZoom = cam->getZoom();
            }
            editing = true;
            growWindow();
        } else {
            editing = false;
            commitNow(); // lo que se estaba escribiendo cuenta como un paso de deshacer
            // ANTES de reconstruir: la fabrica del juego lee el tamanio de la pantalla
            // (p. ej. para centrar el HUD).
            restoreWindow();
        }
        dragging = panning = false;
        pendingRebuild = false;
        rebuildRequested = true;
        return; // esta escena se va a reemplazar: no tiene sentido tocarla
    }
    if (!editing) return;

    // --- Frame de los paneles ----------------------------------------------------
    // Se abre aqui (lo cierra render) para saber desde ya si el raton y el teclado son
    // de los paneles: ImGui lo calcula al empezar el frame.
    mouseOverGui = keysForGui = false;
    if (guiReady) {
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        guiFrame = true;
        mouseOverGui = ImGui::GetIO().WantCaptureMouse;
        keysForGui   = ImGui::GetIO().WantCaptureKeyboard;
    }

    Camera* cam = scene.getActiveCamera();
    TilemapRenderer* map = findMap(scene);

    // --- Teclas ------------------------------------------------------------------
    // Ctrl+S funciona siempre (los campos ya escribieron en el modelo); el resto no
    // mientras se escribe en un campo: una "g" en un nombre no debe apagar la grilla.
    if (ctrlDown() && Input::wasPressed(Key::S)) save();
    if (!keysForGui) {
        // Escribiendo en un campo, Ctrl+Z es de ImGui (deshace el texto del campo).
        if (ctrlDown() && Input::wasPressed(Key::Z)) { if (shiftDown()) redo(); else undo(); }
        if (ctrlDown() && Input::wasPressed(Key::Y)) redo();
        if (ctrlDown() && Input::wasPressed(Key::D)) duplicateSelected(map);
        if (Input::wasPressed(Key::Delete)) deleteSelected();
        if (!ctrlDown() && Input::wasPressed(Key::G)) {
            snap = !snap;
            showMessage(snap ? "Grilla: medio tile" : "Grilla: libre (pixel a pixel)");
        }
        if (!ctrlDown() && Input::wasPressed(Key::C)) showSolids = !showSolids;
        if (Input::wasPressed(Key::Escape)) { selectedId = 0; dragging = false; }
        if (Input::wasPressed(Key::F5)) { reload(); return; }
    }

    float mx = Input::mouseX(), my = Input::mouseY();
    float zoom = (viewZoom > 0.0f) ? viewZoom : 1.0f;

    // --- Vista: flechas, arrastre con clic derecho/medio y rueda -----------------
    // Se divide por el zoom para que la vista se mueva lo mismo EN PANTALLA a
    // cualquier zoom (con zoom 2, 100 px de pantalla son 50 px de mundo).
    if (!keysForGui) {
        viewX += Input::axis(Key::Left, Key::Right) * PAN_SPEED * dt / zoom;
        viewY += Input::axis(Key::Up, Key::Down)    * PAN_SPEED * dt / zoom;
    }

    if (Input::isMouseDown(MouseButton::Right) || Input::isMouseDown(MouseButton::Middle)) {
        // Arrastrar la vista: el mundo sigue al cursor, asi que la camara va al reves.
        // Solo EMPIEZA fuera de los paneles; ya empezado, sigue aunque pase por encima.
        if (panning) {
            viewX -= (mx - panLastX) / zoom;
            viewY -= (my - panLastY) / zoom;
        }
        if (panning || !mouseOverGui) {
            panning = true;
            panLastX = mx;
            panLastY = my;
        }
    } else {
        panning = false;
    }

    float wheel = mouseOverGui ? 0.0f : Input::mouseWheel(); // sobre un panel, la rueda es suya
    if (wheel != 0.0f && cam) {
        // Zoom HACIA EL CURSOR: el punto del mundo que esta bajo el raton sigue ahi
        // despues del zoom. Se mide antes y despues, y se corrige la vista con la
        // diferencia.
        applyView(scene);
        float beforeX, beforeY;
        cam->screenToWorld(mx, my, beforeX, beforeY);
        viewZoom = std::clamp(viewZoom * std::pow(ZOOM_STEP, wheel), ZOOM_MIN, ZOOM_MAX);
        applyView(scene);
        float afterX, afterY;
        cam->screenToWorld(mx, my, afterX, afterY);
        viewX += beforeX - afterX;
        viewY += beforeY - afterY;
    }
    applyView(scene);

    // --- Seleccion y arrastre ----------------------------------------------------
    int outW = 0, outH = 0;
    SDL_GetCurrentRenderOutputSize(scene.getRenderer(), &outW, &outH);
    bool overBar = my >= outH - barHeight();

    float wx, wy;
    screenToWorld(scene, mx, my, wx, wy);

    if (Input::wasMousePressed(MouseButton::Left) && !overBar && !mouseOverGui) {
        selectedId = pickAt(scene, map, wx, wy);
        dragging = dragMoved = false;
        if (TiledObject* o = findObject(selectedId)) {
            // Se guarda la distancia objeto-cursor: al arrastrar, el objeto no salta
            // para centrarse en el raton.
            float ox, oy;
            toWorld(map, *o, ox, oy);
            dragOffX = ox - wx;
            dragOffY = oy - wy;
            pressX = mx;
            pressY = my;
            dragging = true;
        }
    }
    if (dragging) {
        if (!Input::isMouseDown(MouseButton::Left)) {
            dragging = false;
        } else {
            // Un clic sin mover no debe cambiar nada (con grilla, el objeto saltaria
            // a la linea mas cercana solo por seleccionarlo).
            if (!dragMoved && std::hypot(mx - pressX, my - pressY) >= DRAG_THRESHOLD)
                dragMoved = true;
            if (dragMoved) moveSelected(scene, map, wx + dragOffX, wy + dragOffY);
        }
    }

    // --- Paneles -------------------------------------------------------------------
    if (guiFrame) {
        panelObjects(scene, map);
        panelInspector(scene, map);
        panelCamera();
    }

    // Reconstruir cuando el usuario termino de editar: reconstruir a cada tecla
    // pulsada en un campo seria lento y le quitaria el foco al campo.
    if (pendingRebuild && !(guiFrame && ImGui::IsAnyItemActive())) {
        pendingRebuild = false;
        rebuildRequested = true;
    }

    watchMapFile(dt);
    commitIfIdle();
}

// --- Deshacer / rehacer --------------------------------------------------------------
// El modelo son datos, asi que un paso de deshacer es simplemente una COPIA del modelo
// de antes. Nada de "comandos" con su operacion inversa: para un nivel de pocos
// objetos, copiar todo es mas simple y no se puede equivocar.

void LevelEditor::beginChange() {
    if (!uncommitted) {
        uncommitted = true;
        gestureSelectedId = selectedId;
    }
    dirty = true;
}

void LevelEditor::commitIfIdle() {
    // El gesto sigue mientras se arrastra o hay un campo activo (escribiendo, o
    // arrastrando un valor): todo eso sera UN solo paso de deshacer.
    if (!uncommitted || dragging) return;
    if (guiFrame && ImGui::IsAnyItemActive()) return;
    commitNow();
}

void LevelEditor::commitNow() {
    if (!uncommitted) return;
    undoStack.push_back({ committed, gestureSelectedId, version });
    if (undoStack.size() > MAX_UNDO) undoStack.erase(undoStack.begin());
    redoStack.clear(); // un cambio nuevo corta la rama de rehacer
    committed = level;
    version = ++lastVersion;
    uncommitted = false;
    refreshDirty();
}

void LevelEditor::undo() {
    if (dragging) return;
    commitNow(); // si habia un gesto a medias, primero se anota (y es lo que se deshace)
    if (undoStack.empty()) { showMessage("Nada que deshacer"); return; }

    redoStack.push_back({ level, selectedId, version });
    Snapshot s = std::move(undoStack.back());
    undoStack.pop_back();
    level = committed = std::move(s.level);
    version = s.version;
    selectedId = findObject(s.selectedId) ? s.selectedId : 0;
    refreshDirty();
    // La escena puede diferir en cualquier cosa (objetos, types...): se reconstruye.
    pendingRebuild = false;
    rebuildRequested = true;
    showMessage("Deshecho (quedan " + std::to_string(undoStack.size()) + ")");
}

void LevelEditor::redo() {
    if (dragging) return;
    commitNow(); // un gesto nuevo sin anotar ya habria vaciado la pila de rehacer
    if (redoStack.empty()) { showMessage("Nada que rehacer"); return; }

    undoStack.push_back({ level, selectedId, version });
    Snapshot s = std::move(redoStack.back());
    redoStack.pop_back();
    level = committed = std::move(s.level);
    version = s.version;
    selectedId = findObject(s.selectedId) ? s.selectedId : 0;
    refreshDirty();
    pendingRebuild = false;
    rebuildRequested = true;
    showMessage("Rehecho (quedan " + std::to_string(redoStack.size()) + ")");
}

void LevelEditor::resetHistory() {
    undoStack.clear();
    redoStack.clear();
    committed = level;
    uncommitted = false;
    version = savedVersion = ++lastVersion; // lo que hay en memoria es lo del disco
    refreshDirty();
}

// --- Avisos y recarga automatica del mapa ---------------------------------------------

std::string LevelEditor::objectWarning(const TilemapRenderer* map, const TiledObject& o) const {
    if (!map || map->getMapWidth() <= 0 || map->getMapHeight() <= 0 ||
        map->getTileWidth() <= 0 || map->getTileHeight() <= 0) return "";

    // Todo en pixeles del MAPA, que es el espacio del archivo: no depende de la escala.
    float tw = (float)map->getTileWidth(), th = (float)map->getTileHeight();
    if (o.cx < 0.0f || o.cy < 0.0f ||
        o.cx >= map->getMapWidth() * tw || o.cy >= map->getMapHeight() * th)
        return "fuera del mapa";
    // Se mira solo el CENTRO: los pinchos o una meta tocan el suelo con el borde, y eso
    // es correcto. Un centro dentro de la pared casi seguro es un error (el jugador
    // naceria atascado, una fruta quedaria inalcanzable).
    if (map->isSolidCell((int)std::floor(o.cx / tw), (int)std::floor(o.cy / th)))
        return "dentro de un tile solido";
    return "";
}

int64_t LevelEditor::mapFileTime() const {
    SDL_PathInfo info;
    if (level.mapPath.empty() || !SDL_GetPathInfo(level.mapPath.c_str(), &info)) return 0;
    return (int64_t)info.modify_time;
}

void LevelEditor::watchMapFile(float dt) {
    // Se consulta la fecha del archivo cada medio segundo: es barato y no necesita
    // hilos ni avisos del sistema operativo.
    mapPollTimer -= dt;
    if (mapPollTimer > 0.0f) return;
    mapPollTimer = MAP_POLL_SECONDS;

    int64_t t = mapFileTime();
    if (t == 0) return;                          // justo ahora no se puede leer: luego
    if (t == mapTime) { mapTimeSeen = t; return; } // sin cambios
    // Cambio. Se espera a ver la MISMA fecha en dos consultas seguidas: si Tiled aun
    // estuviera escribiendo, se leeria un JSON a medias.
    if (t != mapTimeSeen) { mapTimeSeen = t; return; }
    // Tampoco en medio de un gesto (arrastrando o escribiendo): se reintenta despues.
    if (dragging || (guiFrame && ImGui::IsAnyItemActive())) return;

    // Solo el mapa cambia: los objetos (el modelo) quedan como estan. buildInto
    // actualiza mapTime.
    showMessage("Mapa recargado (se guardo en Tiled)");
    rebuildRequested = true;
}

// --- Consultas y edicion -------------------------------------------------------------

const TiledObject* LevelEditor::findObject(int id) const {
    if (id <= 0) return nullptr;
    for (const TiledObject& o : level.objects)
        if (o.id == id) return &o;
    return nullptr;
}

TiledObject* LevelEditor::findObject(int id) {
    return const_cast<TiledObject*>(static_cast<const LevelEditor*>(this)->findObject(id));
}

void LevelEditor::toWorld(const TilemapRenderer* map, const TiledObject& o,
                          float& wx, float& wy) const {
    if (map) map->mapToWorld(o.cx, o.cy, wx, wy);
    else { wx = o.cx; wy = o.cy; }
}

void LevelEditor::toMap(const TilemapRenderer* map, float wx, float wy,
                        float& mx, float& my) const {
    if (map) map->worldToMap(wx, wy, mx, my);
    else { mx = wx; my = wy; }
}

void LevelEditor::pickRect(Scene& scene, const TilemapRenderer* map, const TiledObject& o,
                           float& left, float& top, float& right, float& bottom) const {
    // 1) Lo que se ve: el collider del GameObject que creo la fabrica.
    if (GameObject* go = findGameObject(scene, o.id)) {
        if (BoxCollider* c = go->getComponent<BoxCollider>()) {
            left   = c->centerX() - c->halfW();
            right  = c->centerX() + c->halfW();
            top    = c->centerY() - c->halfH();
            bottom = c->centerY() + c->halfH();
            return;
        }
    }
    // 2) Un objeto con area propia en el archivo (una zona): su rectangulo.
    if (o.w > 0.0f && o.h > 0.0f) {
        TiledObject corner = o;
        corner.cx = o.cx - o.w * 0.5f;
        corner.cy = o.cy - o.h * 0.5f;
        toWorld(map, corner, left, top);
        corner.cx = o.cx + o.w * 0.5f;
        corner.cy = o.cy + o.h * 0.5f;
        toWorld(map, corner, right, bottom);
        return;
    }
    // 3) Un punto sin nada visible: un cuadrado de tamanio fijo EN PANTALLA.
    float wx, wy;
    toWorld(map, o, wx, wy);
    float half = PICK_HALF_SCREEN / zoomOf(scene);
    left = wx - half; right = wx + half;
    top  = wy - half; bottom = wy + half;
}

int LevelEditor::pickAt(Scene& scene, const TilemapRenderer* map, float wx, float wy) const {
    // Entre los objetos bajo el cursor gana el de centro mas cercano: con dos objetos
    // solapados, se elige el que el usuario tiene mas "apuntado".
    int best = 0;
    float bestDist = 0.0f;
    for (const TiledObject& o : level.objects) {
        float l, t, r, b;
        pickRect(scene, map, o, l, t, r, b);
        if (wx < l || wx > r || wy < t || wy > b) continue;
        float d = std::hypot(wx - (l + r) * 0.5f, wy - (t + b) * 0.5f);
        if (best == 0 || d < bestDist) { best = o.id; bestDist = d; }
    }
    return best;
}

void LevelEditor::snapStep(const TilemapRenderer* map, float& stepX, float& stepY) const {
    // Medio tile (8 px con tiles de 16): incluye los centros y los bordes de las celdas.
    // Sin grilla, pixel entero: asi el archivo guarda numeros limpios.
    stepX = stepY = 1.0f;
    if (snap) {
        stepX = (map && map->getTileWidth()  > 0) ? map->getTileWidth()  * 0.5f : 8.0f;
        stepY = (map && map->getTileHeight() > 0) ? map->getTileHeight() * 0.5f : 8.0f;
    }
}

void LevelEditor::moveSelected(Scene& scene, const TilemapRenderer* map, float wx, float wy) {
    TiledObject* o = findObject(selectedId);
    if (!o) return;

    float mx, my, stepX, stepY;
    toMap(map, wx, wy, mx, my);
    snapStep(map, stepX, stepY);
    setObjectPosition(scene, map, *o,
                      std::round(mx / stepX) * stepX, std::round(my / stepY) * stepY);
}

void LevelEditor::setObjectPosition(Scene& scene, const TilemapRenderer* map, TiledObject& o,
                                    float mx, float my) {
    if (mx == o.cx && my == o.cy) return;
    beginChange();
    o.cx = mx;
    o.cy = my;

    // La escena esta congelada: basta con mover sus GameObjects, no hace falta
    // reconstruirla. Al volver a jugar (F2) se reconstruye igual desde el modelo.
    float nx, ny;
    toWorld(map, o, nx, ny);
    for (const auto& obj : scene.getObjects()) {
        if (obj->levelObjectId == o.id) {
            obj->transform->x = nx;
            obj->transform->y = ny;
        }
    }
}

void LevelEditor::createObject(const TilemapRenderer* map, const std::string& type) {
    TiledObject t;
    // Si ya hay otro del mismo type, sus propiedades y su tamanio sirven de plantilla
    // (una fruta nueva nace con "fruit"; sin plantilla, nace sin propiedades).
    for (const TiledObject& other : level.objects) {
        if (other.type == type) { t = other; break; }
    }
    t.id = level.nextObjectId++;
    t.type = type;
    t.name.clear();

    // Aparece en el centro de la vista, ajustado a la grilla.
    float mx, my, stepX, stepY;
    toMap(map, viewX, viewY, mx, my);
    snapStep(map, stepX, stepY);
    t.cx = std::round(mx / stepX) * stepX;
    t.cy = std::round(my / stepY) * stepY;

    level.objects.push_back(t);
    selectedId = t.id;
    markChanged(true);
    showMessage("Creado #" + std::to_string(t.id) + " " + type);
}

void LevelEditor::duplicateSelected(const TilemapRenderer* map) {
    const TiledObject* o = findObject(selectedId);
    if (!o) return;
    TiledObject copy = *o; // copia ANTES del push_back: el vector puede reubicarse
    copy.id = level.nextObjectId++;
    // Un tile a la derecha, para que no quede escondido justo encima del original.
    copy.cx += (map && map->getTileWidth() > 0) ? (float)map->getTileWidth() : 16.0f;
    level.objects.push_back(copy);
    selectedId = copy.id;
    markChanged(true);
    showMessage("Duplicado como #" + std::to_string(copy.id));
}

void LevelEditor::deleteSelected() {
    auto it = std::find_if(level.objects.begin(), level.objects.end(),
                           [this](const TiledObject& o) { return o.id == selectedId; });
    if (it == level.objects.end()) return;
    markChanged(true); // ANTES de quitar la seleccion: deshacer vuelve a elegir este objeto
    showMessage("Borrado #" + std::to_string(it->id) + " " + it->type);
    level.objects.erase(it);
    selectedId = 0;
    dragging = false;
}

void LevelEditor::markChanged(bool needsRebuild) {
    beginChange();
    if (needsRebuild) pendingRebuild = true;
}

void LevelEditor::applyView(Scene& scene) const {
    Camera* cam = scene.getActiveCamera();
    if (!cam) return;
    cam->gameObject->transform->x = viewX;
    cam->gameObject->transform->y = viewY;
    cam->zoom = viewZoom;
}

void LevelEditor::save() {
    // Lo que se estaba escribiendo se anota como un paso: asi "lo guardado" es una
    // version de la historia, y deshacer hasta ella vuelve a quitar el *SIN GUARDAR.
    commitNow();
    if (!saveLevel(path, level)) {
        showMessage("ERROR al guardar (ver el log)");
        return;
    }
    savedVersion = version;
    refreshDirty();
    // La ruta es relativa al directorio de trabajo, que no siempre es la carpeta del
    // proyecto: se deja en el log la ruta completa para saber donde quedo.
    char* cwd = SDL_GetCurrentDirectory(); // termina en separador
    SDL_Log("LevelEditor: nivel guardado en %s%s", cwd ? cwd : "", path.c_str());
    SDL_free(cwd);
    showMessage("Guardado: " + path);
}

void LevelEditor::reload() {
    // El mapa de Tiled siempre se relee (la fabrica lo carga de disco al reconstruir):
    // es lo que se quiere despues de pintar en Tiled. El .level.json solo se relee si
    // no hay cambios sin guardar, para no tirarlos sin avisar.
    if (!dirty) {
        LevelData fresh;
        if (loadLevel(path, fresh)) {
            level = std::move(fresh);
            resetHistory(); // la historia era del modelo anterior (pudo cambiar a mano)
        }
        showMessage("Recargados el mapa y el nivel");
    } else {
        showMessage("Recargado el mapa (el nivel tiene cambios sin guardar)");
    }
    if (!findObject(selectedId)) selectedId = 0;
    rebuildRequested = true;
}

void LevelEditor::showMessage(const std::string& textToShow) {
    message = textToShow;
    messageTime = MESSAGE_SECONDS;
}

// --- Paneles -------------------------------------------------------------------------
// Textos SIN tildes, como los comentarios: el proyecto compila sin /utf-8, asi que una
// letra acentuada dentro de un string saldria mal en pantalla.

void LevelEditor::panelObjects(Scene& scene, const TilemapRenderer* map) {
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(240.0f * uiScale, 420.0f * uiScale), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Objetos")) { ImGui::End(); return; }

    ImGui::BeginDisabled(undoStack.empty() && !uncommitted);
    if (ImGui::Button("Deshacer")) undo();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(redoStack.empty());
    if (ImGui::Button("Rehacer")) redo();
    ImGui::EndDisabled();

    if (ImGui::Button("Nuevo")) ImGui::OpenPopup("nuevo");
    ImGui::SameLine();
    ImGui::BeginDisabled(findObject(selectedId) == nullptr);
    if (ImGui::Button("Duplicar")) duplicateSelected(map);
    ImGui::SameLine();
    if (ImGui::Button("Borrar")) deleteSelected();
    ImGui::EndDisabled();

    // "Nuevo": los type que YA hay en el nivel, o uno escrito a mano. El motor no
    // conoce los type; solo los recoge del archivo.
    if (ImGui::BeginPopup("nuevo")) {
        std::set<std::string> types;
        for (const TiledObject& o : level.objects)
            if (!o.type.empty()) types.insert(o.type);

        ImGui::TextDisabled("Tipos del nivel");
        for (const std::string& type : types) {
            if (ImGui::Selectable(type.c_str())) {
                createObject(map, type);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::Separator();
        ImGui::TextDisabled("Otro tipo");
        ImGui::SetNextItemWidth(140.0f * uiScale);
        ImGui::InputText("##otro", &newType);
        ImGui::SameLine();
        ImGui::BeginDisabled(newType.empty());
        if (ImGui::Button("Crear")) {
            createObject(map, newType);
            newType.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }

    ImGui::Separator();
    ImGui::TextDisabled("Doble clic: centrar la vista");
    ImGui::BeginChild("lista");
    for (const TiledObject& o : level.objects) {
        bool warn = !objectWarning(map, o).empty();
        std::string itemLabel = std::string(warn ? "! " : "") + "#" + std::to_string(o.id) +
                                "  " + (o.type.empty() ? "(sin type)" : o.type);
        if (!o.name.empty()) itemLabel += "  (" + o.name + ")";

        // "###item": el id del widget no depende del texto (que cambia con el type).
        itemLabel += "###item";
        ImGui::PushID(o.id);
        if (warn) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.15f, 1.0f));
        bool clicked = ImGui::Selectable(itemLabel.c_str(), o.id == selectedId,
                                         ImGuiSelectableFlags_AllowDoubleClick);
        if (warn) ImGui::PopStyleColor();
        if (clicked) {
            selectedId = o.id;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                toWorld(map, o, viewX, viewY);
                applyView(scene);
            }
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::End();
}

void LevelEditor::panelInspector(Scene& scene, const TilemapRenderer* map) {
    int outW = 0, outH = 0;
    SDL_GetCurrentRenderOutputSize(scene.getRenderer(), &outW, &outH);
    float panelW = SIDE_PANEL_W * uiScale;
    ImGui::SetNextWindowPos(ImVec2(outW - panelW - 10.0f, 10.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(panelW, 350.0f * uiScale), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Inspector")) { ImGui::End(); return; }

    // Se busca DESPUES del panel de objetos: si ahi se creo o duplico algo, el vector
    // pudo reubicarse y un puntero de antes no valdria.
    TiledObject* o = findObject(selectedId);
    if (!o) {
        ImGui::TextDisabled("Sin seleccion.");
        ImGui::TextDisabled("Clic sobre un objeto en el mundo");
        ImGui::TextDisabled("o en la lista de Objetos.");
        ImGui::End();
        return;
    }

    ImGui::Text("id: %d", o->id);
    std::string warning = objectWarning(map, *o);
    if (!warning.empty())
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.15f, 1.0f), "Aviso: centro %s", warning.c_str());

    // type: lo interpreta la fabrica del juego -> reconstruir al terminar de escribir.
    if (ImGui::InputText("type", &o->type)) markChanged(true);
    // name: solo identifica; la fabrica no lo usa.
    if (ImGui::InputText("nombre", &o->name)) markChanged(false);

    ImGui::SeparatorText("Posicion (px del mapa, centro)");
    float stepX, stepY;
    snapStep(map, stepX, stepY);
    float x = o->cx, y = o->cy;
    if (ImGui::InputFloat("x", &x, stepX, stepX * 4.0f, "%g"))
        setObjectPosition(scene, map, *o, x, o->cy);
    if (ImGui::InputFloat("y", &y, stepY, stepY * 4.0f, "%g"))
        setObjectPosition(scene, map, *o, o->cx, y);

    ImGui::SeparatorText("Tamanio (0 = punto)");
    float w = o->w, h = o->h;
    if (ImGui::InputFloat("w", &w, 1.0f, 8.0f, "%g")) { o->w = std::max(0.0f, w); markChanged(true); }
    if (ImGui::InputFloat("h", &h, 1.0f, 8.0f, "%g")) { o->h = std::max(0.0f, h); markChanged(true); }

    ImGui::SeparatorText("Propiedades");
    // Lo que se borre se anota y se quita DESPUES de recorrer: borrar de un std::map
    // mientras se lo recorre invalidaria el iterador.
    std::string removeText, removeNumber;
    for (auto& kv : o->stringProps) {
        ImGui::PushID(("s:" + kv.first).c_str());
        if (ImGui::InputText(kv.first.c_str(), &kv.second)) markChanged(true);
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) removeText = kv.first;
        ImGui::PopID();
    }
    for (auto& kv : o->numberProps) {
        ImGui::PushID(("n:" + kv.first).c_str());
        double v = kv.second;
        if (ImGui::InputDouble(kv.first.c_str(), &v, 0.0, 0.0, "%g")) { kv.second = v; markChanged(true); }
        ImGui::SameLine();
        if (ImGui::SmallButton("x")) removeNumber = kv.first;
        ImGui::PopID();
    }
    if (!removeText.empty())   { o->stringProps.erase(removeText);   markChanged(true); }
    if (!removeNumber.empty()) { o->numberProps.erase(removeNumber); markChanged(true); }
    if (o->stringProps.empty() && o->numberProps.empty()) ImGui::TextDisabled("(ninguna)");

    // Agregar una propiedad nueva (texto o numero).
    ImGui::Spacing();
    ImGui::SetNextItemWidth(120.0f * uiScale);
    ImGui::InputText("##nuevaProp", &newPropKey);
    ImGui::SameLine();
    if (ImGui::RadioButton("texto", !newPropIsNumber)) newPropIsNumber = false;
    ImGui::SameLine();
    if (ImGui::RadioButton("numero", newPropIsNumber)) newPropIsNumber = true;
    bool exists = o->stringProps.count(newPropKey) || o->numberProps.count(newPropKey);
    ImGui::BeginDisabled(newPropKey.empty() || exists);
    if (ImGui::Button("Agregar propiedad")) {
        if (newPropIsNumber) o->numberProps[newPropKey] = 0.0;
        else                 o->stringProps[newPropKey] = "";
        newPropKey.clear();
        markChanged(true);
    }
    ImGui::EndDisabled();

    ImGui::End();
}

void LevelEditor::panelCamera() {
    ImGuiIO& io = ImGui::GetIO();
    float panelW = SIDE_PANEL_W * uiScale;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - panelW - 10.0f, 20.0f + 350.0f * uiScale),
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(panelW, 280.0f * uiScale), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Camara")) { ImGui::End(); return; }

    if (camSettings.empty()) {
        ImGui::TextDisabled("La escena no tiene camara.");
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Previsualizar al inicio", &showCameraPreview);
    ImGui::TextDisabled("* = definido en el nivel;");
    ImGui::TextDisabled("  sin *, es el valor del juego");
    ImGui::Separator();

    std::string resetKey;
    for (const std::string& key : levelCameraKeys()) {
        auto it = camSettings.find(key);
        if (it == camSettings.end()) continue;

        bool inLevel = level.camera.count(key) > 0;
        bool isZoom = (key == "zoom");
        bool inPixels = (key == "deadZoneWidth" || key == "deadZoneHeight" || key == "lookAhead");
        float speed = isZoom ? 0.01f : inPixels ? 1.0f : 0.05f;
        float minValue = isZoom ? 0.1f : 0.0f; // zoom 0 no se puede dibujar
        // "###valor": el id del campo NO puede depender del texto. Si dependiera, al
        // pasar de "zoom" a "zoom *" (el primer cambio lo vuelve del nivel) ImGui lo
        // tomaria por otro campo y soltaria el arrastre a medio camino.
        std::string fieldLabel = key + (inLevel ? " *" : "") + "###valor";

        ImGui::PushID(key.c_str());
        ImGui::SetNextItemWidth(110.0f * uiScale);
        float v = it->second;
        if (ImGui::DragFloat(fieldLabel.c_str(), &v, speed, minValue, 10000.0f, "%.2f")) {
            // Editar un valor lo vuelve explicito en el nivel. No hace falta
            // reconstruir: la previsualizacion lee camSettings y F2 reconstruye igual.
            it->second = v;
            level.camera[key] = v;
            markChanged(false);
        }
        if (inLevel) {
            ImGui::SameLine();
            if (ImGui::SmallButton("restablecer")) resetKey = key;
        }
        ImGui::PopID();
    }
    // Quitarlo del nivel devuelve el valor del juego; para saber cual es, hay que
    // reconstruir (lo pone la fabrica).
    if (!resetKey.empty()) {
        level.camera.erase(resetKey);
        markChanged(true);
    }

    ImGui::End();
}

// --- Dibujo --------------------------------------------------------------------------

void LevelEditor::drawCameraPreview(Scene& scene) {
    // Lo que vera el jugador AL EMPEZAR: la pantalla a zoom del juego, centrada en el
    // objetivo de la FollowCamera, y su zona muerta. No aplica los limites del mapa ni
    // el look-ahead: es una guia, no una simulacion.
    FollowCamera* follow = findFollow(scene);
    GameObject* target = follow ? follow->getTarget() : nullptr;
    if (!target) return;

    SDL_Renderer* r = scene.getRenderer();
    // La vista del jugador es la de la ventana del JUEGO (la de antes de maximizar).
    int outW = gamePixelW, outH = gamePixelH;
    if (outW <= 0 || outH <= 0) SDL_GetCurrentRenderOutputSize(r, &outW, &outH);

    auto get = [this](const char* key, float def) {
        auto it = camSettings.find(key);
        return it == camSettings.end() ? def : it->second;
    };
    float gameZoom = get("zoom", 1.0f);
    if (gameZoom <= 0.0f) gameZoom = 1.0f;
    float cx = target->transform->x, cy = target->transform->y;

    float deadW = get("deadZoneWidth", 0.0f), deadH = get("deadZoneHeight", 0.0f);
    SDL_SetRenderDrawColor(r, 120, 255, 120, 220);
    worldRect(scene, r, cx, cy, outW / gameZoom, outH / gameZoom);
    SDL_SetRenderDrawColor(r, 80, 170, 255, 230);
    worldRect(scene, r, cx, cy, deadW, deadH);

    // La etiqueta va sobre la zona muerta y no sobre la vista: con zoom 1 la vista del
    // juego ocupa toda la pantalla y su esquina queda fuera.
    float lx, ly;
    worldToScreen(scene, cx - deadW * 0.5f, cy - deadH * 0.5f, lx, ly);
    float ts = textScale(), lineH = (CHAR + 4.0f) * ts;
    label(r, lx, ly - 2.0f * lineH, "Camara del juego al empezar:", ts, 80, 170, 255);
    label(r, lx, ly - lineH, "zona muerta (azul), vista (verde)", ts, 80, 170, 255);
}

void LevelEditor::render(Scene& scene) {
    if (!levelOpen) return;

    SDL_Renderer* r = scene.getRenderer();
    int outW = 0, outH = 0;
    SDL_GetCurrentRenderOutputSize(r, &outW, &outH);

    // Transparencias para la grilla, los fondos del texto y la barra.
    SDL_BlendMode oldBlend = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(r, &oldBlend);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    if (!editing) {
        // Jugando: solo un recordatorio en la esquina.
        float ts = textScale();
        label(r, 8.0f, outH - (CHAR + 8.0f) * ts,
              dirty ? "F2: editor  (hay cambios SIN GUARDAR)" : "F2: editor", ts, 255, 255, 255);
        SDL_SetRenderDrawBlendMode(r, oldBlend);
        return;
    }

    TilemapRenderer* map = findMap(scene);
    float zoom = zoomOf(scene);

    // --- 1) Grilla y celdas solidas del mapa (solo la parte visible) -----------------
    if (map && map->getMapWidth() > 0 && map->getMapHeight() > 0) {
        float cw = map->getTileWorldWidth(), ch = map->getTileWorldHeight();
        float ox = map->getOriginX(), oy = map->getOriginY();

        float wl, wt, wr, wb;
        screenToWorld(scene, 0.0f, 0.0f, wl, wt);
        screenToWorld(scene, (float)outW, (float)outH, wr, wb);
        int c0, r0, c1, r1;
        map->worldToCell(wl, wt, c0, r0);
        map->worldToCell(wr, wb, c1, r1);
        c0 = std::max(c0, 0);
        r0 = std::max(r0, 0);
        c1 = std::min(c1, map->getMapWidth() - 1);
        r1 = std::min(r1, map->getMapHeight() - 1);

        if (c0 <= c1 && r0 <= r1) {
            // Lineas de la grilla, solo si las celdas se ven de un tamanio razonable
            // (alejado, serian una mancha).
            if (cw * zoom >= 8.0f) {
                SDL_SetRenderDrawColor(r, 255, 255, 255, 40);
                float ax, ay, bx, by;
                for (int c = c0; c <= c1 + 1; ++c) {
                    worldToScreen(scene, ox + c * cw, oy + r0 * ch, ax, ay);
                    worldToScreen(scene, ox + c * cw, oy + (r1 + 1) * ch, bx, by);
                    SDL_RenderLine(r, ax, ay, bx, by);
                }
                for (int row = r0; row <= r1 + 1; ++row) {
                    worldToScreen(scene, ox + c0 * cw, oy + row * ch, ax, ay);
                    worldToScreen(scene, ox + (c1 + 1) * cw, oy + row * ch, bx, by);
                    SDL_RenderLine(r, ax, ay, bx, by);
                }
            }
            // Celdas solidas: lo que la fisica va a tratar como pared/suelo. Sirve para
            // revisar lo que se marco en Tiled (tiles "solid" y capas "collision").
            if (showSolids) {
                for (int row = r0; row <= r1; ++row) {
                    for (int c = c0; c <= c1; ++c) {
                        if (!map->isSolidCell(c, row)) continue;
                        float sl, st, sr, sb;
                        worldToScreen(scene, ox + c * cw, oy + row * ch, sl, st);
                        worldToScreen(scene, ox + (c + 1) * cw, oy + (row + 1) * ch, sr, sb);
                        SDL_FRect cell{ sl, st, sr - sl, sb - st };
                        SDL_SetRenderDrawColor(r, 255, 60, 60, 60);
                        SDL_RenderFillRect(r, &cell);
                        SDL_SetRenderDrawColor(r, 255, 60, 60, 150);
                        SDL_RenderRect(r, &cell);
                    }
                }
            }
        }

        // Borde del mapa completo.
        float ml, mt, mr, mb;
        worldToScreen(scene, ox, oy, ml, mt);
        worldToScreen(scene, ox + map->getWorldWidth(), oy + map->getWorldHeight(), mr, mb);
        SDL_FRect border{ ml, mt, mr - ml, mb - mt };
        SDL_SetRenderDrawColor(r, 80, 170, 255, 220);
        SDL_RenderRect(r, &border);
    }

    // --- 2) Previsualizacion de la camara del juego --------------------------------
    if (showCameraPreview) drawCameraPreview(scene);

    // --- 3) Objetos del nivel: area de seleccion, punto y etiqueta -----------------
    // Amarillo = seleccionado, naranja = con aviso, celeste = el resto.
    int warnings = 0;
    for (const TiledObject& o : level.objects) {
        bool selected = (o.id == selectedId);
        std::string warning = objectWarning(map, o);
        if (!warning.empty()) ++warnings;

        float l, t, rr, b, sl, st, sr, sb;
        pickRect(scene, map, o, l, t, rr, b);
        worldToScreen(scene, l, t, sl, st);
        worldToScreen(scene, rr, b, sr, sb);
        SDL_FRect box{ sl, st, sr - sl, sb - st };
        if (selected) {
            SDL_SetRenderDrawColor(r, 255, 220, 0, 255);
            SDL_RenderRect(r, &box);
            SDL_FRect outer{ box.x - 1.0f, box.y - 1.0f, box.w + 2.0f, box.h + 2.0f };
            SDL_RenderRect(r, &outer);
        } else if (!warning.empty()) {
            SDL_SetRenderDrawColor(r, 255, 150, 40, 230);
            SDL_RenderRect(r, &box);
        } else {
            SDL_SetRenderDrawColor(r, 0, 200, 255, 170);
            SDL_RenderRect(r, &box);
        }

        // Una crucecita en la posicion que se guarda en el archivo.
        float wx, wy, px, py;
        toWorld(map, o, wx, wy);
        worldToScreen(scene, wx, wy, px, py);
        SDL_SetRenderDrawColor(r, 255, 255, 255, 230);
        SDL_RenderLine(r, px - 4.0f, py, px + 4.0f, py);
        SDL_RenderLine(r, px, py - 4.0f, px, py + 4.0f);

        std::string name = o.type.empty() ? "(sin type)" : o.type;
        if (!warning.empty()) name += "  ! " + warning;
        float ts = textScale(), ly = st - (CHAR + 4.0f) * ts;
        if (selected)              label(r, sl, ly, name, ts, 255, 220, 0);
        else if (!warning.empty()) label(r, sl, ly, name, ts, 255, 150, 40);
        else                       label(r, sl, ly, name, ts, 255, 255, 255);
    }

    // --- 4) Barra de estado ---------------------------------------------------------
    // Una linea grande (que hay seleccionado) y tres chicas (atajos y raton). Todo en
    // escala entera (textScale): la fuente de 8x8 se ve nitida.
    float ts = textScale();
    float big = ts + 1.0f;
    float barY = outH - barHeight();
    SDL_FRect bar{ 0.0f, barY, (float)outW, barHeight() };
    SDL_SetRenderDrawColor(r, 0, 0, 0, 190);
    SDL_RenderFillRect(r, &bar);

    std::string line1 = dirty ? "EDITOR *SIN GUARDAR  " : "EDITOR  ";
    if (warnings > 0)
        line1 += "[" + std::to_string(warnings) + (warnings == 1 ? " aviso]  " : " avisos]  ");
    if (const TiledObject* o = findObject(selectedId)) {
        line1 += "#" + std::to_string(o->id) + " " + o->type +
                 "  x=" + num(o->cx) + " y=" + num(o->cy);
        for (const auto& kv : o->stringProps) line1 += "  " + kv.first + "=" + kv.second;
        for (const auto& kv : o->numberProps) line1 += "  " + kv.first + "=" + num((float)kv.second);
    } else {
        line1 += "(clic sobre un objeto para elegirlo)";
    }
    float y = barY + 6.0f;
    if (dirty) text(r, 10.0f, y, line1, big, 255, 200, 80); // naranja: pendiente
    else       text(r, 10.0f, y, line1, big, 255, 255, 255);
    y += CHAR * big + 6.0f;

    char zoomBuf[16];
    std::snprintf(zoomBuf, sizeof(zoomBuf), "%.2f", zoom);
    std::string line2 = "F2 jugar | Ctrl+S guardar | F5 recargar | Ctrl+Z/Y deshacer/rehacer"
                        " | Ctrl+D duplicar | Supr borrar";
    std::string line3 = std::string("G grilla: ") + (snap ? "medio tile" : "libre") +
                        " | C solidos: " + (showSolids ? "si" : "no") +
                        " | Clic der./flechas: vista | Rueda: zoom x" + zoomBuf;
    text(r, 10.0f, y, line2, ts, 190, 190, 190);
    y += CHAR * ts + 4.0f;
    text(r, 10.0f, y, line3, ts, 190, 190, 190);
    y += CHAR * ts + 4.0f;

    float cwx, cwy, cmx, cmy;
    screenToWorld(scene, Input::mouseX(), Input::mouseY(), cwx, cwy);
    toMap(map, cwx, cwy, cmx, cmy);
    std::string line4 = "Raton: mapa (" + num(std::floor(cmx)) + ", " + num(std::floor(cmy)) + ")";
    if (map) {
        int col, row;
        map->worldToCell(cwx, cwy, col, row);
        line4 += "  celda (" + std::to_string(col) + ", " + std::to_string(row) + ")";
    }
    if (messageTime > 0.0f) line4 += "     >> " + message;
    text(r, 10.0f, y, line4, ts, 190, 190, 190);

    SDL_SetRenderDrawBlendMode(r, oldBlend);

    // --- 5) Paneles: encima de todo -------------------------------------------------
    if (guiFrame) {
        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), r);
        guiFrame = false;
    }
}

// --- Ventana y escala ----------------------------------------------------------------

float LevelEditor::textScale() const {
    // Redondeada: 1.5 -> 2. Un 8x8 escalado a 1.5 tendria pixeles de distinto tamanio.
    return std::max(1.0f, std::round(uiScale));
}

float LevelEditor::barHeight() const {
    float ts = textScale();
    return 6.0f + CHAR * (ts + 1.0f) + 6.0f + 3.0f * (CHAR * ts + 4.0f) + 4.0f;
}

void LevelEditor::growWindow() {
    if (!window || windowGrown) return;

    // Se guarda TODO lo necesario para dejar la ventana exactamente como estaba.
    SDL_GetWindowSize(window, &gameW, &gameH);
    SDL_GetWindowSizeInPixels(window, &gamePixelW, &gamePixelH);
    SDL_WindowFlags flags = SDL_GetWindowFlags(window);
    gameWasResizable = (flags & SDL_WINDOW_RESIZABLE) != 0;
    gameWasMaximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;

    // Maximizar exige que la ventana sea redimensionable. Mientras se edita tambien se
    // puede cambiar de tamanio a mano.
    SDL_SetWindowResizable(window, true);
    SDL_MaximizeWindow(window);
    // En Windows maximizar es asincrono: se espera a que termine, para que la escena
    // que se construye enseguida ya vea el tamanio nuevo.
    SDL_SyncWindow(window);
    windowGrown = true;
}

void LevelEditor::restoreWindow() {
    if (!window || !windowGrown) return;
    if (!gameWasMaximized) {
        SDL_RestoreWindow(window);
        SDL_SetWindowSize(window, gameW, gameH);
    }
    SDL_SetWindowResizable(window, gameWasResizable);
    SDL_SyncWindow(window);
    windowGrown = false;
}
