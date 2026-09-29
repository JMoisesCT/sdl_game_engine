#pragma once
#include <string>

class Scene;
class FollowCamera;

// Herramientas de depuracion OPCIONALES. Ningun componente del motor depende de
// esto: se incluye donde quieras y se quita sin tocar nada mas. Si Debug esta
// apagado, todas las funciones retornan sin dibujar.
//
// Uso tipico (en el bucle, DESPUES de scene.render() y antes de RenderPresent):
//     Debug::drawColliders(scene);
//     Debug::drawDeadZone(scene, follow);
// Y para prender/apagar en caliente (p.ej. con una tecla):
//     Debug::toggle();

namespace Debug {
    void setEnabled(bool on);
    void toggle();
    bool isEnabled();

    // Todas reciben la escena para tomar el renderer y la camara activa,
    // asi el dibujo se hace en coordenadas de MUNDO (igual que los sprites).
    void drawColliders(Scene& scene);                    // AABB de cada collider
    void drawDeadZone(Scene& scene, FollowCamera* cam);  // zona muerta de la camara
    void drawRect(Scene& scene, float x, float y, float w, float h);
    void drawLine(Scene& scene, float x1, float y1, float x2, float y2);
    void drawPoint(Scene& scene, float x, float y);
    // Texto en coordenadas de MUNDO (esquina superior izquierda en x,y), con la fuente
    // de depuracion que trae SDL3 (8x8 px, solo ASCII: sin tildes). No necesita cargar
    // ninguna fuente. Util para ver valores encima de un objeto: velocidad, estado...
    void drawText(Scene& scene, float x, float y, const std::string& text);
}
