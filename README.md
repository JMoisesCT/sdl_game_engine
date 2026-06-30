# sdl_upc_engine — Motor 2D educativo sobre SDL3

Motor de videojuegos **2D ligero, estilo componentes** (similar a Unity), construido sobre
**SDL3**. Su propósito es educativo: que estudiantes que ya conocen SDL3 básico (ventana,
renderer, eventos, delta time) creen juegos **agregando componentes a objetos**, sin pelear
con la API cruda de SDL.

El motor no toma el control: tú mantienes tu propio bucle `main` de SDL y en cada frame
"bombeas" la escena (`scene->update(dt)` y `scene->render()`). El motor te entrega el sistema
de objetos/componentes que se actualiza y dibuja.

Este repositorio es la **base de un curso** de desarrollo de videojuegos. Cada sesión suma una
capacidad nueva al motor.

---

## Características

Lo que el motor ya hace hoy:

- **Sistema de componentes** estilo Unity: `GameObject` + `Transform` + componentes con ciclo
  de vida `awake` / `start` / `update` / `render` / `onCollision`. El `Transform` marca el
  **centro** del objeto.
- **Sprites**: `SpriteRenderer` con recortes, flip horizontal y anclaje al centro.
- **Animación por spritesheet**: `SpriteAnimator` con varios formatos: celdas numeradas de
  un solo sheet, una tira (un archivo) por animación, o una fila/columna de un sheet en
  grilla (para personajes direccionales).
- **Cámara**: `Camera` + `FollowCamera` con zona muerta y suavizado.
- **Física AABB**: `RigidBody2D` + `BoxCollider` con gravedad, colisiones, triggers y
  detección de "grounded".
- **Ciclo de vida**: `destroy` diferido, `Lifetime` (autodestrucción por tiempo) y `Spawner`.
- **Tilemap**: `TilemapRenderer` (grilla + tileset) con colisión de tiles, cargable desde
  **código**, desde un **archivo de texto propio** (`.map`) o desde **Tiled JSON** con tileset
  embebido (vía `nlohmann-json`).
- **Debugger conmutable**: dibujo de colliders, zona muerta y primitivas (se prende/apaga en
  caliente).
- **`AssetManager`**: dueño de las texturas; los renderers solo las piden prestadas.

---

## Arquitectura

Idea general (en pocas líneas):

- **Motor por capas**, de lo más cercano al alumno a lo más cercano a SDL:
  juego/ejemplos (`game/`, `main.cpp`) → gameplay (`GameObject` + componentes) →
  subsistemas (sprites, cámara, física, assets) → núcleo (`Scene`) → SDL3 (aislado).
- **Modelo de componentes estilo Unity**: un `GameObject` no hereda comportamiento, lo
  **compone** con `addComponent<T>()` / `getComponent<T>()`. Todo objeto nace con un
  `Transform` (que marca su **centro**). Los componentes siguen el ciclo de vida
  `awake` / `start` / `update` / `render` / `onCollision`.
- **El alumno mantiene el control**: el motor no invierte el bucle. Tú escribes tu `main`
  de SDL y en cada frame "bombeas" la escena (`scene->update(dt)` y `scene->render()`).
  `Scene::update` actualiza objetos, resuelve la física AABB y barre los marcados con
  `destroy()`.
- **SDL queda escondido**: `<SDL3/SDL.h>` vive en los `.cpp` (y muy pocos headers), nunca
  en los headers de composición; donde hace falta un tipo de SDL se usa forward declaration.

---

## Estructura del proyecto

