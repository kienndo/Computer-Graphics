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
#include "modules/TextMaker.hpp"
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
    alignas(16) glm::vec4 highlight;
};

struct OverlayUniformBuffer {
    alignas(4) float visible;
};

struct VertexOverlay {
    glm::vec2 pos;
    glm::vec2 UV;
};

class CG_hospital : public BaseProject {
protected:
    float Ar = 4.0f/3.0f;
    int  selectedObjectIndex = -1;   // real instance index into SC.TI[0].I[...]
    int  selectedListPos     = -1;   // position inside selectableIndices
    bool tabPressed          = false;
    int prevTabState = GLFW_RELEASE;


    // --- Mode switch state ---
    bool editMode = false;                 // false = Camera mode, true = Edit mode
    int  prevQState = GLFW_RELEASE;        // rising-edge detection for 'Q'
    int  prevCommaState = GLFW_RELEASE;    // keep separate from Tab/Comma selection


    OverlayUniformBuffer overlayUBO{};  // default visible = 0.0f

    std::vector<int> selectableIndices;     // instance indices you can select
    std::vector<std::string> selectableIds; // their human-readable IDs from JSON (optional UI)

    RenderPass RP;
    DescriptorSetLayout DSLglobal, DSLmesh, DSLoverlay;

    VertexDescriptor VDsimp, VDoverlay;
    Pipeline         PMesh, POverlay;

    DescriptorSet    DSGubo, DSOverlay;
    Texture TOverlay;
    Model MOverlay;

    // Several Models
    Scene SC;
    std::vector<VertexDescriptorRef> VDRs;
    std::vector<TechniqueRef>PRs;

    TextMaker txt;

    // --- Camera (simple fixed cam) ---
    glm::vec3 camPos{0.0f, 1.6f, 6.0f};
    float     camYaw   = 0.0f;             // radians
    float     camPitch = 0.0f;             // radians
    glm::vec3 camFwd{0,0,-1}, camRight{1,0,0}, camUp{0,1,0};

    // --- Object transform state (controlled by keyboard) ---
    glm::vec3 objPos   {0.0f, 0.0f, 0.0f};
    float     objYaw   = 0.0f; // around Y
    float     objPitch = 0.0f; // around X
    float     objRoll  = 0.0f; // around Z
    float     objScale = 0.01f;

    // Speeds
    float MOVE_SPEED = 10.0f;
    float ROT_SPEED  = glm::radians(90.0f);

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

        txt.resizeScreen(w, h);
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

