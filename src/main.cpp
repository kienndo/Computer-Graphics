#include <sstream>
#include <json.hpp>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_set>

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
    alignas(16) glm::vec4 lightPos[8];
    alignas(16) glm::vec4 lightColor;
    alignas(4) glm::float32 decayFactor;
    alignas(4) glm::float32 g;
    alignas(4) glm::float32 numLights;
    alignas(16) glm::vec3 ambientLightColor;
    alignas(16) glm::vec3 eyePos;
};

struct LocalUBO {
    alignas(4) glm::float32 gamma;
    alignas(16) glm::vec3 specularColor;
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

    // Iterate through assets
    bool tabPressed = false;
    int prevTabState = GLFW_RELEASE;

    // Delete assets
    std::unordered_set<std::string> hiddenIds;
    int prevDelState = GLFW_RELEASE;

    // Mode switch state, false = Camera mode, true = Edit mode
    bool editMode = false;
    int  prevQState = GLFW_RELEASE;

    // Keyboard info
    bool showKeyOverlay = true;
    int  prevPlusState  = GLFW_RELEASE;

    // List of assets
    bool showList = true;
    int  prevLState = GLFW_RELEASE;

    OverlayUniformBuffer KeyUBO{};

    std::vector<int> selectableIndices;  // Assets
    std::vector<std::string> selectableIds;  // Name of the assets
    int  selectedObjectIndex = -1;
    int  selectedListPos     = -1;

    RenderPass RP;
    DescriptorSetLayout DSLglobal, DSLmesh, DSLoverlay;

    VertexDescriptor VDsimp, VDoverlay;
    Pipeline         PMesh, POverlay;

    DescriptorSet    DSGubo, DSKey;
    Texture TKey;
    Model MKey;

    // Several Models
    Scene SC;
    std::vector<VertexDescriptorRef> VDRs;
    std::vector<TechniqueRef>PRs;

    TextMaker txt;

    // Camera
    glm::vec3 camPos{0.0f, 40.0f, 6.0f};
    float     camYaw   = 0.0f;
    float     camPitch = -0.5f;
    glm::vec3 camFwd{0,0,-1}, camRight{1,0,0}, camUp{0,1,0};


    void setWindowParameters() {
        windowWidth = 1280;
        windowHeight = 720;
        windowTitle = "CG_hospital";
        windowResizable = GLFW_TRUE;
    }

    void onWindowResize(int w, int h) override {
        std::cout << "Window resized to: " << w << " x " << h << "\n";
        if (h > 0) Ar = float(w) / float(h);
        RP.width  = w;
        RP.height = h;

        txt.resizeScreen(w, h);
    }

    void localInit() override {
        // set = 0 (global)
        DSLglobal.init(this, {
            { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS,
                sizeof(GlobalUBO), 1}
        });

        // set = 1 (local)
        DSLmesh.init(this, {
            { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            sizeof(LocalUBO), 1 },
            { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT,
            0, 1 },
            { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT,
            1, 1 }
        });

        DSLoverlay.init(this, {
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS,
                sizeof(OverlayUniformBuffer), 1},
            {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT,
                0, 1}
        });