```
sdl_upc_engine/
├── engine/                 # El MOTOR (código genérico, no conoce ningún juego)
│   ├── Component.h         #   base de todos los componentes
│   ├── GameObject.h        #   objeto contenedor de componentes
│   ├── Transform.h         #   posición/escala/rotación (centro del objeto)
│   ├── Scene.{h,cpp}       #   contenedor de objetos + fase de física + render
│   ├── AssetManager.{h,cpp}#   carga y posee texturas
│   ├── SpriteRenderer.*    #   dibujo de sprites
│   ├── SpriteAnimator.*    #   animación por spritesheet
│   ├── Camera.*  FollowCamera.*
│   ├── RigidBody2D.h  BoxCollider.*   # física AABB
│   ├── TilemapRenderer.*   #   grilla de tiles (código / archivo / Tiled JSON)
│   ├── Lifetime.h  Spawner.h
│   └── Debugger.*          #   ayudas visuales de depuración
├── game/                   # Lógica de los EJEMPLOS (lado del juego, no del motor)
│   ├── Platformer.{h,cpp}  #   ejemplo 1
│   ├── TopDown.{h,cpp}     #   ejemplo 2
│   └── Shooter.{h,cpp}     #   ejemplo 3
├── main.cpp                # Bucle de SDL + selector de ejemplos (teclas 1/2/3)
├── assets/                 # Recursos junto al ejecutable (imágenes, mapas)
│   ├── pixel_adventure/    #   sprites del pack Pixel Adventure (platformer)
│   ├── ninja_adventure/    #   sprites y tileset del pack Ninja Adventure (top-down)
│   └── maps/               #   niveles de Tiled (.json/.tmx) y mapa propio (.map)
├── sdl_upc_engine.vcxproj  # Proyecto de Visual Studio (un solo ejecutable)
└── vcpkg.json              # Manifiesto vcpkg (solo nlohmann-json)
```

`engine/` es **genérico**: nunca contiene nombres de un juego concreto (nada de "Player" o
"Bala"). Toda la lógica de gameplay vive en `game/` y `main.cpp`.

---

## Cómo compilar y ejecutar

El proyecto se compila con **Visual Studio 2026** (un único proyecto que produce un
ejecutable de consola). No hay solución `.sln` ni `CMakeLists.txt` en el repo: se abre el
`.vcxproj` directamente.

### Requisitos

- **Visual Studio 2026** con el toolset de C++ (PlatformToolset `v145`), C++17.
- **SDL3** y **SDL3_image** instalados **manualmente**. El proyecto los espera en estas rutas
  (configuradas en `sdl_upc_engine.vcxproj`, plataforma **x64**):
  - Includes: `D:\SDL3\include`, `D:\SDL3_image\include`
  - Libs: `D:\SDL3\lib\x64`, `D:\SDL3_image\lib\x64`
  - DLLs: `D:\SDL3\lib\x64\SDL3.dll`, `D:\SDL3_image\lib\x64\SDL3_image.dll`

  > Si instalaste SDL3 en otra ruta, ajusta `AdditionalIncludeDirectories`,
  > `AdditionalLibraryDirectories` y el `PostBuildEvent` del `.vcxproj`.
- **vcpkg en modo manifiesto** para `nlohmann-json` (la única dependencia del manifiesto;
  SDL3 **no** viene de vcpkg). Visual Studio la restaura sola al abrir el proyecto si tienes
  la integración de vcpkg activa.

### Pasos

1. Instala SDL3 y SDL3_image en `D:\SDL3` y `D:\SDL3_image` (o ajusta las rutas del proyecto).
2. Abre `sdl_upc_engine.vcxproj` en Visual Studio 2026.
3. Selecciona la configuración **x64** (las rutas de SDL y la copia de DLLs/`assets` están
   cableadas para **x64 Debug**).
4. Compila y ejecuta (F5). El evento post-build copia automáticamente `SDL3.dll`,
   `SDL3_image.dll` y la carpeta `assets/` junto al ejecutable.

---

## Controles y ejemplos

Hay **tres ejemplos** que se cambian en caliente con las teclas numéricas:

| Tecla | Ejemplo |
|-------|---------|
| `1`   | Platformer (lateral con gravedad y salto; personaje de **Pixel Adventure** animado por estado) |
| `2`   | Top-down (4 direcciones; personaje de **Ninja Adventure** con animación direccional y mundo desde Tiled) |
| `3`   | Shooter (disparo) |
| `F1`  | Prende/apaga el dibujo de debug (colliders, etc.) |

Controles dentro de cada ejemplo:

- **Platformer (`1`)**: `←`/`→` mueven, `Espacio` salta.
- **Top-down (`2`)**: `←`/`→`/`↑`/`↓` mueven en las 4 direcciones.
- **Shooter (`3`)**: `←`/`→` mueven, `Espacio` dispara.

---

## Cómo editar un mapa con Tiled (guía para alumnos)

