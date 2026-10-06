#pragma once
#include "../engine/ObjectCatalog.h"

class Scene;
struct LevelData;

// Archivo de nivel del shooter (objetos + camara; a su vez dice que mapa de Tiled usar).
// Lo expone para que main pueda abrirlo en el editor de niveles.
extern const char* SHOOTER_LEVEL_FILE;

// Lee SHOOTER_LEVEL_FILE y construye el nivel.
void buildShooter(Scene& scene);

// Construye el nivel a partir de datos YA cargados (la fabrica que usa el editor).
void buildShooterLevel(Scene& scene, const LevelData& level);

// Los objetos del shooter: cada type con la funcion que lo arma y sus propiedades.
ObjectCatalog shooterObjects();
