#include "LevelEditor.h"
#include "Scene.h"
#include "GameObject.h"
#include "Transform.h"
#include "Camera.h"
#include "BoxCollider.h"
#include "TilemapRenderer.h"
#include "Input.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
    const float PICK_HALF_SCREEN = 12.0f; // medio lado (px de pantalla) del area de clic sin collider
    const float DRAG_THRESHOLD   = 3.0f;  // px de pantalla antes de empezar a mover (un clic no mueve)
    const float PAN_SPEED        = 900.0f;// px de pantalla por segundo con las flechas
    const float ZOOM_STEP        = 1.15f; // factor por muesca de la rueda
    const float ZOOM_MIN = 0.25f, ZOOM_MAX = 4.0f;
    const float MESSAGE_SECONDS  = 3.0f;
    const float BAR_H            = 56.0f; // alto de la barra de estado (abajo)
    const float CHAR             = (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE; // 8 px

    TilemapRenderer* findMap(Scene& scene) {
        for (const auto& obj : scene.getObjects())
            if (TilemapRenderer* tm = obj->getComponent<TilemapRenderer>()) return tm;
        return nullptr;
    }

    GameObject* findGameObject(Scene& scene, int levelId) {
        if (levelId <= 0) return nullptr;
        for (const auto& obj : scene.getObjects())
            if (obj->levelObjectId == levelId) return obj.get();
        return nullptr;
    }

    bool ctrlDown() { return Input::isDown(Key::LCtrl) || Input::isDown(Key::RCtrl); }

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

    // Texto pequeno con un fondo oscuro detras, para que se lea sobre cualquier cosa.
    void label(SDL_Renderer* r, float x, float y, const std::string& s,
               Uint8 cr, Uint8 cg, Uint8 cb) {
        SDL_FRect bg{ x - 2.0f, y - 2.0f, s.size() * CHAR + 4.0f, CHAR + 4.0f };
        SDL_SetRenderDrawColor(r, 0, 0, 0, 170);
        SDL_RenderFillRect(r, &bg);
        text(r, x, y, s, 1.0f, cr, cg, cb);
    }
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
    return true;
}

void LevelEditor::close() {
    if (levelOpen && dirty)
        SDL_Log("LevelEditor: se descartan los cambios sin guardar de '%s'.", path.c_str());
    path.clear();
    build = nullptr;
    level = LevelData();
    levelOpen = editing = dirty = rebuildRequested = false;
    selectedId = 0;
    dragging = dragMoved = panning = false;
    message.clear();
    messageTime = 0.0f;
}

