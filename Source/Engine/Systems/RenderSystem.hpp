#pragma once

#include <volk.h>

#include "Engine/Systems/System.hpp"
#include "Engine/Render/Vulkan/Frame.hpp"
#include "Engine/Render/RenderOptions.hpp"

namespace ES
{
    struct KeyInput;
}

class VulkanContext;
class Window;
class EventSystem;
class Renderer;

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
    
    void SetRenderer(RendererType rendererType);

    void OnKeyInput(const ES::KeyInput& event);

    const VulkanContext* vulkanContext = nullptr;
    RenderOptions* renderOptions = nullptr;

    EventSystem& eventSystem;

    std::unique_ptr<Renderer> sceneRenderer;
    std::unique_ptr<Renderer> computeRenderer;
    std::unique_ptr<Renderer> uiRenderer;
    
    std::vector<Frame> frames;
    uint32_t currentFrame = 0;
    
    RendererType rendererType;
    Renderer* renderer = nullptr;
};
