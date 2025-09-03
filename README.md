# CG_hospital

A modern Vulkan application that renders an interactive 3D hospital scene with dynamic lighting, object manipulation, and overlay features.

---

## ✨ Overview

CG_hospital is a sophisticated 3D rendering application built with Vulkan that provides an interactive visualization of a hospital environment. The application offers:

* **Scene Management**
  * Loads and renders 3D scenes from `assets/models/scene.json`
  * Dynamic object manipulation and visibility control
  * Support for multiple selectable and interactive scene elements

* **Interactive Features**
  * Dual operation modes: Camera and Edit
  * Real-time object transformation (translate/rotate/scale)
  * Object visibility toggling
  * Interactive selection system

* **Rendering Features**
  * Advanced Blinn-Phong lighting with 8 point lights
  * Screen-space overlay system
  * Dynamic text rendering
  * Real-time transform updates

---

## 🎮 Controls

### Global Controls
* `ESC` - Exit application
* `Q` - Toggle between Camera and Edit modes
* `P` - Toggle keyboard overlay visibility
* `L` - Hold to view list of visible objects (when overlay is hidden)

### Object Management
* `TAB` - Cycle through selectable objects
* `D` - Delete selected object (Edit mode)

### Camera Mode
* Movement controlled via six-axis input system
* Standard FPS-style camera controls
* Mouse/gamepad for view control

### Edit Mode (Transform Controls)
* **Translation** (World Space)
  * `←/→` - X-axis movement
  * `↑/↓` - Z-axis movement
  * `R/F` - Y-axis movement

* **Rotation**
  * `T` - Rotate +Y (local)
  * `G` - Rotate -Y (local)

* **Scale**
  * `W` - Increase size
  * `S` - Decrease size
---

## 🔧 Technical Architecture

### Pipeline Structure
* **Global Descriptor Set** (set=0)
  * Contains global uniform buffer (GlobalUBO)
  * Shared lighting and camera data

* **Mesh Descriptor Set** (set=1)
  * Local uniform buffer (LocalUBO)
  * Two texture bindings

* **Overlay Descriptor Set**
  * Overlay uniform buffer
  * Keyboard texture binding

### Shader Pipeline
* **Mesh Pipeline**
  * Vertex: `shaders/Mesh.vert.spv`
  * Fragment: `shaders/Lambert-Blinn.frag.spv`
  * Handles 3D geometry rendering

* **Overlay Pipeline**
  * Vertex: `shaders/Overlay.vert.spv`
  * Fragment: `shaders/Overlay.frag.spv`
  * Manages 2D overlay elements

---

## 📦 Asset Requirements

### Core Assets
* Scene definition: `assets/models/scene.json`
* Overlay texture: `assets/models/Keyboard.png`

### Shader Files
* `shaders/Mesh.vert.spv`
* `shaders/Lambert-Blinn.frag.spv`
* `shaders/Overlay.vert.spv`
* `shaders/Overlay.frag.spv`
* `shaders/Text.vert.spv`
* `shaders/Text.frag.spv`
---

## 🏗 Building the Project

### Prerequisites
* Vulkan SDK (with validation layers)
* GLFW3
* GLM
* nlohmann::json
* C++17 capable compiler
* MCGC framework components

### Build Configuration

* Switch between **Camera** and **Edit** modes.
* **Select** scene instances by ID and **manipulate** their transforms (translate/rotate/scale).
* **Delete** individual objects 
* Toggle an on-screen keyboard overlay and a list of visible items.

The app sets up two pipelines:

* **Mesh** (3D geometry, per-instance local uniforms)
* **Overlay** (screen-space quad with `assets/models/Keyboard.png` + on-screen text)

Lighting uses a simple Blinn/Lambert fragment shader with eight point lights and ambient.

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