        DSLoverlay.init(this, {
                 {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS, sizeof(OverlayUniformBuffer), 1},
                 {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1}
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

        VDoverlay.init(this, {
                  {0, sizeof(VertexOverlay), VK_VERTEX_INPUT_RATE_VERTEX}
                }, {
                  {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(VertexOverlay, pos),
                         sizeof(glm::vec2), OTHER},
                  {0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(VertexOverlay, UV),
                         sizeof(glm::vec2), UV}
                });

        POverlay.init(this, &VDoverlay,
              "shaders/Overlay.vert.spv",
              "shaders/Overlay.frag.spv",
              { &DSLoverlay });

        POverlay.setCompareOp(VK_COMPARE_OP_ALWAYS);
        POverlay.setCullMode(VK_CULL_MODE_NONE);
        POverlay.setPolygonMode(VK_POLYGON_MODE_FILL);


        VDRs.resize(1);
        VDRs[0].init("VDsimp", &VDsimp);


        RP.init(this);
        RP.properties[0].clearValue = {0.05f, 0.05f, 0.08f, 1.0f};

        PMesh.init(this, &VDsimp,
        "shaders/Mesh.vert.spv",
  "shaders/Mesh.frag.spv",{ &DSLglobal, &DSLmesh });
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
        DPSZs.uniformBlocksInPool = 4;
        DPSZs.texturesInPool      = 29;
        DPSZs.setsInPool          = 3;

        MOverlay.vertices = std::vector<unsigned char>(4 * sizeof(VertexOverlay));
        VertexOverlay *V1 = (VertexOverlay *)(&(MOverlay.vertices[0]));

        V1[0] = { {-1.0f,  -0.7}, {0.0f, 0.0f} }; // bottom-left
        V1[1] = { {-1.0f,  -1.0f   }, {0.0f, 1.0f} }; // top-left
        V1[2] = { {-0.6f, -0.7}, {1.0f, 0.0f} }; // bottom-right
        V1[3] = { {-0.6f, -1.0f   }, {1.0f, 1.0f} }; // top-right

        MOverlay.indices = {0, 1, 2,    1, 2, 3};
        MOverlay.initMesh(this, &VDoverlay);

        TOverlay.init(this, "assets/models/Untitled.png");
        txt.init(this, windowWidth, windowHeight);

        // Add a tiny dummy
        txt.print(-0.9f, -0.9f, ("CAM MODE"), 2, "SS");
        txt.print(-0.9f, -0.7f, "Currently editing: \nNone",
              4,
              "SS",
              false, true, true,
              TAL_LEFT, TRH_LEFT, TRV_TOP);
        txt.updateCommandBuffer();


        std::cout << "\nLoading the scene\n\n";
        SC.init(this, 1, VDRs, PRs, "assets/models/scene.json");
        buildSelectableFromJSON("assets/models/scene.json");
    }

    void pipelinesAndDescriptorSetsInit() {
        RP.create();
        PMesh.create(&RP);
        POverlay.create(&RP);

        DSGubo.init(this, &DSLglobal, {});
        DSOverlay.init(this, &DSLoverlay, {TOverlay.getViewAndSampler()});

        SC.pipelinesAndDescriptorSetsInit();
        txt.pipelinesAndDescriptorSetsInit();

        submitCommandBuffer("main", 0, populateCommandBufferAccess, this);
    }

    void pipelinesAndDescriptorSetsCleanup() {

        txt.removeAllText();
        txt.print(0.0f, 0.0f, " ", -1, "SS");
        txt.updateCommandBuffer();

        PMesh.cleanup();
        RP.cleanup();
        DSGubo.cleanup();
        POverlay.cleanup();
        DSOverlay.cleanup();

        SC.pipelinesAndDescriptorSetsCleanup();
        txt.pipelinesAndDescriptorSetsCleanup();
    }

    void localCleanup() {
        TOverlay.cleanup();
        MOverlay.cleanup();
        DSLoverlay.cleanup();

        DSLglobal.cleanup();
        DSLmesh.cleanup();
        PMesh.destroy();
        RP.destroy();
        POverlay.destroy();

        txt.localCleanup();
        SC.localCleanup();
    }

    void populateCommandBuffer(VkCommandBuffer cmdBuffer, int currentImage){
        RP.begin(cmdBuffer, currentImage);

        // --- Scene first ---
        PMesh.bind(cmdBuffer);
        DSGubo.bind(cmdBuffer, PMesh, 0, currentImage);           // set=0 for scene
        SC.populateCommandBuffer(cmdBuffer, 0, currentImage);     // draws all mesh instances


        POverlay.bind(cmdBuffer);
        DSOverlay.bind(cmdBuffer, POverlay, 0, currentImage);
        MOverlay.bind(cmdBuffer);
        vkCmdDrawIndexed(cmdBuffer,
        static_cast<uint32_t>(MOverlay.indices.size()), 1, 0, 0, 0);

        RP.end(cmdBuffer);
    }

    void handleModeToggle() {
        int s = glfwGetKey(window, GLFW_KEY_Q);
        if (s == GLFW_PRESS && prevQState == GLFW_RELEASE) {
            editMode = !editMode;
            std::cout << (editMode ? "[MODE] Edit\n" : "[MODE] Camera\n");
            txt.print(-0.9f, -0.9f, (editMode ? "EDIT MODE" : "CAM MODE"), 2, "SS");
            txt.updateCommandBuffer();
        }
        prevQState = s;
    }

    void updateFromInput(float dt, const glm::vec3& m, const glm::vec3& r, bool fire) {
        const float ROT_SPEED       = glm::radians(120.0f);
        const float MOVE_SPEED_BASE = 10.0f;
        const float MOVE_SPEED_RUN  = 10.0f;
        const float MOVE_SPEED      = fire ? MOVE_SPEED_RUN : MOVE_SPEED_BASE;

        if (!editMode) {
            // --- CAMERA MODE ---
            camYaw   -= r.y * ROT_SPEED * dt;
            camPitch -= r.x * ROT_SPEED * dt;
            camPitch  = glm::clamp(camPitch, glm::radians(-89.0f), glm::radians(89.0f));

            // R = Ry * Rx
            glm::mat4 Ry = glm::rotate(glm::mat4(1), camYaw,   glm::vec3(0,1,0));
            glm::mat4 Rx = glm::rotate(glm::mat4(1), camPitch, glm::vec3(1,0,0));
            glm::mat4 R  = Ry * Rx;

            camFwd   = glm::normalize(glm::vec3(R * glm::vec4(0,0,-1,0)));
            camRight = glm::normalize(glm::vec3(R * glm::vec4(1,0, 0,0)));
            camUp    = glm::normalize(glm::vec3(R * glm::vec4(0,1, 0,0)));

            camPos += camRight * (m.x * MOVE_SPEED * dt);
            camPos += camUp    * (m.y * MOVE_SPEED * dt);
            camPos -= camFwd   * (m.z * MOVE_SPEED * dt);
        } else {
            // --- EDIT MODE ---
            manipulateSelected(dt, fire);
        }
    }

    void updateUniformBuffer(uint32_t currentImage) {

        if (glfwGetKey(window, GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(window, GL_TRUE);

        // 1) Input (once)
        float dt = 0.0f;
        glm::vec3 m(0.0f), r(0.0f);
        bool fire = false;

        getSixAxis(dt, m, r, fire);

        // 2) Selection & mode toggle (edge-triggered)
        handleObjectSelection();
        handleModeToggle();

        // 3) Update either camera or selected object based on mode
        updateFromInput(dt, m, r, fire);

        // 4) Matrices
        float Ar = float(windowWidth) / float(windowHeight);
        glm::mat4 Prj = glm::perspective(glm::radians(60.0f), Ar, 0.01f, 200.0f);
        Prj[1][1] *= -1.0f;

        glm::mat4 View = glm::lookAt(camPos, camPos + camFwd, glm::vec3(0,1,0));

        // 5) Global UBO
        GlobalUBO g{};
        g.lightDir   = glm::normalize(glm::vec3(1,2,3));
        g.lightColor = glm::vec4(1,1,1,1);
        g.eyePos     = camPos;

        // 6) Per-instance locals
        for (int i = 0; i < SC.TI[0].InstanceCount; ++i) {
            auto &inst = SC.TI[0].I[i];

            LocalUBO l{};
            l.mMat   = inst.Wm;
            l.nMat   = glm::inverse(glm::transpose(l.mMat));
            l.mvpMat = Prj * View * l.mMat;
            l.highlight = glm::vec4((i == selectedObjectIndex) ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);

            inst.DS[0][0]->map(currentImage, &g, 0);  // global
            inst.DS[0][1]->map(currentImage, &l, 0);  // local
        }

        // 7) Overlay
        overlayUBO.visible = 1.0f;
        DSOverlay.map(currentImage, &overlayUBO, 0);
    }


    static void populateCommandBufferAccess(VkCommandBuffer commandBuffer, int currentImage, void *params) {
        auto *app = reinterpret_cast<CG_hospital*>(params);
        app->populateCommandBuffer(commandBuffer, currentImage);
    }

    void buildSelectableFromJSON(const char* path) {
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
                return (s == "floor" || s == "wall" || s =="door" || s == "window");
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

        // --- ROTATE about Y-axis ---
        if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) inst.Wm = inst.Wm * glm::rotate(glm::mat4(1),  ROT*dt, glm::vec3(0,1,0));
        if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS) inst.Wm = inst.Wm * glm::rotate(glm::mat4(1), -ROT*dt, glm::vec3(0,1,0));

        // --- SCALE uniformly in LOCAL space (post-multiply) ---
        bool plusKey  = glfwGetKey(window, GLFW_KEY_KP_ADD)      == GLFW_PRESS
                     || glfwGetKey(window, GLFW_KEY_EQUAL)       == GLFW_PRESS;   // main row "=" / "+" (with Shift)
        bool minusKey = glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS
                     || glfwGetKey(window, GLFW_KEY_MINUS)       == GLFW_PRESS;   // main row "-"

        float step = std::pow(SCL, dt * 5.0f);

        if (plusKey) {
            // grow, MAC: "+"
            inst.Wm = inst.Wm * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f / step));
        }
        if (minusKey) {
            // shrink (inverse step), MAC: "´"
            inst.Wm = inst.Wm * glm::scale(glm::mat4(1.0f), glm::vec3(step));

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
                    ? "Currently editing: \n" + selectableIds[selectedListPos]
                    : std::string("instance_") + std::to_string(selectedObjectIndex);
                txt.print(-0.9f, -0.7f, label,
              4,
              "SS",
              false, true, true,
              TAL_LEFT, TRH_LEFT, TRV_TOP);

                txt.updateCommandBuffer();

                std::cout << "Selected object idx: " << selectedObjectIndex
                          << "  id: " << label << "\n";
            }
        }
        prevTabState = state;
        txt.updateCommandBuffer();

    }
};

int main() {
    CG_hospital app;
    try { app.run(); }
    catch (const std::exception& e) { std::cerr << e.what() << std::endl; return EXIT_FAILURE; }
    return EXIT_SUCCESS;
}