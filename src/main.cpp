#include <sstream>
#include <json.hpp>
#include "modules/Starter.hpp"
#include "modules/Scene.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

struct VertexSimp { glm::vec3 pos; glm::vec3 norm; glm::vec2 UV; };

struct GlobalUBO {
    alignas(16) glm::vec3 lightDir;
    alignas(16) glm::vec4 lightColor;
    alignas(16) glm::vec3 eyePos;
};

struct LocalUBO {
    alignas(16) glm::mat4 mvpMat;
    alignas(16) glm::mat4 mMat;
    alignas(16) glm::mat4 nMat;
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
        SC.init(this, 1, VDRs, PRs, "assets/models/sceneK.json");

        // After SC.init(...)
        if (SC.TechniqueInstanceCount > 0 && SC.TI[0].InstanceCount > 0) {
            for (int i = 0; i < SC.TI[0].InstanceCount; ++i) {
                auto &inst = SC.TI[0].I[i];
                // Scale it down and push it in front of the camera
                inst.Wm = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.2f, -5.0f))
                        * glm::scale(glm::mat4(1.0f),    glm::vec3(0.1f)); // adjust 0.1f as needed
            }
        }
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

            // --- Inputs (same getSixAxis) ---
            float dt = 0.0f; glm::vec3 m(0.0f), r(0.0f); bool fire = false;
            getSixAxis(dt, m, r, fire);

            static float yaw   = 0.0f;                 // radians
            static float pitch = 0.0f;
            static glm::vec3 camPos = glm::vec3(0.0f, 1.6f, 6.0f);

            // Match E09 speeds
            const float ROT_SPEED         = glm::radians(120.0f);  // rad/sec
            const float MOVE_SPEED_BASE   = 10.0f;                  // m/s
            const float MOVE_SPEED_RUN    = 10.0f;                  // m/s
            const float MOVE_SPEED        = fire ? MOVE_SPEED_RUN : MOVE_SPEED_BASE;

            // NOTE: E09 maps yaw<-r.y, pitch<-r.x (your version had them swapped)
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

            // --- Local UBOs for all instances in technique 0 ---
            for (int i = 0; i < SC.TI[0].InstanceCount; ++i) {
                auto &inst = SC.TI[0].I[i];

                LocalUBO l{};
                l.mMat   = inst.Wm;
                l.nMat   = glm::inverse(glm::transpose(l.mMat));
                l.mvpMat = Prj * View * l.mMat;

                inst.DS[0][0]->map(currentImage, &g, 0); // set=0, binding 0 → global UBO
                inst.DS[0][1]->map(currentImage, &l, 0); // set=1, binding 0 → local UBO
            }
    }



    static void populateCommandBufferAccess(VkCommandBuffer commandBuffer, int currentImage, void *params) {
        auto *app = reinterpret_cast<CG_hospital*>(params);
        app->populateCommandBuffer(commandBuffer, currentImage);
    }
};

int main() {
    CG_hospital app;
    try { app.run(); }
    catch (const std::exception& e) { std::cerr << e.what() << std::endl; return EXIT_FAILURE; }
    return EXIT_SUCCESS;
}