El motor lee mapas exportados desde **[Tiled](https://www.mapeditor.org/)** en formato
**JSON**. El ejemplo de plataformas carga `assets/maps/platformer_level1.json` (ver
`game/Platformer.cpp`).

### 1. Instala Tiled

Descárgalo gratis desde [mapeditor.org](https://www.mapeditor.org/).

### 2. Abre el mapa incluido o crea uno nuevo

- **Abrir el incluido**: `assets/maps/platformer_level1.tmx` (también está el proyecto de
  Tiled `assets/maps/platformer_level1.tiled-project`).
- **Crear uno nuevo**, con estas opciones (las que el motor espera):
  - Orientación: **Orthogonal**
  - Formato de capa de tiles: **CSV** (¡no Base64!)
  - Tamaño de tile: **16 × 16**
  - Tileset: **embebido en el mapa** (no como archivo externo)

### 3. Marca los tiles sólidos

La física "viaja" dentro del mapa: el motor crea un collider por cada tile marcado como
sólido. Para marcar un tile:

1. Selecciona el tileset y luego el tile en el panel de tilesets.
2. En **Propiedades personalizadas** (Custom Properties), agrega una propiedad **booleana**
   llamada exactamente **`solid`** y ponla en **`true`**.
3. Repite con todos los tiles que deban colisionar (suelo, paredes, plataformas).

El parser busca en el tileset embebido cada tile con la propiedad `solid == true` y lo
registra como sólido.

### 4. Exporta a JSON en la ruta que el juego espera

Exporta el mapa como **JSON** sobre la ruta que carga el código:

```
assets/maps/platformer_level1.json
```

(Es la ruta de `buildPlatformer` en `game/Platformer.cpp`. Si usas otro nombre, cambia esa
ruta en el código.)

### ⚠️ Advertencias importantes

- **Mantén la estructura de carpetas** al clonar el repo. La imagen del tileset se referencia
  con ruta **relativa** al `.json` (`../pixel_adventure/Terrain/Terrain (16x16).png`); si
  mueves carpetas, el tileset no cargará.
- Usa **CSV**, no **Base64 ni compresión**: el parser lee la capa de tiles como lista de
  números.
- **No uses el volteo/rotación de tiles** de Tiled: el parser aún **ignora** esos bits de
  flip (enmascara los 3 bits altos del GID).
- Hoy se soporta la **capa de tiles**; la capa de objetos de Tiled todavía no se interpreta.

---

## Roadmap

Proyecto en **desarrollo activo**: el motor crece sesión a sesión a lo largo del curso.

**Hecho:**

- Núcleo: `Component`, `GameObject`, `Transform`, `Scene`, `AssetManager`.
- Render: `SpriteRenderer` (recortes, flip, anclaje al centro), `SpriteAnimator`.
- Cámara: `Camera` (zoom, mundo→pantalla) y `FollowCamera` (zona muerta + suavizado).
- Física: `RigidBody2D` (gravedad, `grounded`), `BoxCollider` (AABB, triggers) y fase de
  colisiones en `Scene`.
- Ciclo de vida: `destroy` diferido, `Lifetime`, `Spawner`.
- `TilemapRenderer` (código / archivo propio / Tiled JSON) y `Debugger` conmutable.
- Tres ejemplos: platformer, top-down y shooter (`1`/`2`/`3`).

**Pendiente (sin orden fijo):**

- `Health` / `Damageable` (vida y condición de derrota).
- Clase `Input` consultable (`isKeyDown` / `wasPressed`).
- `TextRenderer` + UI básica (SDL3_ttf): HUD, puntaje, diálogos.
- Sistema de tags o capas (reemplazar el filtro por `name`).
- Audio (SDL3_mixer); mejoras de física (one-way platforms, broad-phase, fricción/rebote);
  handles seguros; parenting de `Transform`; partículas.

**Limitaciones conocidas:** colisiones O(n²) (bien para decenas de objetos, no miles); la
resolución por pares puede temblar con colliders apilados; no hay desregistro automático de
punteros a objetos destruidos.

---

## Créditos de assets

- **[Pixel Adventure](https://pixelfrog-assets.itch.io/pixel-adventure-1)** de **Pixel Frog**
  (itch.io) — sprites del ejemplo *platformer*. Respeta su licencia si reutilizas o
  redistribuyes los assets.
- **[Ninja Adventure Asset Pack](https://pixel-boy.itch.io/ninja-adventure-asset-pack)** de
  **Pixel-boy y AAA** — sprites del ninja y tileset del ejemplo *top-down*. Publicado bajo
  **CC0** (dominio público; atribución no obligatoria pero apreciada).

Se usan con fines educativos.