void LevelEditor::buildInto(Scene& scene) {
    rebuildRequested = false;
    dragging = panning = false;
    if (build) build(scene, level);
    // Editando, la camara de la escena nueva toma la vista del editor desde el primer
    // frame (si no, se veria un frame con la camara del juego en su posicion inicial).
    if (editing) applyView(scene);
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
        } else {
            editing = false;
        }
        dragging = panning = false;
        rebuildRequested = true;
        return; // esta escena se va a reemplazar: no tiene sentido tocarla
    }
    if (!editing) return;

    // --- Teclas ------------------------------------------------------------------
    if (ctrlDown() && Input::wasPressed(Key::S)) save();
    if (!ctrlDown() && Input::wasPressed(Key::G)) {
        snap = !snap;
        showMessage(snap ? "Grilla: medio tile" : "Grilla: libre (pixel a pixel)");
    }
    if (!ctrlDown() && Input::wasPressed(Key::C)) showSolids = !showSolids;
    if (Input::wasPressed(Key::Escape)) { selectedId = 0; dragging = false; }
    if (Input::wasPressed(Key::F5)) { reload(); return; }

    Camera* cam = scene.getActiveCamera();
    TilemapRenderer* map = findMap(scene);
    float mx = Input::mouseX(), my = Input::mouseY();
    float zoom = (viewZoom > 0.0f) ? viewZoom : 1.0f;

    // --- Vista: flechas, arrastre con clic derecho/medio y rueda -----------------
    // Se divide por el zoom para que la vista se mueva lo mismo EN PANTALLA a
    // cualquier zoom (con zoom 2, 100 px de pantalla son 50 px de mundo).
    viewX += Input::axis(Key::Left, Key::Right) * PAN_SPEED * dt / zoom;
    viewY += Input::axis(Key::Up, Key::Down)    * PAN_SPEED * dt / zoom;

    if (Input::isMouseDown(MouseButton::Right) || Input::isMouseDown(MouseButton::Middle)) {
        // Arrastrar la vista: el mundo sigue al cursor, asi que la camara va al reves.
        if (panning) {
            viewX -= (mx - panLastX) / zoom;
            viewY -= (my - panLastY) / zoom;
        }
        panning = true;
        panLastX = mx;
        panLastY = my;
    } else {
        panning = false;
    }

    float wheel = Input::mouseWheel();
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
    bool overBar = my >= outH - BAR_H;

    float wx, wy;
    screenToWorld(scene, mx, my, wx, wy);

    if (Input::wasMousePressed(MouseButton::Left) && !overBar) {
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

void LevelEditor::moveSelected(Scene& scene, const TilemapRenderer* map, float wx, float wy) {
    TiledObject* o = findObject(selectedId);
    if (!o) return;

    float mx, my;
    toMap(map, wx, wy, mx, my);

    // Paso de la grilla en pixeles del mapa: medio tile (8 px con tiles de 16), que
    // incluye los centros y los bordes de las celdas. Sin grilla, pixel entero: asi el
    // archivo guarda numeros limpios.
    float stepX = 1.0f, stepY = 1.0f;
    if (snap) {
        stepX = (map && map->getTileWidth()  > 0) ? map->getTileWidth()  * 0.5f : 8.0f;
        stepY = (map && map->getTileHeight() > 0) ? map->getTileHeight() * 0.5f : 8.0f;
    }
    mx = std::round(mx / stepX) * stepX;
    my = std::round(my / stepY) * stepY;
    if (mx == o->cx && my == o->cy) return;

    o->cx = mx;
    o->cy = my;
    dirty = true;

    // La escena esta congelada: basta con mover sus GameObjects, no hace falta
    // reconstruirla. Al volver a jugar (F2) se reconstruye igual desde el modelo.
    float nx, ny;
    toWorld(map, *o, nx, ny);
    for (const auto& obj : scene.getObjects()) {
        if (obj->levelObjectId == o->id) {
            obj->transform->x = nx;
            obj->transform->y = ny;
        }
    }
}

void LevelEditor::applyView(Scene& scene) const {
    Camera* cam = scene.getActiveCamera();
    if (!cam) return;
    cam->gameObject->transform->x = viewX;
    cam->gameObject->transform->y = viewY;
    cam->zoom = viewZoom;
}

void LevelEditor::save() {
    if (!saveLevel(path, level)) {
        showMessage("ERROR al guardar (ver el log)");
        return;
    }
    dirty = false;
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
        if (loadLevel(path, fresh)) level = std::move(fresh);
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

// --- Dibujo --------------------------------------------------------------------------

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
        label(r, 8.0f, outH - 16.0f,
              dirty ? "F2: editor  (hay cambios SIN GUARDAR)" : "F2: editor", 255, 255, 255);
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

    // --- 2) Objetos del nivel: area de seleccion, punto y etiqueta -----------------
    for (const TiledObject& o : level.objects) {
        bool selected = (o.id == selectedId);

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
        if (selected) label(r, sl, st - 12.0f, name, 255, 220, 0);
        else          label(r, sl, st - 12.0f, name, 255, 255, 255);
    }

    // --- 3) Barra de estado ---------------------------------------------------------
    float barY = outH - BAR_H;
    SDL_FRect bar{ 0.0f, barY, (float)outW, BAR_H };
    SDL_SetRenderDrawColor(r, 0, 0, 0, 190);
    SDL_RenderFillRect(r, &bar);

    std::string line1 = dirty ? "EDITOR *SIN GUARDAR  " : "EDITOR  ";
    if (const TiledObject* o = findObject(selectedId)) {
        line1 += "#" + std::to_string(o->id) + " " + o->type +
                 "  x=" + num(o->cx) + " y=" + num(o->cy);
        for (const auto& kv : o->stringProps) line1 += "  " + kv.first + "=" + kv.second;
        for (const auto& kv : o->numberProps) line1 += "  " + kv.first + "=" + num((float)kv.second);
    } else {
        line1 += "(clic sobre un objeto para elegirlo)";
    }
    if (dirty) text(r, 10.0f, barY + 6.0f, line1, 2.0f, 255, 200, 80); // naranja: pendiente
    else       text(r, 10.0f, barY + 6.0f, line1, 2.0f, 255, 255, 255);

    char zoomBuf[16];
    std::snprintf(zoomBuf, sizeof(zoomBuf), "%.2f", zoom);
    std::string line2 = std::string("F2 jugar | Ctrl+S guardar | F5 recargar | G grilla: ") +
                        (snap ? "medio tile" : "libre") + " | C solidos: " +
                        (showSolids ? "si" : "no") + " | Esc deseleccionar | " +
                        "Clic der./flechas: mover vista | Rueda: zoom x" + zoomBuf;
    text(r, 10.0f, barY + 28.0f, line2, 1.0f, 190, 190, 190);

    float cwx, cwy, cmx, cmy;
    screenToWorld(scene, Input::mouseX(), Input::mouseY(), cwx, cwy);
    toMap(map, cwx, cwy, cmx, cmy);
    std::string line3 = "Raton: mapa (" + num(std::floor(cmx)) + ", " + num(std::floor(cmy)) + ")";
    if (map) {
        int col, row;
        map->worldToCell(cwx, cwy, col, row);
        line3 += "  celda (" + std::to_string(col) + ", " + std::to_string(row) + ")";
    }
    if (messageTime > 0.0f) line3 += "     >> " + message;
    text(r, 10.0f, barY + 42.0f, line3, 1.0f, 190, 190, 190);

    SDL_SetRenderDrawBlendMode(r, oldBlend);
}
