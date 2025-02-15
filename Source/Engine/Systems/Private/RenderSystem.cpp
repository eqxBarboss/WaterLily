#include "Engine/Systems/RenderSystem.hpp"

#include "Engine/Engine.hpp"
#include "Engine/EventSystem.hpp"
#include "Engine/Render/Vulkan/VulkanContext.hpp"
#include "Engine/Render/Vulkan/VulkanHelpers.hpp"
#include "Engine/Render/Resources/Image.hpp"
#include "Engine/Render/Resources/ImageView.hpp"
#include "Shaders/Common.h"

#include <glm/gtc/matrix_transform.hpp>

namespace RenderSystemDetails
{

}

RenderSystem::RenderSystem(EventSystem& aEventSystem, const VulkanContext& aVulkanContext)
    : vulkanContext{aVulkanContext}
    , device{vulkanContext.GetDevice()}
    , eventSystem{aEventSystem}
    , renderPass{std::make_unique<RenderPass>(vulkanContext)}
    , graphicsPipeline{std::make_unique<GraphicsPipeline>(*renderPass, vulkanContext)}
{
    using namespace VulkanHelpers;
    using namespace VulkanConfig;

    CreateAttachmentsAndFramebuffers();
    
    frames = CreateFrames(maxFramesInFlight, vulkanContext);

    descriptorPool = CreateDescriptorPool(maxFramesInFlight, maxFramesInFlight, device.GetVkDevice());

    eventSystem.Subscribe<ES::WindowResized>(this, &RenderSystem::OnResize);
    eventSystem.Subscribe<ES::SceneOpened>(this, &RenderSystem::OnSceneOpen);
    eventSystem.Subscribe<ES::SceneClosed>(this, &RenderSystem::OnSceneClose);
    eventSystem.Subscribe<ES::BeforeWindowRecreated>(this, &RenderSystem::OnBeforeWindowRecreated, ES::Priority::eHigh);
    eventSystem.Subscribe<ES::WindowRecreated>(this, &RenderSystem::OnWindowRecreated, ES::Priority::eLow);
}

RenderSystem::~RenderSystem()
{
    eventSystem.Unsubscribe<ES::WindowResized>(this);
    eventSystem.Unsubscribe<ES::SceneOpened>(this);
    eventSystem.Unsubscribe<ES::SceneClosed>(this);
    eventSystem.Unsubscribe<ES::BeforeWindowRecreated>(this);
    eventSystem.Unsubscribe<ES::WindowRecreated>(this);

    DestroyAttachmentsAndFramebuffers();

    vkDestroyDescriptorPool(device.GetVkDevice(), descriptorPool, nullptr);
}

void RenderSystem::Process(float deltaSeconds)
{
    if (!scene)
    {
        return;
    }

    const CameraComponent& camera = scene->GetCamera();

    ubo.view = camera.GetViewMatrix();
    ubo.projection = camera.GetProjectionMatrix();
    ubo.viewPos = camera.GetPosition();
}

void RenderSystem::Render()
{
    using namespace RenderSystemDetails;
    using namespace VulkanHelpers;

    if (!scene)
    {
        return;
    }
    
    Frame& frame = frames[currentFrame];

    const auto& [waitSemaphores, waitStages, signalSemaphores, fence] = frame.GetSync().AsTuple();

    // Wait for frame to finish execution and signal fence
    vkWaitForFences(device.GetVkDevice(), 1, &fence, VK_TRUE, UINT64_MAX);
    vkResetFences(device.GetVkDevice(), 1, &fence);

    // Acquire next image from the swapchain, frame wait semaphore will be signaled by the presentation engine when it
    // finishes using the image so we can start rendering
    const uint32_t imageIndex = AcquireNextImage(frame);

    // Fill frame data
    frame.GetUniformBuffer().Fill(std::span{ reinterpret_cast<const std::byte*>(&ubo), sizeof(ubo) });

    // Submit scene rendering commands
    const auto renderCommands = [&](VkCommandBuffer buffer) { RenderScene(frame, framebuffers[imageIndex]); };
    SubmitCommandBuffer(frame.GetCommandBuffer(), device.GetQueues().graphics, renderCommands, frame.GetSync());

    // Present will happen when rendering is finished and the frame signal semaphore is signaled
    Present(frame, imageIndex);

    currentFrame = (currentFrame + 1) % VulkanConfig::maxFramesInFlight;
}

uint32_t RenderSystem::AcquireNextImage(const Frame& frame) const
{
    const VkSwapchainKHR swapchain = vulkanContext.GetSwapchain().GetVkSwapchainKHR();

    uint32_t imageIndex;
    const VkResult acquireResult = vkAcquireNextImageKHR(device.GetVkDevice(), swapchain, 
        std::numeric_limits<uint64_t>::max(), frame.GetSync().GetWaitSemaphores()[0], VK_NULL_HANDLE, &imageIndex);
    Assert(acquireResult == VK_SUCCESS || acquireResult == VK_SUBOPTIMAL_KHR);

    return imageIndex;
}

