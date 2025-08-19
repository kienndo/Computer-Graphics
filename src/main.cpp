#include <sstream>
#include <json.hpp>
#include "modules/Starter.hpp"
#include <glm/gtc/matrix_transform.hpp>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>



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
    alignas(4) float amb;
    alignas(4) float gamma;
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
    Pipeline         PMesh;          // lit pipeline

    DescriptorSet    DSGubo, DSObj;
    Model   MObj;
    Texture TObj;

    float CamH, CamRadius, CamPitch, CamYaw;

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

        DSLMesh.init(this, {
            { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_SHADER_STAGE_ALL_GRAPHICS,
            sizeof(UBOLocal), 1 },
            {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT,0,1 }
        });

        DSLGubo.init(this, {
            { 0,
              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS,
              sizeof(UBOGlobal),1 }
        });

        VMesh.init(this,
    { { 0, sizeof(VD), VK_VERTEX_INPUT_RATE_VERTEX } },
    {
        { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VD, pos),  sizeof(glm::vec3), POSITION },
        { 0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VD, norm), sizeof(glm::vec3), NORMAL   },
        { 0, 2, VK_FORMAT_R32G32_SFLOAT,    offsetof(VD, uv),   sizeof(glm::vec2), UV       }
        }
        );

        RP.init(this);

        PMesh.init(this, &VMesh,
                   "shaders/Mesh.vert.spv", "shaders/Mesh.frag.spv",
                   { &DSLGubo, &DSLMesh });
        PMesh.setCullMode(VK_CULL_MODE_NONE);


        MObj.init(this, &VMesh, "assets/models/M_Aircondition_01.mgcg", MGCG);
        TObj.init(this, "assets/textures/T_Aircondition_01.png");

        DPSZs.uniformBlocksInPool = 8; //
        DPSZs.texturesInPool      = 4; //
        DPSZs.setsInPool          = 8; //

        CamH = 1.0f;
        CamRadius = 3.0f;
        CamPitch = glm::radians(15.0f);
        CamYaw = glm::radians(30.0f);

    }

    void pipelinesAndDescriptorSetsInit() {

        RP.create();
        PMesh.create(&RP);

        DSObj.init(this, &DSLMesh, { TObj.getViewAndSampler() });
        DSGubo.init(this, &DSLGubo, {});

        submitCommandBuffer("main", 0, populateCommandBufferAccess, this);

    }

    void pipelinesAndDescriptorSetsCleanup(){
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

    void populateCommandBuffer(VkCommandBuffer cmdBuffer, int currentImage) {
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


        // ---- Camera (E06-ish orbit or fixed) ----
        const float FOVy = glm::radians(60.0f);
        glm::mat4 Prj = glm::perspective(FOVy, Ar, 0.1f, 100.0f);
        Prj[1][1] *= -1.0f; // Vulkan Y flip

        // simple fixed camera looking down -Z
        glm::vec3 eye = glm::vec3(0.0f, 1.4f, 4.0f);
        glm::mat4 View = glm::lookAt(eye, eye + glm::vec3(0,0,-1), glm::vec3(0,1,0));

        // object at origin
        glm::mat4 World = glm::scale(glm::mat4(1.0f), glm::vec3(0.01f)); // or 10.0f

        // ---- Global UBO (set=0, binding=0) ----
        UBOGlobal g{};
        g.lightDir      = glm::normalize(glm::vec3(1,2,3));
        g.lightColor    = glm::vec4(1,1,1,1);
        g.ambLightColor = glm::vec3(0.1f);
        g.eyePos        = eye;
        DSGubo.map(currentImage, &g, /*binding*/0);

        // ---- Local UBO (set=1, binding=0) ----
        UBOLocal l{};
        l.amb    = 1.0f;
        l.gamma  = 180.0f;
        l.sColor = glm::vec3(1.0f);
        l.mMat   = World;
        l.nMat   = glm::inverse(glm::transpose(World));
        l.mvpMat = Prj * View * World;
        DSObj.map(currentImage, &l, /*binding*/0);

    }

    // Wrapper used by BaseProject to fill command buffers
    static void populateCommandBufferAccess(VkCommandBuffer commandBuffer,
                                            int currentImage,
                                            void *params) {
        auto *app = reinterpret_cast<CG_hospital*>(params);
        app->populateCommandBuffer(commandBuffer, currentImage);
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
