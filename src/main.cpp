#include "modules/Starter.hpp"

class CG_hospital : public BaseProject {
protected:
    void setWindowParameters() override {
        windowWidth = 1280;
        windowHeight = 720;
        windowTitle = "CG_hospital - Window Only";
        windowResizable = GLFW_TRUE;
    }

    void localInit() override {}
    void pipelinesAndDescriptorSetsInit() override {}
    void pipelinesAndDescriptorSetsCleanup() override {}
    void localCleanup() override {}
    void updateUniformBuffer(uint32_t currentImage) override {}

    void onWindowResize(int w, int h) override {}

    void populateCommandBuffer(VkCommandBuffer commandBuffer, int currentImage) {}

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
