#pragma once

#include <volk.h>

#include "Engine/Render/Frame.hpp"
#include "Engine/Systems/System.hpp"
#include "Engine/Render/Vulkan/RenderPass.hpp"

namespace ES
{
    struct KeyInput;
}

class VulkanContext;
class Window;
class EventSystem;
class SceneRenderer;
class ComputeRenderer;
class UiRenderer;

class RenderSystem : public System
{
public:
    RenderSystem(const Window& window, EventSystem& aEventSystem, const VulkanContext& vulkanContext);
    ~RenderSystem() override;

    RenderSystem(const RenderSystem&) = delete;
    RenderSystem& operator=(const RenderSystem&) = delete;

    RenderSystem(RenderSystem&&) = delete;
    RenderSystem& operator=(RenderSystem&&) = delete;

    void Process(float deltaSeconds) override;

    void Render();

private:
    uint32_t AcquireNextSwapchainImage(VkSemaphore signalSemaphore) const;
    void Present(const std::vector<VkSemaphore>& waitSemaphores, uint32_t imageIndex) const;

    void OnKeyInput(const ES::KeyInput& event);

    const VulkanContext& vulkanContext;

    EventSystem& eventSystem;

    std::unique_ptr<SceneRenderer> sceneRenderer;
    std::unique_ptr<ComputeRenderer> computeRenderer;
    std::unique_ptr<UiRenderer> uiRenderer;
    
    std::vector<Frame> frames;
    uint32_t currentFrame = 0;
};
