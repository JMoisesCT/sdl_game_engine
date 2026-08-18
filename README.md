# sdl_game_engine — Motor 2D educativo sobre SDL3

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
  embebido (vía **nlohmann/json**, que viene incluida en el repo en `engine/third_party/`).
  Expone consultas del mapa cargado (`getMapWidth/Height`, `getTileWidth/Height`,
  `getWorldWidth/Height`) útiles, p. ej., para centrar la cámara según el ancho del mapa.
- **Capa de objetos de Tiled**: `TiledObjectLayer` lee las capas *objectgroup* como **datos
  planos** (`TiledObject`: `type`, centro ya corregido, tamaño y propiedades personalizadas).
  El motor no sabe qué significa cada `type`; la semántica (jugador, enemigo, power-up…) la
  pone el juego con una fábrica sobre `type`.
- **Texto y HUD**: `TextRenderer` (vía **SDL3_ttf**) dibuja una cadena con una fuente cacheada;
  modo `screenSpace` para HUD fijo (ignora la cámara) o texto en el mundo, con *dirty flag*
  para regenerar la textura solo cuando el texto cambia. Usa `TTF_RenderText_Solid` (sin
  antialiasing) para que la fuente pixel quede nítida.
- **Debugger conmutable**: dibujo de colliders, zona muerta y primitivas (se prende/apaga en
  caliente).
- **`AssetManager`**: dueño de las **texturas** y de las **fuentes** (`loadFont(ruta, tamaño)`,
  cacheadas por pareja ruta/tamaño); los renderers solo las piden prestadas.

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
sdl_game_engine/
├── engine/                 # El MOTOR (código genérico, no conoce ningún juego)
│   ├── Component.h         #   base de todos los componentes
│   ├── GameObject.h        #   objeto contenedor de componentes
│   ├── Transform.h         #   posición/escala/rotación (centro del objeto)
│   ├── Scene.{h,cpp}       #   contenedor de objetos + fase de física + render
│   ├── AssetManager.{h,cpp}#   carga y posee texturas
│   ├── SpriteRenderer.*    #   dibujo de sprites
│   ├── SpriteAnimator.*    #   animación por spritesheet
│   ├── TextRenderer.*      #   texto/HUD con fuentes (SDL3_ttf)
│   ├── Camera.*  FollowCamera.*
│   ├── RigidBody2D.h  BoxCollider.*   # física AABB
│   ├── TilemapRenderer.*   #   grilla de tiles (código / archivo / Tiled JSON)
│   ├── TiledObjectLayer.*  #   lee la capa de objetos de Tiled como datos planos
│   ├── Lifetime.h  Spawner.h
│   ├── Debugger.*          #   ayudas visuales de depuración
│   └── third_party/        #   librerías de terceros incluidas (vendored)
│       └── nlohmann/json.hpp  # nlohmann/json single-include (MIT), para Tiled JSON
├── game/                   # Lógica de los EJEMPLOS (lado del juego, no del motor)
│   ├── Platformer.{h,cpp}  #   ejemplo 1
│   ├── TopDown.{h,cpp}     #   ejemplo 2
│   └── Shooter.{h,cpp}     #   ejemplo 3
├── main.cpp                # Bucle de SDL + selector de ejemplos (teclas 1/2/3)
├── assets/                 # Recursos junto al ejecutable (imágenes, mapas)
│   ├── pixel_adventure/    #   sprites del pack Pixel Adventure (platformer)
│   ├── ninja_adventure/    #   sprites/tileset (top-down) y fuente del HUD (Ui/Font)
│   ├── kenney_pixelshmup/  #   naves y tileset del pack Kenney Pixel Shmup (shooter)
│   └── maps/               #   niveles de Tiled (.json/.tmx) y mapa propio (.map)
└── sdl_game_engine.vcxproj # Proyecto de Visual Studio (un solo ejecutable)
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
- **SDL3 y sus librerías satélite instaladas manualmente** (SDL3 **no** viene de vcpkg). Estas
  son las versiones contra las que se compila el curso — **usa exactamente estas** para que los
  problemas que reportes sean reproducibles:

  | Librería | Versión | Ruta esperada | Para qué |
  |---|---|---|---|
  | **SDL3** | **3.4.14** | `D:\SDL3` | ventana, renderer, input, tiempo |
  | **SDL3_image** | **3.4.4** | `D:\SDL3_image` | cargar PNG de sprites y tilesets |
  | **SDL3_ttf** | **3.2.2** | `D:\SDL3_ttf` | texto y HUD (`TextRenderer`) |
  | **SDL3_mixer** | **3.2.4** | `D:\SDL3_mixer` | audio (aún sin usar, ver nota) |

  Cada una se instala igual: descomprimir el paquete de desarrollo para VC en su carpeta de
  `D:\`, de modo que queden `<carpeta>\include` y `<carpeta>\lib\x64`. El `.vcxproj`
  (plataforma **x64**) ya apunta ahí:
  - Includes: `D:\SDL3\include`, `D:\SDL3_image\include`, `D:\SDL3_mixer\include`,
    `D:\SDL3_ttf\include`
  - Libs: `D:\SDL3\lib\x64`, `D:\SDL3_image\lib\x64`, `D:\SDL3_ttf\lib\x64`,
    `D:\SDL3_mixer\lib\x64`
  - DLLs copiadas por el post-build: `SDL3.dll`, `SDL3_image.dll`, `SDL3_ttf.dll`,
    `SDL3_mixer.dll`

  > **Sobre SDL3_mixer:** ya está **instalado y enlazado** en el proyecto (lib + copia de DLL),
  > pero el motor **todavía no tiene un componente `AudioSource`**: la dependencia está lista
  > de antemano para la sesión de audio. Enlazarla sin usarla no hace daño.
  >
  > Cuando se implemente el audio, ten en cuenta que SDL3_mixer reproduce **WAV** de fábrica,
  > mientras que los formatos extra (OGG Vorbis, Opus, WavPack, módulos, GME) se apoyan en las
  > DLLs sueltas de `D:\SDL3_mixer\lib\x64\optional\`, que el post-build **no** copia hoy. Si el
  > curso usa música en OGG habrá que agregarlas al `PostBuildEvent`.
  >
  > Si instalaste SDL3 en otra ruta, ajusta `AdditionalIncludeDirectories`,
  > `AdditionalLibraryDirectories` y el `PostBuildEvent` del `.vcxproj`.
- **nlohmann/json** (para leer mapas de Tiled en JSON) **ya viene incluida en el repo**, en
  `engine/third_party/nlohmann/json.hpp` (single-include, header-only). **No requiere instalar
  nada ni vcpkg**: al clonar el repo la tienes lista. El proyecto añade `engine/third_party`
  como *include directory*, así que en el código basta `#include <nlohmann/json.hpp>`.

