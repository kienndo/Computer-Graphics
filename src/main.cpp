#include <sstream>
#include <json.hpp>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "modules/Starter.hpp"
#include "modules/Scene.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct VertexSimp {
    glm::vec3 pos;
    glm::vec3 norm;
    glm::vec2 UV;
};

struct GlobalUBO {
    alignas(16) glm::vec3 lightDir;
    alignas(16) glm::vec4 lightColor;
    alignas(16) glm::vec3 eyePos;
};

struct LocalUBO {
    alignas(16) glm::mat4 mvpMat;
    alignas(16) glm::mat4 mMat;
    alignas(16) glm::mat4 nMat;
    alignas(16) glm::vec4 highlight;  // x = 1.0 → highlighted, 0.0 otherwise
};


struct OverlayUniformBuffer {
    alignas(4) float visible;
};

struct VertexOverlay {
    alignas(16) glm::vec3 pos;
    alignas(16) glm::vec2 UV;
};

class CG_hospital : public BaseProject {
protected:
    float Ar = 4.0f/3.0f;
    int  selectedObjectIndex = -1;   // real instance index into SC.TI[0].I[...]
    int  selectedListPos     = -1;   // position inside selectableIndices
    bool tabPressed          = false;
    int prevTabState = GLFW_RELEASE;


    std::vector<int> selectableIndices;     // instance indices you can select
    std::vector<std::string> selectableIds; // their human-readable IDs from JSON (optional UI)


    RenderPass RP;
    DescriptorSetLayout DSLglobal, DSLmesh;

    VertexDescriptor VDsimp;
    Pipeline         PMesh;

    DescriptorSet    DSGubo;

    // Several Models
    Scene SC;
    std::vector<VertexDescriptorRef> VDRs;
    std::vector<TechniqueRef>PRs;

    // --- Camera (simple fixed cam) ---
    glm::vec3 camPos{0.0f, 1.6f, 6.0f};

    // --- Object transform state (controlled by keyboard) ---
    glm::vec3 objPos   {0.0f, 0.0f, 0.0f};
    float     objYaw   = 0.0f; // around Y
    float     objPitch = 0.0f; // around X
    float     objRoll  = 0.0f; // around Z
    float     objScale = 0.01f;

    // Speeds
    float MOVE_SPEED = 10.0f;                 // meters/sec
    float ROT_SPEED  = glm::radians(90.0f);  // rad/sec

    void setWindowParameters() {
        windowWidth = 1280;
        windowHeight = 720;
        windowTitle = "CG_hospital";
        windowResizable = GLFW_TRUE;
        Ar = float(windowWidth) / float(windowHeight);
    }

    void onWindowResize(int w, int h) {
        std::cout << "Window resized to: " << w << " x " << h << "\n";
        if (h > 0) Ar = float(w) / float(h);
        RP.width  = w;
        RP.height = h;
    }

