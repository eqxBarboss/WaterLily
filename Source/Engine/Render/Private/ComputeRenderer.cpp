#include "Engine/Render/ComputeRenderer.hpp"

#include "Utils/Constants.hpp"
#include "Engine/EventSystem.hpp"
#include "Engine/Render/Resources/Image.hpp"
#include "Engine/Render/Resources/ImageView.hpp"
#include "Engine/Render/Vulkan/VulkanConfig.hpp"
#include "Engine/Render/Vulkan/VulkanContext.hpp"
#include "Engine/Render/Resources/ResourceHelpers.hpp"
#include "Engine/Render/Resources/Pipelines/ComputePipelineBuilder.hpp"

namespace ComputeRendererDetails
{
    static constexpr std::string_view shaderPath = "~/Shaders/gradient.comp";
}

ComputeRenderer::ComputeRenderer(EventSystem& aEventSystem, const VulkanContext& aVulkanContext)
    : vulkanContext{ &aVulkanContext }
    , eventSystem{ &aEventSystem }
{
    using namespace ComputeRendererDetails;

    CreateRenderTarget();
    
    const ShaderManager& shaderManager = vulkanContext->GetShaderManager();
    
    ShaderModule shaderModule = shaderManager.CreateShaderModule(FilePath(shaderPath), ShaderType::eCompute);
    Assert(shaderModule.IsValid());

    CreatePipeline(std::move(shaderModule));

    eventSystem->Subscribe<ES::BeforeSwapchainRecreated>(this, &ComputeRenderer::OnBeforeSwapchainRecreated);
    eventSystem->Subscribe<ES::SwapchainRecreated>(this, &ComputeRenderer::OnSwapchainRecreated);
    eventSystem->Subscribe<ES::TryReloadShaders>(this, &ComputeRenderer::OnTryReloadShaders);
}

ComputeRenderer::~ComputeRenderer()
{
    eventSystem->UnsubscribeAll(this);

    DestroyRenderTarget();
}

void ComputeRenderer::Process(const float deltaSeconds)
{
    
}

void ComputeRenderer::Render(const Frame& frame)
{
//    using namespace VulkanHelpers;
//
//    if (!scene)
//    {
//        return;
//    }
//
//    uniformBuffers[frame.index].Fill(std::as_bytes(std::span(&ubo, 1)));
//
//    VkRenderPassBeginInfo renderPassInfo{};
//    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
//    renderPassInfo.renderPass = renderPass.GetVkRenderPass();
//    renderPassInfo.framebuffer = framebuffers[frame.swapchainImageIndex];
//    renderPassInfo.renderArea.offset = { 0, 0 };
//    renderPassInfo.renderArea.extent = vulkanContext->GetSwapchain().GetExtent();
//
//    std::array<VkClearValue, 3> clearValues{};
//    clearValues[0].color = { { 0.73f, 0.95f, 1.0f, 1.0f } };
//    clearValues[2].depthStencil = { 1.0f, 0 };
//
//    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
//    renderPassInfo.pClearValues = clearValues.data();
//    
//    const VkCommandBuffer commandBuffer = frame.commandBuffer;
//
//    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
//    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.Get());
//
//    const VkExtent2D extent = vulkanContext->GetSwapchain().GetExtent();
//    VkViewport viewport = GetViewport(static_cast<float>(extent.width), static_cast<float>(extent.height));
//    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
//
//    const VkRect2D scissor = GetScissor(extent);
//    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
//
//    VkBuffer vertexBuffers[] = { scene->GetVertexBuffer().GetVkBuffer() };
//    VkDeviceSize offsets[] = { 0 };
//    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
//
//    vkCmdBindIndexBuffer(commandBuffer, scene->GetIndexBuffer().GetVkBuffer(), 0, VK_INDEX_TYPE_UINT32);
//
//    std::vector<VkDescriptorSet> descriptors = { uniformDescriptors[frame.index] };
//    std::ranges::copy(scene->GetGlobalDescriptors(), std::back_inserter(descriptors));
//
//    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.GetLayout(),
//        0, static_cast<uint32_t>(descriptors.size()), descriptors.data(), 0, nullptr);
//
//    vkCmdDrawIndexedIndirect(commandBuffer, indirectBuffer->GetVkBuffer(), 0, indirectDrawCount,
//        sizeof(VkDrawIndexedIndirectCommand));
//
//    vkCmdEndRenderPass(commandBuffer);
    
    using namespace ResourceHelpers;
    
    const VkCommandBuffer commandBuffer = frame.commandBuffer;
    const Swapchain& swapchain = vulkanContext->GetSwapchain();
    const VkImage swapchainImage = swapchain.GetImages()[frame.swapchainImageIndex];
    const VkExtent3D swapchainExtent = { swapchain.GetExtent().width, swapchain.GetExtent().height, 1 };
    
//vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
//
//draw_background(cmd);
//
////transition the draw image and the swapchain image into their correct transfer layouts
//vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
//vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
//
//// execute a copy from the draw image into the swapchain
//vkutil::copy_image_to_image(cmd, _drawImage.image, _swapchainImages[swapchainImageIndex], _drawExtent, _swapchainExtent);
//
//// set swapchain image layout to Present so we can show it on the screen
//vkutil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
//
////finalize the command buffer (we can no longer add commands, but it can now be executed)
//VK_CHECK(vkEndCommandBuffer(cmd));
    
    using namespace ImageLayoutTransitions;

    TransitionLayout(commandBuffer, *renderTarget, undefinedToGeneral);
    FillImage(commandBuffer, *renderTarget, Color::magenta);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.Get());
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.GetLayout(), 0, 1, &descriptor, 0, nullptr);
    vkCmdDispatch(commandBuffer, std::ceil(swapchainExtent.width / 16.0), std::ceil(swapchainExtent.height / 16.0), 1);
    
    TransitionLayout(commandBuffer, *renderTarget, generalToSrcOptimal);
    
    TransitionLayout(commandBuffer, swapchainImage, undefinedToDstOptimal, 0);
    
    BlitImageToImage(commandBuffer, *renderTarget, swapchainImage, swapchainExtent);
}