void RenderSystem::RenderScene(const Frame& frame, const VkFramebuffer framebuffer) const
{
    using namespace VulkanHelpers;

    const VkCommandBuffer commandBuffer = frame.GetCommandBuffer();
    const VkDescriptorSet descriptorSet = frame.GetDescriptorSet();

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass->GetVkRenderPass();
    renderPassInfo.framebuffer = framebuffer;
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = vulkanContext.GetSwapchain().GetExtent();

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { { 0.73f, 0.95f, 1.0f, 1.0f } };
    clearValues[1].depthStencil = { 1.0f, 0 };

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->GetVkPipeline());

    const VkExtent2D extent = vulkanContext.GetSwapchain().GetExtent();
    VkViewport viewport = GetViewport(extent);
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = GetScissor(extent);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    VkBuffer vertexBuffers[] = { scene->GetVertexBuffer().GetVkBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer, scene->GetIndexBuffer().GetVkBuffer(), 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->GetPipelineLayout(),
        0, 1, &descriptorSet, 0, nullptr);

    vkCmdDrawIndexedIndirect(commandBuffer, indirectBuffer->GetVkBuffer(), 0, indirectDrawCount,
        sizeof(VkDrawIndexedIndirectCommand));

    vkCmdEndRenderPass(commandBuffer);
}

void RenderSystem::Present(const Frame& frame, const uint32_t imageIndex) const
{
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    const std::vector<VkSemaphore> signalSemaphores = frame.GetSync().GetSignalSemaphores();
    presentInfo.waitSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
    presentInfo.pWaitSemaphores = signalSemaphores.data();

    VkSwapchainKHR swapChains[] = { vulkanContext.GetSwapchain().GetVkSwapchainKHR() };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    const VkResult presentResult = vkQueuePresentKHR(device.GetQueues().present, &presentInfo);
    Assert(presentResult == VK_SUCCESS);
}

void RenderSystem::CreateAttachmentsAndFramebuffers()
{
    using namespace VulkanHelpers;

    device.WaitIdle();

    const Swapchain& swapchain = vulkanContext.GetSwapchain();

    colorAttachment = CreateColorAttachment(swapchain.GetExtent(), vulkanContext);
    colorAttachmentView = std::make_unique<ImageView>(*colorAttachment, VK_IMAGE_ASPECT_COLOR_BIT, &vulkanContext);

    depthAttachment = CreateDepthAttachment(swapchain.GetExtent(), vulkanContext);
    depthAttachmentView = std::make_unique<ImageView>(*depthAttachment, VK_IMAGE_ASPECT_DEPTH_BIT, &vulkanContext);

    framebuffers = CreateFramebuffers(swapchain, *colorAttachmentView, *depthAttachmentView,
        renderPass->GetVkRenderPass(), device.GetVkDevice());
}

void RenderSystem::DestroyAttachmentsAndFramebuffers()
{
    using namespace VulkanHelpers;

    device.WaitIdle();

    DestroyFramebuffers(framebuffers, device.GetVkDevice());

    colorAttachmentView.reset();
    colorAttachment.reset();

    depthAttachmentView.reset();
    depthAttachment.reset();
}

void RenderSystem::OnResize(const ES::WindowResized& event)
{
    if (event.newExtent.width == 0 || event.newExtent.height == 0)
    {
        return;
    }

    DestroyAttachmentsAndFramebuffers();
    vulkanContext.GetSwapchain().Recreate(event.newExtent);
    CreateAttachmentsAndFramebuffers();
}

void RenderSystem::OnBeforeWindowRecreated(const ES::BeforeWindowRecreated& event)
{
    DestroyAttachmentsAndFramebuffers();
}

void RenderSystem::OnWindowRecreated(const ES::WindowRecreated& event)
{
    CreateAttachmentsAndFramebuffers();
}

void RenderSystem::OnSceneOpen(const ES::SceneOpened& event)
{
    using namespace VulkanConfig;
    using namespace VulkanHelpers;

    scene = &event.scene;

    VkDescriptorSetLayout layout = graphicsPipeline->GetDescriptorSetLayouts()[0];
    
    std::vector<VkDescriptorSet> descriptorSets = CreateDescriptorSets(descriptorPool, layout, maxFramesInFlight, 
        device.GetVkDevice());
    
    for (size_t i = 0; i < maxFramesInFlight; ++i)
    {
        PopulateDescriptorSet(descriptorSets[i], frames[i].GetUniformBuffer(), *scene, device.GetVkDevice());
        frames[i].SetDescriptorSet(descriptorSets[i]);
    }

    std::tie(indirectBuffer, indirectDrawCount) = CreateIndirectBuffer(*scene, vulkanContext);
}

void RenderSystem::OnSceneClose(const ES::SceneClosed& event)
{    
    device.WaitIdle();
    
    for (Frame& frame : frames)
    {
        frame.SetDescriptorSet(VK_NULL_HANDLE);
    }
    
    vkResetDescriptorPool(device.GetVkDevice(), descriptorPool, 0);
    
    scene = nullptr;
}