    void localInit() {

        // DSLs
        // set = 0 (global)
        DSLglobal.init(this, {
          { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS,
            sizeof(GlobalUBO), 1 }
        });

        // set = 1 (local)
        DSLmesh.init(this, {
          { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         VK_SHADER_STAGE_VERTEX_BIT,
            sizeof(LocalUBO), 1 },
          { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT,
            0, 1 },
          { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT,
            1, 1 }
        });


        // Vertex layout
        VDsimp.init(this,
        { {0, sizeof(VertexSimp), VK_VERTEX_INPUT_RATE_VERTEX} },
        {
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexSimp,pos),  sizeof(glm::vec3), POSITION},
            {0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexSimp,norm), sizeof(glm::vec3), NORMAL},
            {0, 2, VK_FORMAT_R32G32_SFLOAT,    offsetof(VertexSimp,UV),   sizeof(glm::vec2), UV}
        }
        );

        VDRs.resize(1);
        VDRs[0].init("VDsimp", &VDsimp); // OR use the name “VMesh” if your JSON expects that label


        RP.init(this);
        RP.properties[0].clearValue = {0.05f, 0.05f, 0.08f, 1.0f};

        PMesh.init(this, &VDsimp,
        "shaders/Mesh.vert.spv",
  "shaders/Mesh.frag.spv",
  { &DSLglobal, &DSLmesh }
        );
        PMesh.setCullMode(VK_CULL_MODE_NONE);
        PMesh.setPolygonMode(VK_POLYGON_MODE_FILL);


        PRs.resize(1);
        PRs[0].init("Mesh", {
          { &PMesh, { /* set0 (global) */{},
                      /* set1 (local)  */{
                        /* binding 0: UBO  */ { true, 0, {} },
                        /* binding 1: tex  */ { true, 1, { } }
                      } } }
        }, /*TotalNtextures*/2, &VDsimp);

        // Pool sizing
        DPSZs.uniformBlocksInPool = 3;
        DPSZs.texturesInPool      = 4;
        DPSZs.setsInPool          = 3;

        std::cout << "\nLoading the scene\n\n";
        SC.init(this, 1, VDRs, PRs, "assets/models/scene.json");
        buildSelectableFromJSON("assets/models/scene.json");
    }

    void pipelinesAndDescriptorSetsInit() {
        RP.create();
        PMesh.create(&RP);

        DSGubo.init(this, &DSLglobal, {});

        SC.pipelinesAndDescriptorSetsInit();

        // Register CB filler
        submitCommandBuffer("main", 0, populateCommandBufferAccess, this);
    }

    void pipelinesAndDescriptorSetsCleanup() {
        PMesh.cleanup();
        RP.cleanup();
        DSGubo.cleanup();

        SC.pipelinesAndDescriptorSetsCleanup();
    }

    void localCleanup() {
        DSLglobal.cleanup();
        DSLmesh.cleanup();
        PMesh.destroy();
        RP.destroy();

        SC.localCleanup();
    }

    void populateCommandBuffer(VkCommandBuffer cmdBuffer, int currentImage){
        RP.begin(cmdBuffer, currentImage);


        PMesh.bind(cmdBuffer);

        // You own set=0 (global). Scene will NOT bind it for you.
        DSGubo.bind(cmdBuffer, PMesh, 0, currentImage);

        // Scene binds only its local set(s) and issues vkCmdDrawIndexed
        SC.populateCommandBuffer(cmdBuffer, 0, currentImage);

        RP.end(cmdBuffer);
    }

    void updateUniformBuffer(uint32_t currentImage) {

            if (glfwGetKey(window, GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(window, GL_TRUE);
            handleObjectSelection();

            // --- Inputs (same getSixAxis) ---
            float dt = 0.0f; glm::vec3 m(0.0f), r(0.0f); bool fire = false;
            getSixAxis(dt, m, r, fire);

            manipulateSelected(dt, fire);

            static float yaw   = 0.0f;                 // radians
            static float pitch = 0.0f;
            static glm::vec3 camPos = glm::vec3(0.0f, 1.6f, 6.0f);


            const float ROT_SPEED         = glm::radians(120.0f);  // rad/sec
            const float MOVE_SPEED_BASE   = 10.0f;                  // m/s
            const float MOVE_SPEED_RUN    = 10.0f;                  // m/s
            const float MOVE_SPEED        = fire ? MOVE_SPEED_RUN : MOVE_SPEED_BASE;

            yaw   -= r.y * ROT_SPEED * dt;
            pitch -= r.x * ROT_SPEED * dt;
            pitch  = glm::clamp(pitch, glm::radians(-89.0f), glm::radians(89.0f));

            // R = Ry * Rx
            glm::mat4 Ry = glm::rotate(glm::mat4(1), yaw,   glm::vec3(0,1,0));
            glm::mat4 Rx = glm::rotate(glm::mat4(1), pitch, glm::vec3(1,0,0));
            glm::mat4 R  = Ry * Rx;

            glm::vec3 fwd = glm::normalize(glm::vec3(R * glm::vec4(0,0,-1,0))); // forward
            glm::vec3 rgt = glm::normalize(glm::vec3(R * glm::vec4(1,0, 0,0)));
            glm::vec3 up  = glm::normalize(glm::vec3(R * glm::vec4(0,1, 0,0)));

            // Movement: match E09 sign convention (forward is -m.z along forward dir)
            camPos += rgt * (m.x * MOVE_SPEED * dt);
            camPos += up  * (m.y * MOVE_SPEED * dt);
            camPos -= fwd * (m.z * MOVE_SPEED * dt);  // NOTE: minus here to mirror E09

            // --- Matrices (unchanged) ---
            float Ar = float(windowWidth) / float(windowHeight);
            glm::mat4 Prj = glm::perspective(glm::radians(60.0f), Ar, 0.01f, 200.0f);
            Prj[1][1] *= -1.0f;

            glm::mat4 View = glm::lookAt(camPos, camPos + fwd, glm::vec3(0,1,0));

            // --- Global UBO ---
            GlobalUBO g{};
            g.lightDir   = glm::normalize(glm::vec3(1,2,3));
            g.lightColor = glm::vec4(1,1,1,1);
            g.eyePos     = camPos;

        for (int i = 0; i < SC.TI[0].InstanceCount; ++i) {
            auto &inst = SC.TI[0].I[i];

            LocalUBO l{};
            l.mMat   = inst.Wm;
            l.nMat   = glm::inverse(glm::transpose(l.mMat));
            l.mvpMat = Prj * View * l.mMat;


            l.highlight = glm::vec4((i == selectedObjectIndex) ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);

            // map as you already do:
            inst.DS[0][0]->map(currentImage, &g, 0);  // global
            inst.DS[0][1]->map(currentImage, &l, 0);  // local
        }

    }

    static void populateCommandBufferAccess(VkCommandBuffer commandBuffer, int currentImage, void *params) {
        auto *app = reinterpret_cast<CG_hospital*>(params);
        app->populateCommandBuffer(commandBuffer, currentImage);
    }

    void buildSelectableFromJSON(const char* path) {
        try {
            std::ifstream f(path);
            if (!f) return;
            nlohmann::json j; f >> j;

            auto& instBlocks = j["instances"];
            if (!instBlocks.is_array() || instBlocks.empty()) return;

            auto& elements = instBlocks[0]["elements"];
            if (!elements.is_array()) return;

            // Denylist / rules for unselectables (floor, wall, etc.)
            auto isUnselectable = [](const std::string& id){
                std::string s = id;
                std::transform(s.begin(), s.end(), s.begin(), ::tolower);
                return (s == "floor" || s == "wall" ||
                        s.find("ceiling") != std::string::npos ||
                        s.find("sky")     != std::string::npos);
            };

            selectableIndices.clear();
            selectableIds.clear();

            // elements order == Scene instance order
            for (int i = 0; i < (int)elements.size(); ++i) {
                std::string id = elements[i].value("id", std::string{});
                if (id.empty() || !isUnselectable(id)) {
                    selectableIndices.push_back(i);       // keep instance index
                    selectableIds.push_back(id.empty() ? ("instance_" + std::to_string(i)) : id);
                }
            }
        } catch (...) {
            int count = /* safe fallback if Scene is present */ 0;
            for (int i=0; i<count; ++i) {
                selectableIndices.push_back(i);
                selectableIds.push_back("instance_" + std::to_string(i));
            }
        }
    }

    void manipulateSelected(float dt, bool fire) {
    if (selectedObjectIndex < 0) return;              // nothing selected

    // Tune these however you like (Shift = "fire" already from getSixAxis)
    const float MOVE = (fire ? 50.0f : 15.0f);        // units/sec
    const float ROT  = glm::radians(fire ? 180.0f : 90.0f); // rad/sec
    const float SCL  = (fire ? 1.5f : 1.2f);          // scale step multiplier

    auto &inst = SC.TI[0].I[selectedObjectIndex];

    // --- TRANSLATE in WORLD space (pre-multiply) ---
    // Arrows: X/Z, PageUp/PageDown: Y
    if (glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS) inst.Wm = glm::translate(glm::mat4(1), glm::vec3(-MOVE*dt, 0,        0)) * inst.Wm;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) inst.Wm = glm::translate(glm::mat4(1), glm::vec3( MOVE*dt, 0,        0)) * inst.Wm;
    if (glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS) inst.Wm = glm::translate(glm::mat4(1), glm::vec3( 0,        0, -MOVE*dt)) * inst.Wm;
    if (glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS) inst.Wm = glm::translate(glm::mat4(1), glm::vec3( 0,        0,  MOVE*dt)) * inst.Wm;
    if (glfwGetKey(window, GLFW_KEY_PAGE_UP)   == GLFW_PRESS) inst.Wm = glm::translate(glm::mat4(1), glm::vec3( 0,  MOVE*dt, 0)) * inst.Wm;
    if (glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS) inst.Wm = glm::translate(glm::mat4(1), glm::vec3( 0, -MOVE*dt, 0)) * inst.Wm;

    // --- ROTATE in LOCAL space (post-multiply) ---
    // R/F => ±X,  T/G => ±Y,  Y/H => ±Z
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) inst.Wm = inst.Wm * glm::rotate(glm::mat4(1),  ROT*dt, glm::vec3(1,0,0));
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) inst.Wm = inst.Wm * glm::rotate(glm::mat4(1), -ROT*dt, glm::vec3(1,0,0));

    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) inst.Wm = inst.Wm * glm::rotate(glm::mat4(1),  ROT*dt, glm::vec3(0,1,0));
    if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS) inst.Wm = inst.Wm * glm::rotate(glm::mat4(1), -ROT*dt, glm::vec3(0,1,0));

    if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS) inst.Wm = inst.Wm * glm::rotate(glm::mat4(1),  ROT*dt, glm::vec3(0,0,1));
    if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS) inst.Wm = inst.Wm * glm::rotate(glm::mat4(1), -ROT*dt, glm::vec3(0,0,1));

    // --- SCALE uniformly in LOCAL space (post-multiply) ---
    // '[' = smaller, ']' = bigger
    if (glfwGetKey(window, GLFW_KEY_KP_SUBTRACT)  == GLFW_PRESS) {
        float s = std::pow(1.0f / SCL, dt * 60.0f); // frame-rate independent-ish
        inst.Wm = inst.Wm * glm::scale(glm::mat4(1), glm::vec3(s));
    }
    if (glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS) {
        float s = std::pow(SCL, dt * 60.0f);
        inst.Wm = inst.Wm * glm::scale(glm::mat4(1), glm::vec3(s));
    }
}

    void handleObjectSelection() {
        // Rising-edge detection: fires once when the key goes from RELEASE -> PRESS
        int state = glfwGetKey(window, GLFW_KEY_COMMA);
        if (state == GLFW_PRESS && prevTabState == GLFW_RELEASE) {
            if (selectableIndices.empty()) {
                selectedObjectIndex = -1;
                selectedListPos     = -1;
                std::cout << "No selectable objects.\n";
            } else {
                selectedListPos = (selectedListPos + 1) % (int)selectableIndices.size();
                selectedObjectIndex = selectableIndices[selectedListPos];

                const std::string& label =
                    (selectedListPos >= 0 && selectedListPos < (int)selectableIds.size())
                    ? selectableIds[selectedListPos]
                    : std::string("instance_") + std::to_string(selectedObjectIndex);

                std::cout << "Selected object idx: " << selectedObjectIndex
                          << "  id: " << label << "\n";
            }
        }
        prevTabState = state;
    }

};

int main() {
    CG_hospital app;
    try { app.run(); }
    catch (const std::exception& e) { std::cerr << e.what() << std::endl; return EXIT_FAILURE; }
    return EXIT_SUCCESS;
}