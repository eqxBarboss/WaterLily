#include "Engine/Systems/RenderSystem.hpp"

#include "Engine/EventSystem.hpp"
#include "Engine/Render/SceneRenderer.hpp"
#include "Engine/Render/Ui/UiRenderer.hpp"
#include "Engine/Render/ComputeRenderer.hpp"
#include "Engine/Render/Vulkan/VulkanContext.hpp"
#include "Engine/Render/Vulkan/VulkanHelpers.hpp"

namespace RenderSystemDetails
{
    static CommandBufferSync CreateFrameSync(const VulkanContext& vulkanContext)
    {
        using namespace VulkanHelpers;
        
        const VkDevice device = vulkanContext.GetDevice().GetVkDevice();
        
        std::vector<VkSemaphore> waitSemaphores = { CreateSemaphore(device) };
        std::vector<VkPipelineStageFlags> waitStages = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        std::vector<VkSemaphore> signalSemaphores = { CreateSemaphore(device) };
        VkFence fence = CreateFence(device, VK_FENCE_CREATE_SIGNALED_BIT);
        
        return { waitSemaphores, waitStages, signalSemaphores, fence, device };
    }

    static VkCommandBuffer CreateCommandBuffer(const VulkanContext& vulkanContext)
    {
        const Device& device = vulkanContext.GetDevice();
        const VkCommandPool longLivedPool = device.GetCommandPool(CommandBufferType::eLongLived);
        
        return VulkanHelpers::CreateCommandBuffers(device.GetVkDevice(), 1, longLivedPool)[0];
    }
}

RenderSystem::RenderSystem(const Window& window, EventSystem& aEventSystem, const VulkanContext& aVulkanContext)
    : vulkanContext{ aVulkanContext }
    , eventSystem{ aEventSystem }
    , sceneRenderer{ std::make_unique<SceneRenderer>(eventSystem, vulkanContext) }
    , computeRenderer{ std::make_unique<ComputeRenderer>(eventSystem, vulkanContext) }
    , uiRenderer{ std::make_unique<UiRenderer>(window, eventSystem, vulkanContext) }
{
    using namespace RenderSystemDetails;
    
    const auto createFrame = [&](const uint32_t index) {
        return Frame(index, 0, CreateCommandBuffer(vulkanContext), CreateFrameSync(vulkanContext));
    };
    
    const auto frameIndices = std::views::iota(static_cast<uint32_t>(0), VulkanConfig::maxFramesInFlight);
    std::ranges::transform(frameIndices, std::back_inserter(frames), createFrame);

    eventSystem.Subscribe<ES::KeyInput>(this, &RenderSystem::OnKeyInput);
}

RenderSystem::~RenderSystem()
{
    eventSystem.UnsubscribeAll(this);

    vulkanContext.GetDevice().WaitIdle();
    
    vulkanContext.GetDescriptorSetsManager().ResetDescriptors(DescriptorScope::eGlobal);
}

void RenderSystem::Process(const float deltaSeconds)
{
    sceneRenderer->Process(deltaSeconds);
    computeRenderer->Process(deltaSeconds);
    uiRenderer->Process(deltaSeconds);
}

void RenderSystem::Render()
{
    using namespace VulkanHelpers;

    Frame& frame = frames[currentFrame];

    const auto& [waitSemaphores, waitStages, signalSemaphores, fence] = frame.sync.AsTuple();

    const Device& device = vulkanContext.GetDevice();

    // Wait for frame to finish execution and signal fence
    vkWaitForFences(device.GetVkDevice(), 1, &fence, VK_TRUE, UINT64_MAX);
    vkResetFences(device.GetVkDevice(), 1, &fence);

    // Acquire next image from the swapchain, frame wait semaphore will be signaled by the presentation engine when it
    // finishes using the image so we can start rendering
    frame.swapchainImageIndex = AcquireNextSwapchainImage(waitSemaphores[0]);

    // Submit scene rendering commands
    const auto renderCommands = [&](VkCommandBuffer buffer) {
        computeRenderer->Render(frame);
        uiRenderer->Render(frame);
    };

    SubmitCommandBuffer(frame.commandBuffer, device.GetQueues().graphics, renderCommands, frame.sync);

    // Present will happen when rendering is finished and the frame signal semaphores are signaled
    Present(signalSemaphores, frame.swapchainImageIndex);

    currentFrame = (currentFrame + 1) % VulkanConfig::maxFramesInFlight;
}

uint32_t RenderSystem::AcquireNextSwapchainImage(const VkSemaphore signalSemaphore) const
{
    const VkSwapchainKHR swapchain = vulkanContext.GetSwapchain().GetVkSwapchainKHR();

    uint32_t imageIndex;
    const VkResult acquireResult = vkAcquireNextImageKHR(vulkanContext.GetDevice().GetVkDevice(), swapchain,
        std::numeric_limits<uint64_t>::max(), signalSemaphore, VK_NULL_HANDLE, &imageIndex);
    Assert(acquireResult == VK_SUCCESS || acquireResult == VK_SUBOPTIMAL_KHR);

    return imageIndex;
}

void RenderSystem::Present(const std::vector<VkSemaphore>& waitSemaphores, const uint32_t imageIndex) const
{
    VkSwapchainKHR swapchains[] = { vulkanContext.GetSwapchain().GetVkSwapchainKHR() };
    
    VkPresentInfoKHR presentInfo = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
    presentInfo.pWaitSemaphores = waitSemaphores.data();
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    const VkResult presentResult = vkQueuePresentKHR(vulkanContext.GetDevice().GetQueues().present, &presentInfo);
    Assert(presentResult == VK_SUCCESS);
}

void RenderSystem::OnKeyInput(const ES::KeyInput& event)
{
    if (event.key == Key::eR && event.action == KeyAction::ePress &&
        HasMod(event.mods, ctrlKeyMod))
    {
        eventSystem.Fire<ES::TryReloadShaders>();
    }
}
