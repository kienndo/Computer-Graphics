# CG\_hospital

CG_hospital is a Vulkan app, displaying an interactive hospital scene. 

---

## ✨ Overview

`CG_hospital` renders a scene from `assets/models/scene.json` and lets you:

* Switch between **Camera** and **Edit** modes.
* **Select** scene instances by ID and **manipulate** their transforms (translate/rotate/scale).
* **Hide/show** individual objects (non-destructive "delete").
* Toggle an on-screen keyboard overlay and a list of visible items.

The app sets up two pipelines:

* **Mesh** (3D geometry, per-instance local uniforms)
* **Overlay** (screen-space quad with `assets/models/Keyboard.png` + on-screen text)

Lighting uses a simple Blinn/Lambert fragment shader with eight point lights and ambient.

---

## 🗂 Project Structure (from this file)

* **Pipelines & layouts**

    * `DSLglobal` (set=0): global UBO (`GlobalUBO`)
    * `DSLmesh`   (set=1): local UBO (`LocalUBO`) + 2 textures
    * `DSLoverlay`: overlay UBO (`OverlayUniformBuffer`) + 1 texture (`Keyboard.png`)
* **Pipelines**

    * `PMesh`: `shaders/Mesh.vert.spv` + `shaders/Lambert-Blinn.frag.spv`
    * `POverlay`: `shaders/Overlay.vert.spv` + `shaders/Overlay.frag.spv`
* **Geometry**

    * `Scene SC` loads **`assets/models/scene.json`** (Technique: `Mesh`, Vertex layout: `VertexSimp`)
    * Overlay quad built in code (`MKey`) using `VertexOverlay`
* **Text overlay**

    * `TextMaker txt` for HUD strings (mode, current selection, list/hints)

---

## 📦 Assets & Shaders

* **Scene JSON**: `assets/models/scene.json`
* **Overlay texture**: `assets/models/Keyboard.png`
* **Shaders**:

    * Mesh: `shaders/Mesh.vert.spv`, `shaders/Lambert-Blinn.frag.spv`
    * Overlay: `shaders/Overlay.vert.spv`, `shaders/Overlay.frag.spv`

> Make sure these files exist at the specified paths, or update the paths in the code.

---

## 🧱 Data Layouts

### VertexSimp (mesh)

```cpp
struct VertexSimp {
  glm::vec3 pos;   // LOCATION 0
  glm::vec3 norm;  // LOCATION 1
  glm::vec2 UV;    // LOCATION 2
};
```

### LocalUBO (per-instance)

```cpp
struct LocalUBO {
  float     gamma;          // default 120.0f
  glm::vec3 specularColor;  // (1.0, 0.95, 0.9)
  glm::mat4 mvpMat;
  glm::mat4 mMat;           // world
  glm::mat4 nMat;           // inverse-transpose of mMat
  glm::vec4 highlight;      // x:1 if selected, w:1 visible / 0 hidden
};
```

### GlobalUBO (per-frame)

```cpp
struct GlobalUBO {
  glm::vec4 lightPos[4];
  glm::vec4 lightColor;       // (1, 0.95, 0.9, 1)
  float     decayFactor;      // 1.0
  float     g;                // 20.0
  float     numLights;        // 4
  glm::vec3 ambientLightColor;// (0.1, 0.095, 0.09)
  glm::vec3 eyePos;           // camera position
};
```

### OverlayUniformBuffer

```cpp
struct OverlayUniformBuffer { float visible; };
```

---

## 🧭 Controls & Modes

The app reads input via `Starter.hpp` helpers and raw GLFW keys. (Exact mouse/gamepad mapping for `getSixAxis(...)` depends on `Starter.hpp`.)

### Global

* **ESC** — Quit
* **Q** — Toggle **Camera** ↔ **Edit** mode (HUD shows current mode)
* **P** — Toggle the keyboard overlay image on/off

### Selection & Visibility

* **TAB** — Cycle to the next **visible** selectable object
* **L** — While held (and overlay hidden), shows list of **visible** object IDs
* **D** — In **Edit** mode: toggle visibility (hide/show) of the **currently selected** object

> Non-selectable IDs: `floor`, `wall`, `door`, `window` are *skipped* by the selector.

### Camera Mode (default)