void ComputeRenderer::CreatePipeline(ShaderModule&& shaderModule)
{
    using namespace VulkanHelpers;

    std::vector layouts = { layout.Get() };
    
    computePipeline = ComputePipelineBuilder(*vulkanContext)
        .SetDescriptorSetLayouts(std::move(layouts))
        .SetShaderModule(std::move(shaderModule))
        .Build();
}

void ComputeRenderer::CreateRenderTarget()
{
    using namespace VulkanHelpers;
    
    // Let this target match window size
    const VkExtent2D extent = vulkanContext->GetSwapchain().GetExtent();
    
    ImageDescription imageDescription{
        .extent = { extent.width, extent.height, 1 },
        .mipLevelsCount = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
        .memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };

    // TODO: Render target class? Add layout inside to track it?
    renderTarget = std::make_unique<Image>(imageDescription, *vulkanContext);
    renderTargetView = std::make_unique<ImageView>(*renderTarget, VK_IMAGE_ASPECT_COLOR_BIT, *vulkanContext);
    
    // TODO: LEAK! Now we don't clear the pool and each resize adds descriptors to it
    std::tie(descriptor, layout) = vulkanContext->GetDescriptorSetsManager().GetDescriptorSetBuilder()
        .Bind(0, *renderTargetView, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
        .Build();
}

void ComputeRenderer::DestroyRenderTarget()
{
    using namespace VulkanHelpers;
    
    renderTargetView.reset();
    renderTarget.reset();
}

void ComputeRenderer::OnBeforeSwapchainRecreated(const ES::BeforeSwapchainRecreated& event)
{
    DestroyRenderTarget();
}

void ComputeRenderer::OnSwapchainRecreated(const ES::SwapchainRecreated& event)
{
    CreateRenderTarget();
}

void ComputeRenderer::OnTryReloadShaders(const ES::TryReloadShaders& event)
{
    using namespace ComputeRendererDetails;
    
    const ShaderManager& shaderManager = vulkanContext->GetShaderManager();

    if (ShaderModule shaderModule = shaderManager.CreateShaderModule(FilePath(shaderPath), ShaderType::eCompute);
        shaderModule.IsValid())
    {
        CreatePipeline(std::move(shaderModule));
    }
}
