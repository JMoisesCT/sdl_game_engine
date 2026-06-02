#include "SpriteAnimator.h"
#include "GameObject.h"
#include "SpriteRenderer.h"

SpriteAnimator::SpriteAnimator(int frameWidth, int frameHeight, int sheetColumns)
    : frameW(frameWidth), frameH(frameHeight), columns(sheetColumns) {}

void SpriteAnimator::addAnimation(const std::string& name, const std::vector<int>& frames,
                                  float fps, bool loop) {
    clips[name] = Clip{ frames, fps, loop };
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
    if (clip.frames.empty() || clip.fps <= 0.0f) return;

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
    if (clip.frames.empty()) return;

    int cell = clip.frames[currentIndex];
    int col  = cell % columns;  // posicion dentro de la hoja
    int row  = cell / columns;

    sprite->setSourceRect((float)(col * frameW), (float)(row * frameH),
                          (float)frameW, (float)frameH);
}