* Movement uses the app’s six-axis input (`getSixAxis`) mapped to **Right/Up/Forward** deltas.
* Typical bindings (depends on `Starter.hpp`): move/strafe/fly + yaw/pitch with mouse/gamepad.

### Edit Mode (selected object transform)

* **Translate (world space, pre-multiply)**

    * **←/→**: X−/X+
    * **↑/↓**: Z−/Z+
    * **PageUp/PageDown**: Y+/Y−
* **Rotate (local Y)**

    * **T**: +Y rotation
    * **G**: −Y rotation
* **Scale (uniform, local, post-multiply)**

    * **W**: Grow
    * **S**: Shrink

> Holding the input system’s **“fire/run”** modifier increases move/rotate/scale rates (see `Starter.hpp`).

---

## 🧩 Scene JSON expectations (selector)

Selection UI parses `assets/models/scene.json` to build the list of selectable IDs:

* Looks at `instances[0].elements`.
* Each element may have an `"id"` string. If missing, it is auto-labeled as `instance_<index>`.
* Elements with IDs `floor`, `wall`, `door`, `window` are considered **unselectable**.

**Minimal example:**

```json
{
  "instances": [
    {
      "elements": [
        { "id": "bed" },
        { "id": "floor" },
        { "id": "chair" },
        { "id": "window" }
      ]
    }
  ]
}
```

In this example, the selector will cycle through `bed`, `chair` (skipping `floor` and `window`).

> Rendering, materials, textures, and descriptor binding for each instance are driven by `Scene` (via `SC.TI[0]`) and the `Mesh` technique. Ensure your scene JSON matches the expectations of your `Scene`/MCGC loader.

---

## 🔦 Lighting (defaults)

Four point lights in world space:

```
( 70, 35, -30)
( 10, 35, -30)
(-60, 35, -20)
( 10, 35,  20)
```

* Light color: `(1.0, 0.95, 0.9, 1.0)`
* Ambient: `(0.10, 0.095, 0.09)`
* `decayFactor = 1.0`, `g = 20.0`, `numLights = 4`

---

## 🛠 Build & Run

This project assumes an environment similar to the MCGC course stack.

### Requirements

* **Vulkan SDK** (headers, loader, validation layers)
* **GLFW**
* **GLM**
* **nlohmann::json** (`#include <json.hpp>`)
* Toolchain capable of C++17 or later

> The project also depends on `BaseProject` and related MCGC modules (e.g., `Scene`, `TextMaker`). Make sure your include paths and libraries for these modules are configured.

### Typical CMake outline (sketch)

```cmake
cmake_minimum_required(VERSION 3.20)
project(CG_hospital CXX)
set(CMAKE_CXX_STANDARD 17)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
# Find GLM and nlohmann_json the way your environment provides them

add_executable(CG_hospital
  src/main.cpp
  # modules/*.cpp ...
)

target_include_directories(CG_hospital PRIVATE
  ${Vulkan_INCLUDE_DIRS}
  # path/to/modules
)

target_link_libraries(CG_hospital PRIVATE
  ${Vulkan_LIBRARIES}
  glfw
  # glm, nlohmann_json, etc.
)
```

### Run

Ensure working directory has access to:

```
assets/models/scene.json
assets/models/Keyboard.png
shaders/Mesh.vert.spv
shaders/Lambert-Blinn.frag.spv
shaders/Overlay.vert.spv
shaders/Overlay.frag.spv
```

Then launch the binary. The initial window is `1280×720`, resizable; the aspect ratio is handled in the projection.

---

## 🧪 Runtime Notes

* Per-frame, the app updates all instances’ local UBOs with `mMat`, `nMat`, `mvpMat`, selection highlight, and visibility.
* Visibility is purely a shader flag (`highlight.w`), so **hidden** objects are still processed (just not drawn visually).
* HUD text is redrawn only when needed (mode changes, selection changes, list toggle), then `txt.updateCommandBuffer()` is called to refresh the overlay command buffer.

---

## 🐞 Troubleshooting

* **Black screen**: verify shader paths and `Scene` JSON; ensure the clear color isn’t the only thing rendering.
* **Nothing selectable**: check `instances[0].elements` in JSON and that IDs aren’t all denied (`floor/wall/door/window`).
* **Overlay missing**: verify `assets/models/Keyboard.png` and that `P` toggles visibility.
* **Controls too slow/fast**: the "run/fire" modifier changes rates; see `Starter.hpp` mapping.
