#include "Engine/Render/SceneRenderer.hpp"

#include "Engine/EventSystem.hpp"
#include "Engine/Render/Vulkan/VulkanConfig.hpp"
#include "Engine/Render/Vulkan/VulkanContext.hpp"
#include "Engine/Render/RenderStages/ForwardStage.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace SceneRendererDetails
{}

SceneRenderer::SceneRenderer(EventSystem& aEventSystem, const VulkanContext& aVulkanContext)
    : vulkanContext{ &aVulkanContext }
    , eventSystem{ &aEventSystem }
{
    using namespace SceneRendererDetails;

    CreateRenderTargets();
    
    forwardStage = std::make_unique<ForwardStage>(*vulkanContext, renderContext);

    eventSystem->Subscribe<ES::BeforeSwapchainRecreated>(this, &SceneRenderer::OnBeforeSwapchainRecreated);
    eventSystem->Subscribe<ES::SwapchainRecreated>(this, &SceneRenderer::OnSwapchainRecreated);
    eventSystem->Subscribe<ES::TryReloadShaders>(this, &SceneRenderer::OnTryReloadShaders);
    eventSystem->Subscribe<ES::SceneOpened>(this, &SceneRenderer::OnSceneOpen);
    eventSystem->Subscribe<ES::SceneClosed>(this, &SceneRenderer::OnSceneClose);
}

SceneRenderer::~SceneRenderer()
{
    eventSystem->UnsubscribeAll(this);

    DestroyRenderTargets();
}

void SceneRenderer::Process(const float deltaSeconds)
{
    if (!scene) 
    {
        return;
    }

    const CameraComponent& camera = scene->GetCamera();

    renderContext.globals.view = camera.GetViewMatrix();
    renderContext.globals.projection = camera.GetProjectionMatrix();
    renderContext.globals.viewPos = camera.GetPosition();
}

void SceneRenderer::Render(const Frame& frame)
{
    if (!scene)
    {
        return;
    }

    forwardStage->Execute(frame);
}

void SceneRenderer::CreateRenderTargets()
{
    const Swapchain& swapchain = vulkanContext->GetSwapchain();
    const VkExtent2D swapchainExtent = swapchain.GetExtent();

    // Render targets
    ImageDescription colorTargetDescription = {
        .extent = { swapchainExtent.width, swapchainExtent.height, 1 },
        .mipLevelsCount = 1,
        .samples = vulkanContext->GetDevice().GetProperties().maxSampleCount,
        .format = swapchain.GetSurfaceFormat().format,
        .usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };
    
    ImageDescription depthTargetDescription = {
        .extent = { swapchainExtent.width, swapchainExtent.height, 1 },
        .mipLevelsCount = 1,
        .samples = vulkanContext->GetDevice().GetProperties().maxSampleCount,
        .format = VulkanConfig::depthImageFormat,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };
    
    renderContext.colorTarget = RenderTarget(colorTargetDescription, VK_IMAGE_ASPECT_COLOR_BIT, *vulkanContext);
    renderContext.depthTarget = RenderTarget(depthTargetDescription, VK_IMAGE_ASPECT_DEPTH_BIT, *vulkanContext);
}

void SceneRenderer::DestroyRenderTargets()
{
    renderContext.colorTarget = {};
    renderContext.depthTarget = {};
}

void SceneRenderer::OnBeforeSwapchainRecreated(const ES::BeforeSwapchainRecreated& event)
{
    DestroyRenderTargets();
}

void SceneRenderer::OnSwapchainRecreated(const ES::SwapchainRecreated& event)
{
    CreateRenderTargets();
    
    forwardStage->RecreateFramebuffers();
}

void SceneRenderer::OnTryReloadShaders(const ES::TryReloadShaders& event)
{
    forwardStage->TryReloadShaders();
}

void SceneRenderer::OnSceneOpen(const ES::SceneOpened& event)
{
    using namespace SceneRendererDetails;
    using namespace VulkanUtils;

    scene = &event.scene;
    
    renderContext.vertexBuffer = std::move(scene->vertexBuffer);
    renderContext.indexBuffer = std::move(scene->indexBuffer);
    renderContext.transformBuffer = std::move(scene->transformBuffer);
    renderContext.meshletDataBuffer = std::move(scene->meshletDataBuffer);
    renderContext.meshletBuffer = std::move(scene->meshletBuffer);
    renderContext.primitiveBuffer = std::move(scene->primitiveBuffer);
    renderContext.drawBuffer = std::move(scene->drawBuffer);
    renderContext.indirectBuffer = std::move(scene->indirectBuffer);
    
    renderContext.drawCount = scene->drawCount;
    renderContext.indirectDrawCount = scene->indirectDrawCount;
    
    renderContext.globalDescriptors = std::move(scene->globalDescriptors);
    renderContext.globalDescriptorSetLayout = scene->globalDescriptorSetLayout;
}

void SceneRenderer::OnSceneClose(const ES::SceneClosed& event)
{
    scene = nullptr;
}
