#pragma once

#include "Engine/Render/Renderer.hpp"
#include "Engine/Render/RenderContext.hpp"

class VulkanContext;
class EventSystem;
class RenderStage;
class Scene;

namespace ES
{
    struct BeforeSwapchainRecreated;
    struct SwapchainRecreated;
    struct TryReloadShaders;
    struct SceneOpened;
    struct SceneClosed;
}

class SceneRenderer : public Renderer
{
public:
    SceneRenderer(EventSystem& eventSystem, const VulkanContext& vulkanContext);
    ~SceneRenderer() override;

    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;

    SceneRenderer(SceneRenderer&&) = delete;
    SceneRenderer& operator=(SceneRenderer&&) = delete;

    void Process(float deltaSeconds) override;

    void Render(const Frame& frame) override;

private:
    void CreateRenderTargets();
    void DestroyRenderTargets();

    void OnBeforeSwapchainRecreated(const ES::BeforeSwapchainRecreated& event);
    void OnSwapchainRecreated(const ES::SwapchainRecreated& event);
    void OnTryReloadShaders(const ES::TryReloadShaders& event);
    void OnSceneOpen(const ES::SceneOpened& event);
    void OnSceneClose(const ES::SceneClosed& event);

    const VulkanContext* vulkanContext = nullptr;

    EventSystem* eventSystem = nullptr;
    
    RenderContext renderContext;
    
    std::unique_ptr<RenderStage> forwardStage;

    Scene* scene = nullptr;
};
