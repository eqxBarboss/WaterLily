#pragma once

#include <volk.h>

#include "Engine/Render/Renderer.hpp"
#include "Engine/Render/Vulkan/Pipeline.hpp"
#include "Engine/Render/Vulkan/Resources/Shaders/ShaderModule.hpp"
#include "Engine/Render/Vulkan/Resources/DescriptorSets/DescriptorSetLayout.hpp"

class VulkanContext;
class EventSystem;
class Image;
class ImageView;

namespace ES
{
    struct BeforeSwapchainRecreated;
    struct SwapchainRecreated;
    struct TryReloadShaders;
}

class ComputeRenderer : public Renderer
{
public:
    ComputeRenderer(EventSystem& eventSystem, const VulkanContext& vulkanContext);
    ~ComputeRenderer() override;

    ComputeRenderer(const ComputeRenderer&) = delete;
    ComputeRenderer& operator=(const ComputeRenderer&) = delete;

    ComputeRenderer(ComputeRenderer&&) = delete;
    ComputeRenderer& operator=(ComputeRenderer&&) = delete;

    void Process(float deltaSeconds) override;

    void Render(const Frame& frame) override;

private:
    void CreatePipeline(ShaderModule&& shaderModule);

    void CreateRenderTarget();
    void DestroyRenderTarget();

    void OnBeforeSwapchainRecreated(const ES::BeforeSwapchainRecreated& event);
    void OnSwapchainRecreated(const ES::SwapchainRecreated& event);
    void OnTryReloadShaders(const ES::TryReloadShaders& event);

    const VulkanContext* vulkanContext = nullptr;

    EventSystem* eventSystem = nullptr;

    Pipeline computePipeline;

    std::unique_ptr<Image> renderTarget;
    std::unique_ptr<ImageView> renderTargetView;

    std::vector<VkFramebuffer> framebuffers;

    VkDescriptorSet descriptor;
    DescriptorSetLayout layout;
};