### Pasos

1. Instala SDL3, SDL3_image, SDL3_ttf y SDL3_mixer en `D:\SDL3`, `D:\SDL3_image`,
   `D:\SDL3_ttf` y `D:\SDL3_mixer` (o ajusta las rutas del proyecto), en las versiones de la
   tabla de arriba.
2. Abre `sdl_game_engine.vcxproj` en Visual Studio 2026.
3. Selecciona la configuración **x64** (las rutas de SDL y la copia de DLLs/`assets` están
   cableadas para **x64 Debug**).
4. Compila y ejecuta (F5). El evento post-build copia automáticamente `SDL3.dll`,
   `SDL3_image.dll`, `SDL3_ttf.dll`, `SDL3_mixer.dll` y la carpeta `assets/` junto al
   ejecutable.

---

## Controles y ejemplos

Hay **tres ejemplos** que se cambian en caliente con las teclas numéricas:

| Tecla | Ejemplo |
|-------|---------|
| `1`   | Platformer (lateral con gravedad y salto; personaje de **Pixel Adventure** animado por estado) |
| `2`   | Top-down (4 direcciones; personaje de **Ninja Adventure** con animación direccional y mundo desde Tiled) |
| `3`   | Shooter (shmup vertical con scroll de cámara; naves del pack **Kenney Pixel Shmup**, enemigos por *streaming* desde la capa de objetos de Tiled y **HUD de puntaje**) |
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
- Además de la **capa de tiles**, se leen las **capas de objetos** (*objectgroup*):
  `TiledObjectLayer` las entrega como datos planos (`TiledObject`) y el juego decide qué crear
  según el `type` (lo usa el shooter para el jugador, enemigos, power-ups y zonas). El motor no
  interpreta el `type`: esa semántica vive en `game/`.

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
- `TilemapRenderer` (código / archivo propio / Tiled JSON, con consultas de tamaño) y
  `TiledObjectLayer` (capa de objetos como datos planos); `Debugger` conmutable.
- `TextRenderer` (SDL3_ttf) y HUD de puntaje en el shooter; `AssetManager` cachea fuentes.
- Tres ejemplos: platformer, top-down y shooter (`1`/`2`/`3`).

**Pendiente (sin orden fijo):**

- `Health` / `Damageable` (vida y condición de derrota).
- Clase `Input` consultable (`isKeyDown` / `wasPressed`).
- UI básica más allá del HUD (diálogos, menús); gestor de escenas (menú → juego → game over).
- Sistema de tags o capas (reemplazar el filtro por `name`).
- `AudioSource` (efectos y música). **SDL3_mixer 3.2.4 ya está instalado y enlazado**; falta
  el componente del motor que lo use.
- Mejoras de física (one-way platforms, broad-phase, fricción/rebote); handles seguros;
  parenting de `Transform`; partículas.
- En Tiled siguen sin soportarse los **tiles volteados** (se ignoran los bits de flip del GID).

**Limitaciones conocidas:** colisiones O(n²) (bien para decenas de objetos, no miles); la
resolución por pares puede temblar con colliders apilados; no hay desregistro automático de
punteros a objetos destruidos.

---

## Créditos y licencias de terceros

### Librerías

- **[nlohmann/json](https://github.com/nlohmann/json)** de **Niels Lohmann** — librería
  *JSON for Modern C++* usada para leer los mapas de Tiled. Licencia **MIT**. Se incluye
  **vendorizada** en el repo (`engine/third_party/nlohmann/json.hpp`, single-include), con su
  cabecera de licencia MIT intacta; no requiere instalación.

### Assets

- **[Pixel Adventure](https://pixelfrog-assets.itch.io/pixel-adventure-1)** de **Pixel Frog**
  (itch.io) — sprites del ejemplo *platformer*. Respeta su licencia si reutilizas o
  redistribuyes los assets.
- **[Ninja Adventure Asset Pack](https://pixel-boy.itch.io/ninja-adventure-asset-pack)** de
  **Pixel-boy y AAA** — sprites del ninja y tileset del ejemplo *top-down*, y la fuente
  (`Ui/Font/NormalFont.ttf`) usada en el HUD del shooter. Publicado bajo **CC0** (dominio
  público; atribución no obligatoria pero apreciada).
- **[Pixel Shmup](https://kenney.nl/assets/pixel-shmup)** de **Kenney** — naves y tileset del
  ejemplo *shooter*. Publicado bajo **CC0** (dominio público). Ver `assets/kenney_pixelshmup/License.txt`.

Se usan con fines educativos.
