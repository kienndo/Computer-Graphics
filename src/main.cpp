#include <sstream>
#include <json.hpp>
#include "modules/Starter.hpp"

// GLM config must be before GLM headers
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct VD {
    glm::vec3 pos;
    glm::vec3 norm;
    glm::vec2 uv;
};

struct UBOGlobal {
    alignas(16) glm::vec3 lightDir;
    alignas(16) glm::vec4 lightColor;
    alignas(16) glm::vec3 ambLightColor;
    alignas(16) glm::vec3 eyePos;
};

struct UBOLocal {
    alignas(4)  float     amb;
    alignas(4)  float     gamma;
    alignas(16) glm::vec3 sColor;
    alignas(16) glm::mat4 mvpMat;
    alignas(16) glm::mat4 mMat;
    alignas(16) glm::mat4 nMat;
};

class CG_hospital : public BaseProject {
protected:
    float Ar = 4.0f/3.0f;

    RenderPass RP;
    DescriptorSetLayout DSLGubo, DSLMesh;

    VertexDescriptor VMesh;
    Pipeline         PMesh;

    DescriptorSet    DSGubo, DSObj;
    Model            MObj;
    Texture          TObj;

    // --- Camera (simple fixed cam) ---
    glm::vec3 camPos{0.0f, 1.4f, 4.0f};

    // --- Object transform state (controlled by keyboard) ---
    glm::vec3 objPos   {0.0f, 0.0f, 0.0f};
    float     objYaw   = 0.0f; // around Y
    float     objPitch = 0.0f; // around X
    float     objRoll  = 0.0f; // around Z
    float     objScale = 0.01f;

    // Speeds
    float MOVE_SPEED = 2.0f;                 // meters/sec
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
        DSLMesh.init(this, {
            { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,    VK_SHADER_STAGE_ALL_GRAPHICS, sizeof(UBOLocal), 1 },
            { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 1 }
        });
        DSLGubo.init(this, {
            { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,    VK_SHADER_STAGE_ALL_GRAPHICS, sizeof(UBOGlobal), 1 }
        });

        // Vertex layout
        VMesh.init(this,
            { { 0, sizeof(VD), VK_VERTEX_INPUT_RATE_VERTEX } },
            {
                { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VD, pos),  sizeof(glm::vec3), POSITION },
                { 0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VD, norm), sizeof(glm::vec3), NORMAL   },
                { 0, 2, VK_FORMAT_R32G32_SFLOAT,    offsetof(VD, uv),   sizeof(glm::vec2), UV       }
            }
        );

        RP.init(this);

        PMesh.init(this, &VMesh, "shaders/Mesh.vert.spv", "shaders/Mesh.frag.spv", { &DSLGubo, &DSLMesh });
        PMesh.setCullMode(VK_CULL_MODE_NONE); // easier while testing placement

        // Assets
        MObj.init(this, &VMesh, "assets/models/M_Aircondition_01.mgcg", MGCG);
        TObj.init(this, "assets/textures/T_Aircondition_01.png");

        // Pool sizing
        DPSZs.uniformBlocksInPool = 8;
        DPSZs.texturesInPool      = 4;
        DPSZs.setsInPool          = 8;
    }

    void pipelinesAndDescriptorSetsInit() {
        RP.create();
        PMesh.create(&RP);

        DSObj.init(this, &DSLMesh, { TObj.getViewAndSampler() });
        DSGubo.init(this, &DSLGubo, {});

        // Register CB filler
        submitCommandBuffer("main", 0, populateCommandBufferAccess, this);
    }

    void pipelinesAndDescriptorSetsCleanup() {
        PMesh.cleanup();
        RP.cleanup();
        DSGubo.cleanup();
        DSObj.cleanup();
    }

    void localCleanup() {
        TObj.cleanup();
        MObj.cleanup();
        DSLGubo.cleanup();
        DSLMesh.cleanup();
        PMesh.destroy();
        RP.destroy();
    }

    void populateCommandBuffer(VkCommandBuffer cmdBuffer, int currentImage){
        RP.begin(cmdBuffer, currentImage);

        PMesh.bind(cmdBuffer);
        DSGubo.bind(cmdBuffer, PMesh, 0, currentImage);
        MObj.bind(cmdBuffer);
        DSObj.bind(cmdBuffer, PMesh, 1, currentImage);

        vkCmdDrawIndexed(cmdBuffer, static_cast<uint32_t>(MObj.indices.size()), 1, 0, 0, 0);
        RP.end(cmdBuffer);
    }

    void updateUniformBuffer(uint32_t currentImage) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE)) {
            glfwSetWindowShouldClose(window, GL_TRUE);
        }

        // --- Input axes from Starter.hpp (WASD/RF for move, arrows/QE for rotate)
        float deltaT = 0.0f;
        glm::vec3 m(0.0f), r(0.0f);
        bool fire = false;
        getSixAxis(deltaT, m, r, fire);

        objYaw   -= r.x * ROT_SPEED * deltaT;
        objPitch -= r.y * ROT_SPEED * deltaT;
        objRoll  -= r.z * ROT_SPEED * deltaT;

        objPitch = glm::clamp(objPitch, glm::radians(-89.0f), glm::radians(89.0f));

        // --- Build rotation (Ry * Rx * Rz) for local axes ---
        glm::mat4 Rx = glm::rotate(glm::mat4(1.0f), objPitch, glm::vec3(1,0,0));
        glm::mat4 Ry = glm::rotate(glm::mat4(1.0f), objYaw,   glm::vec3(0,1,0));
        glm::mat4 Rz = glm::rotate(glm::mat4(1.0f), objRoll,  glm::vec3(0,0,1));
        glm::mat4 R  = Ry * Rx * Rz;

        // Local basis vectors (forward/right/up) from rotation
        glm::vec3 fwd = glm::vec3(R * glm::vec4(0,0,-1,0));
        glm::vec3 rgt = glm::vec3(R * glm::vec4(1,0, 0,0));
        glm::vec3 up  = glm::vec3(R * glm::vec4(0,1, 0,0));

        // Update position (move in object-local space)
        objPos += rgt * (m.x * MOVE_SPEED * deltaT);   // A/D
        objPos += up  * (m.y * MOVE_SPEED * deltaT);   // R/F
        objPos += fwd * (m.z * MOVE_SPEED * deltaT);   // W/S f

        // Camera (fixed)
        const float FOVy = glm::radians(60.0f);
        glm::mat4 Prj  = glm::perspective(FOVy, Ar, 0.1f, 100.0f);
        Prj[1][1] *= -1.0f;
        glm::mat4 View = glm::lookAt(camPos, camPos + glm::vec3(0,0,-1), glm::vec3(0,1,0));

        // Object world transform: T * R * S
        glm::mat4 T = glm::translate(glm::mat4(1.0f), objPos);
        glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(objScale));
        glm::mat4 World = T * R * S;

        // Global UBO (set=0, binding=0)
        UBOGlobal g{};
        g.lightDir      = glm::normalize(glm::vec3(1,2,3));
        g.lightColor    = glm::vec4(1,1,1,1);
        g.ambLightColor = glm::vec3(0.1f);
        g.eyePos        = camPos;
        DSGubo.map(currentImage, &g, 0);

        // Local UBO (set=1, binding=0)
        UBOLocal l{};
        l.amb    = 1.0f;
        l.gamma  = 180.0f;
        l.sColor = glm::vec3(1.0f);
        l.mMat   = World;
        l.nMat   = glm::inverse(glm::transpose(World));
        l.mvpMat = Prj * View * World;
        DSObj.map(currentImage, &l, 0);
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