        VDsimp.init(this,
        { {0, sizeof(VertexSimp), VK_VERTEX_INPUT_RATE_VERTEX} },
        {
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexSimp,pos),  sizeof(glm::vec3), POSITION},
            {0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexSimp,norm), sizeof(glm::vec3), NORMAL},
            {0, 2, VK_FORMAT_R32G32_SFLOAT,    offsetof(VertexSimp,UV),   sizeof(glm::vec2), UV}
        });

        VDoverlay.init(this,
        { {0, sizeof(VertexOverlay), VK_VERTEX_INPUT_RATE_VERTEX} },
            {
                {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(VertexOverlay, pos), sizeof(glm::vec2), OTHER},
                {0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(VertexOverlay, UV), sizeof(glm::vec2), UV}
            });

        RP.init(this);
        RP.properties[0].clearValue = {0.05f, 0.05f, 0.08f, 1.0f};

        POverlay.init(this, &VDoverlay,
            "shaders/Overlay.vert.spv",
            "shaders/Overlay.frag.spv",
            {&DSLoverlay});

        POverlay.setCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL);
        POverlay.setCullMode(VK_CULL_MODE_NONE);
        POverlay.setPolygonMode(VK_POLYGON_MODE_FILL);

        PMesh.init(this, &VDsimp,
            "shaders/Mesh.vert.spv",
            "shaders/Lambert-Blinn.frag.spv",
        { &DSLglobal, &DSLmesh });

        PMesh.setCullMode(VK_CULL_MODE_NONE);
        PMesh.setPolygonMode(VK_POLYGON_MODE_FILL);
        PMesh.setCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL);

        VDRs.resize(1);
        VDRs[0].init("VDsimp", &VDsimp);

        PRs.resize(1);
        PRs[0].init("Mesh", {
          { &PMesh, { {},
            {
                { true, 0, {} },
                { true, 1, {} },
                }
            }
          }
        }, 2, &VDsimp);

        // Pool sizing
        DPSZs.uniformBlocksInPool = 4;
        DPSZs.texturesInPool      = 30;
        DPSZs.setsInPool          = 3;

        MKey.vertices = std::vector<unsigned char>(4 * sizeof(VertexOverlay));
        VertexOverlay *V2 = (VertexOverlay *)(&(MKey.vertices[0]));

        V2[0] = { {-0.6f, -0.6f}, {0.0f, 0.0f} }; // bottom-left
        V2[1] = { {-0.6f,  0.6f}, {0.0f, 1.0f} }; // top-left
        V2[2] = { { 0.6f, -0.6f}, {1.0f, 0.0f} }; // bottom-right
        V2[3] = { { 0.6f,  0.6f}, {1.0f, 1.0f} }; // top-right

        MKey.indices = {0, 1, 2, 1, 2, 3};
        MKey.initMesh(this, &VDoverlay);

        TKey.init(this, "assets/models/Keyboard.png");

        txt.init(this, windowWidth, windowHeight);

        txt.print(-0.95f, -0.95f, ("CAM MODE"), 1, "SS");
        txt.print(-0.95f, -0.85f, "Currently editing: \nNone", 2, "SS", false,
                true, true, TAL_LEFT, TRH_LEFT, TRV_TOP);
        txt.updateCommandBuffer();

        std::cout << "\nLoading the scene\n\n";
        SC.init(this, 1, VDRs, PRs, "assets/models/scene.json");
        buildSelectableFromJSON("assets/models/scene.json");
    }

    void pipelinesAndDescriptorSetsInit() override {
        RP.create();
        PMesh.create(&RP);
        POverlay.create(&RP);

        DSGubo.init(this, &DSLglobal, {});
        DSKey.init(this, &DSLoverlay, {TKey.getViewAndSampler()});

        SC.pipelinesAndDescriptorSetsInit();
        txt.pipelinesAndDescriptorSetsInit();

        submitCommandBuffer("main", 0, populateCommandBufferAccess, this);
    }

    void pipelinesAndDescriptorSetsCleanup() override {
        PMesh.cleanup();
        DSGubo.cleanup();
        POverlay.cleanup();
        DSKey.cleanup();
        RP.cleanup();

        SC.pipelinesAndDescriptorSetsCleanup();
        txt.pipelinesAndDescriptorSetsCleanup();
    }

    void localCleanup() {
        TKey.cleanup();
        MKey.cleanup();
        DSLoverlay.cleanup();

        DSLglobal.cleanup();
        DSLmesh.cleanup();
        PMesh.destroy();
        POverlay.destroy();
        RP.destroy();

        txt.localCleanup();
        SC.localCleanup();
    }

    void populateCommandBuffer(VkCommandBuffer cmdBuffer, int currentImage) {
        RP.begin(cmdBuffer, currentImage);

        PMesh.bind(cmdBuffer);
        DSGubo.bind(cmdBuffer, PMesh, 0, currentImage);  // set = 0 for scene
        SC.populateCommandBuffer(cmdBuffer, 0, currentImage);  // draws all mesh instances

        POverlay.bind(cmdBuffer);
        DSKey.bind(cmdBuffer, POverlay, 0, currentImage);
        MKey.bind(cmdBuffer);
        vkCmdDrawIndexed(cmdBuffer,static_cast<uint32_t>(MKey.indices.size()), 1,
                        0, 0, 0);

        RP.end(cmdBuffer);
    }


    void updateFromInput(float dt, const glm::vec3& m, const glm::vec3& r, bool fire) {
        const float ROT_SPEED       = glm::radians(120.0f);
        const float MOVE_SPEED_BASE = 10.0f;
        const float MOVE_SPEED_RUN  = 10.0f;
        const float MOVE_SPEED      = fire ? MOVE_SPEED_RUN : MOVE_SPEED_BASE;

        if (!editMode) {
            // CAMERA MODE
            camYaw   -= r.y * ROT_SPEED * dt;
            camPitch -= r.x * ROT_SPEED * dt;
            camPitch  = glm::clamp(camPitch, glm::radians(-89.0f), glm::radians(89.0f));

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
            // EDIT MODE
            manipulateSelected(dt, fire);
        }
    }

    void updateUniformBuffer(uint32_t currentImage) {

        if (glfwGetKey(window, GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(window, GL_TRUE);

        float dt = 0.0f;
        glm::vec3 m(0.0f), r(0.0f);
        bool fire = false;

        getSixAxis(dt, m, r, fire);

        handleObjectSelection();
        handleKeyboardOverlay();
        handleModeToggle();
        handleDelete();
        handleListDisplay();

        updateFromInput(dt, m, r, fire);

        float Ar = float(windowWidth) / float(windowHeight);
        glm::mat4 Prj = glm::perspective(glm::radians(60.0f), Ar, 0.01f, 270.0f);
        Prj[1][1] *= -1.0f;

        glm::mat4 View = glm::lookAt(camPos, camPos + camFwd, glm::vec3(0,1,0));

        glm::vec4 LightPos[8] = {
            glm::vec4(-15, 35, -50, 1),
            glm::vec4(-15, 35, 0, 1),
            glm::vec4(-15, 35, 50, 1),
            glm::vec4(-15, 35, 100, 1),
            glm::vec4(-67, 35, -10, 1),
            glm::vec4(-67, 35, 85, 1),
            glm::vec4(75, 35, -40, 1),
            glm::vec4(75, 35, -5, 1)
        };

        GlobalUBO g{};
        for (int i = 0; i < 8; i++) {
            g.lightPos[i] = LightPos[i];
        }
        g.lightColor = glm::vec4(1,0.95,0.9,1);
        g.decayFactor = 1.0f;
        g.g = 5.0f;
        g.numLights = 8;
        g.ambientLightColor = glm::vec3(0.2f, 0.19f, 0.18f);
        g.eyePos     = camPos;

        for (int i = 0; i < SC.TI[0].InstanceCount; ++i) {
            auto &inst = SC.TI[0].I[i];

            LocalUBO l{};
            l.gamma = 120.0f;
            l.specularColor = glm::vec3(1.0f, 0.95f, 0.9f);
            l.mMat   = inst.Wm;
            l.nMat   = glm::inverse(glm::transpose(l.mMat));
            l.mvpMat = Prj * View * l.mMat;

            const std::string& iid = *inst.id;  // instance's string id
            float visible = (hiddenIds.count(iid) ? 0.0f : 1.0f);

            l.highlight = glm::vec4(
                (i == selectedObjectIndex) ? 1.0f : 0.0f,  // x: highlighted selection
                0.0f,
                0.0f,
                visible  // w: visibility flag (1=show, 0=hide)
            );

            inst.DS[0][0]->map(currentImage, &g, 0);  // global
            inst.DS[0][1]->map(currentImage, &l, 0);  // local
        }

        KeyUBO.visible = showKeyOverlay ? 1.0f : 0.0f;
        DSKey.map(currentImage, &KeyUBO, 0);
    }

    void handleKeyboardOverlay() {
        int state = glfwGetKey(window, GLFW_KEY_P);

        if (state == GLFW_PRESS && prevPlusState == GLFW_RELEASE) {
            showKeyOverlay = !showKeyOverlay;
            txt.print(-0.95f, -0.7, (showKeyOverlay ? "" : "Hold L to see all furnitures"), 4,
                "SS", false, true, true, TAL_LEFT, TRH_LEFT, TRV_TOP);
            txt.print(-0.95f, -0.75, (showKeyOverlay ? "" : "Press P to show keyboard actions"), 3,
                "SS", false, true, true, TAL_LEFT, TRH_LEFT, TRV_TOP);
            txt.updateCommandBuffer();
        }

        prevPlusState = state;
    }

    void handleDelete() {

        int kState = glfwGetKey(window, GLFW_KEY_D);

        if (kState == GLFW_PRESS && prevDelState == GLFW_RELEASE && editMode) {
            if (selectedListPos >= 0 && selectedListPos < (int)selectableIds.size()) {
                const std::string id = selectableIds[selectedListPos];
                const bool wasVisible = (hiddenIds.count(id) == 0);

                if (wasVisible) {
                    hiddenIds.insert(id);
                    selectedListPos = -1;
                    selectedObjectIndex = -1;
                } else {
                    hiddenIds.erase(id);
                    // keep selection as None; user can TAB to choose again
                }

                // Always refresh the "Currently editing" label
                const std::string curr =
                    (selectedListPos >= 0 && selectedListPos < (int)selectableIds.size())
                    ? "Currently editing: \n" + selectableIds[selectedListPos]
                    : "Currently editing: \nNone";

                txt.print(-0.95f, -0.85f, curr, 2, "SS", false,
                    true, true, TAL_LEFT, TRH_LEFT, TRV_TOP);

                // Refresh list only if L is currently held
                if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
                    txt.print(-0.95f, -0.7f, makeVisibleListString(), 4, "SS",
                        false, true, true, TAL_LEFT, TRH_LEFT, TRV_TOP);
                }

                txt.updateCommandBuffer();
            }
        }

        prevDelState = kState;
    }

    void handleModeToggle() {
        int s = glfwGetKey(window, GLFW_KEY_Q);

        if (s == GLFW_PRESS && prevQState == GLFW_RELEASE) {
            editMode = !editMode;
            std::cout << (editMode ? "[MODE] Edit\n" : "[MODE] Camera\n");
            txt.print(-0.95f, -0.95f, (editMode ? "EDIT MODE" : "CAM MODE"), 1, "SS");
            txt.updateCommandBuffer();
        }

        prevQState = s;
    }

    void handleListDisplay() {
        int state = glfwGetKey(window, GLFW_KEY_L);

        // pressed -> show list
        if (state == GLFW_PRESS && prevLState == GLFW_RELEASE && !showKeyOverlay) {
            txt.print(-0.95f, -0.7f, makeVisibleListString(), 4, "SS",
                false, true, true, TAL_LEFT, TRH_LEFT, TRV_TOP);
            txt.updateCommandBuffer();
        }

        // released -> show hint (do NOT touch the "Currently editing" line)
        if (state == GLFW_RELEASE && prevLState == GLFW_PRESS && !showKeyOverlay) {
            txt.print(-0.95f, -0.7f, "Press L to see all furnitures", 4, "SS",
                false, true, true, TAL_LEFT, TRH_LEFT, TRV_TOP);
            txt.updateCommandBuffer();
        }

        prevLState = state;
    }

    void handleObjectSelection() {
        int state = glfwGetKey(window, GLFW_KEY_TAB);

        if (state == GLFW_PRESS && prevTabState == GLFW_RELEASE) {
            if (selectableIndices.empty() || !selectNextVisible(+1)) {
                selectedListPos = -1;
                selectedObjectIndex = -1;
            }

            const std::string curr =
                (selectedListPos >= 0 && selectedListPos < (int)selectableIds.size())
                ? "Currently editing: \n" + selectableIds[selectedListPos]
                : "Currently editing: \nNone";

            txt.print(-0.95f, -0.85f, curr, 2, "SS", false,
                        true, true, TAL_LEFT, TRH_LEFT, TRV_TOP);

            // Optional: if L held, refresh the list content too
            if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
                txt.print(-0.95f, -0.7f, makeVisibleListString(), 4, "SS",
                    false, true, true, TAL_LEFT, TRH_LEFT, TRV_TOP);
            }

            txt.updateCommandBuffer();
        }

        prevTabState = state;
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
                selectableIndices.push_back(i);  // keep instance index
                selectableIds.push_back(id.empty() ? ("instance_" + std::to_string(i)) : id);
            }
        }
    }

    void manipulateSelected(float dt, bool fire) {
        if (selectedObjectIndex < 0) return;  // nothing selected

        const float MOVE = (fire ? 50.0f : 15.0f);  // units/sec
        const float ROT = glm::radians(fire ? 180.0f : 90.0f);  // rad/sec
        const float SCL = (fire ? 1.5f : 1.2f);  // scale step multiplier

        auto &inst = SC.TI[0].I[selectedObjectIndex];

        // TRANSLATE in WORLD space (pre-multiply)
        // Arrows: X/Z, PageUp/PageDown: Y
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
            inst.Wm = glm::translate(glm::mat4(1), glm::vec3(-MOVE*dt, 0, 0)) * inst.Wm;
        }
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
            inst.Wm = glm::translate(glm::mat4(1), glm::vec3(MOVE*dt, 0, 0)) * inst.Wm;
        }
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
            inst.Wm = glm::translate(glm::mat4(1), glm::vec3(0, 0, -MOVE*dt)) * inst.Wm;
        }
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
            inst.Wm = glm::translate(glm::mat4(1), glm::vec3(0, 0, MOVE*dt)) * inst.Wm;
        }
        if (glfwGetKey(window, GLFW_KEY_PAGE_UP) == GLFW_PRESS) {
            inst.Wm = glm::translate(glm::mat4(1), glm::vec3(0, MOVE*dt, 0)) * inst.Wm;
        }
        if (glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS) {
            inst.Wm = glm::translate(glm::mat4(1), glm::vec3(0, -MOVE*dt, 0)) * inst.Wm;
        }

        // ROTATE about Y-axis
        if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) {
            inst.Wm = inst.Wm * glm::rotate(glm::mat4(1), ROT*dt, glm::vec3(0,1,0));
        }
        if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS) {
            inst.Wm = inst.Wm * glm::rotate(glm::mat4(1), -ROT*dt, glm::vec3(0,1,0));
        }

        // SCALE uniformly in LOCAL space (post-multiply)
        float step = std::pow(SCL, dt * 5.0f);

        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            // shrink
            inst.Wm = inst.Wm * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f / step));
        }

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            // grow
            inst.Wm = inst.Wm * glm::scale(glm::mat4(1.0f), glm::vec3(step));
        }
    }

    bool isVisibleAtListPos(int listPos) const {
        return (listPos >= 0 && listPos < (int)selectableIds.size() &&
                hiddenIds.count(selectableIds[listPos]) == 0);
    }

    std::string makeVisibleListString() const {
        std::string s;

        for (size_t i = 0; i < selectableIds.size(); ++i) {
            if (hiddenIds.count(selectableIds[i]) == 0) {
                s += selectableIds[i] + "\n";
            }
        }

        if (s.empty()) {
            s = "(no visible objects)";
        }

        return s;
    }

    bool selectNextVisible(int step = +1) {
        if (selectableIndices.empty()){
            selectedListPos = -1; selectedObjectIndex = -1; return false;
        }

        const int n = (int)selectableIndices.size();

        for (int k = 0; k < n; ++k) {
            selectedListPos = (selectedListPos + step + n) % n;

            if (hiddenIds.count(selectableIds[selectedListPos]) == 0) {
                selectedObjectIndex = selectableIndices[selectedListPos];
                return true;
            }
        }

        selectedListPos = -1; selectedObjectIndex = -1; return false;
    }
};

int main() {
    CG_hospital app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}