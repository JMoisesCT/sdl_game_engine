#include "SpriteAnimator.h"
#include "GameObject.h"
#include "Scene.h"
#include "SpriteRenderer.h"

#include <SDL3/SDL.h>

SpriteAnimator::SpriteAnimator(int frameWidth, int frameHeight, int sheetColumns)
    : frameW(frameWidth), frameH(frameHeight), columns(sheetColumns) {}

void SpriteAnimator::addAnimation(const std::string& name, const std::vector<int>& frames,
                                  float fps, bool loop) {
    // Modo hoja unica: sin textura propia (usa la del SpriteRenderer) y con el
    // tamano/columnas por defecto del animator.
    Clip clip;
    clip.texture = nullptr;
    clip.frameW  = frameW;
    clip.frameH  = frameH;
    clip.columns = columns;
    clip.frames  = frames;
    clip.fps     = fps;
    clip.loop    = loop;
    clips[name]  = std::move(clip);
}

void SpriteAnimator::addStripAnimation(const std::string& name, const std::string& path,
                                       int fw, int fh, float fps, bool loop) {
    // Una tira es el caso particular de una fila (la fila 0) de un sheet.
    addRowAnimation(name, path, fw, fh, 0, fps, loop);
}

void SpriteAnimator::addRowAnimation(const std::string& name, const std::string& path,
                                     int fw, int fh, int row, float fps, bool loop) {
    // Atajo: una fila es una linea con eje Row.
    addLineAnimation(name, path, fw, fh, row, StripAxis::Row, fps, loop);
}

void SpriteAnimator::addLineAnimation(const std::string& name, const std::string& path,
                                      int fw, int fh, int index, StripAxis axis,
                                      float fps, bool loop) {
    // El AssetManager cachea por ruta, asi que clips con la misma ruta comparten
    // textura (p.ej. varias lineas del mismo sheet) y con rutas distintas no.
    SDL_Texture* tex = gameObject->scene->getAssets().loadTexture(path);

    int cols = 1, rows = 1; // celdas por eje de TODA la hoja
    if (tex) {
        float w = 0.0f, h = 0.0f;
        SDL_GetTextureSize(tex, &w, &h);
        if (fw > 0) cols = (int)(w / fw);
        if (fh > 0) rows = (int)(h / fh);
        if (cols < 1) cols = 1;
        if (rows < 1) rows = 1;
    }

    Clip clip;
    clip.texture = tex;
    clip.frameW  = fw;
    clip.frameH  = fh;
    clip.columns = cols; // columnas de TODA la hoja, para mapear celda -> col/fila

    // La numeracion de celdas es row-major: cell = fila * cols + col, y luego
    // applyFrame deduce col = cell % cols, fila = cell / cols.
    if (axis == StripAxis::Row) {
        // Fila fija 'index'; los frames avanzan por las columnas.
        clip.frames.reserve(cols);
        for (int col = 0; col < cols; ++col) clip.frames.push_back(index * cols + col);
    } else {
        // Columna fija 'index'; los frames avanzan por las filas.
        clip.frames.reserve(rows);
        for (int row = 0; row < rows; ++row) clip.frames.push_back(row * cols + index);
    }
    clip.fps  = fps;
    clip.loop = loop;
    clips[name] = std::move(clip);
}

void SpriteAnimator::play(const std::string& name) {
    if (current == name) return;                  // ya esta sonando
    if (clips.find(name) == clips.end()) return;  // no existe
    current = name;
    currentIndex = 0;
    timer = 0.0f;
    applyFrame();
}

void SpriteAnimator::update(float dt) {
    if (!sprite) sprite = gameObject->getComponent<SpriteRenderer>(); // perezoso
    if (!sprite || current.empty()) return;

    Clip& clip = clips[current];
    if (clip.frames.empty() || clip.fps <= 0.0f) { applyFrame(); return; }

    timer += dt;
    float frameTime = 1.0f / clip.fps;

    // Avanza tantos cuadros como corresponda al tiempo acumulado.
    while (timer >= frameTime) {
        timer -= frameTime;
        currentIndex++;
        if (currentIndex >= (int)clip.frames.size()) {
            if (clip.loop) {
                currentIndex = 0;
            } else {
                currentIndex = (int)clip.frames.size() - 1; // se queda en el ultimo
                break;
            }
        }
    }
    applyFrame();
}

void SpriteAnimator::applyFrame() {
    if (!sprite || current.empty()) return;
    Clip& clip = clips[current];
    if (clip.frames.empty() || clip.columns <= 0) return;

    // Si el clip trae su propia textura (modo una tira por archivo), asegurarse de
    // que el SpriteRenderer la este dibujando. Solo se reasigna al cambiar de clip.
    if (clip.texture && clip.texture != appliedTexture) {
        sprite->setTexture(clip.texture);
        appliedTexture = clip.texture;
    }

    int cell = clip.frames[currentIndex];
    int col  = cell % clip.columns;  // posicion dentro de la hoja
    int row  = cell / clip.columns;

    sprite->setSourceRect((float)(col * clip.frameW), (float)(row * clip.frameH),
                          (float)clip.frameW, (float)clip.frameH);
}
