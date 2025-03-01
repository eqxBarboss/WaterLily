#include "Engine/Render/Vulkan/VulkanHelpers.hpp"

#include "Engine/Render/Vulkan/VulkanContext.hpp"
#include "Engine/Render/Vulkan/RenderPass.hpp"
#include "Engine/Render/Resources/CommandBufferSync.hpp"
#include "Engine/Render/Resources/Buffer.hpp"
#include "Engine/Render/Resources/ImageView.hpp"
#include "Engine/Render/Resources/Image.hpp"
#include "Engine/Scene/Scene.hpp"

std::vector<VkCommandBuffer> VulkanHelpers::CreateCommandBuffers(VkDevice device, const size_t count, 
    VkCommandPool commandPool)
{
    std::vector<VkCommandBuffer> commandBuffers;
    commandBuffers.resize(count);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    VkResult result = vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data());
    Assert(result == VK_SUCCESS);

    return commandBuffers;
}

void VulkanHelpers::SubmitCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, const DeviceCommands& commands,
    const CommandBufferSync& sync)
{
    const auto& [waitSemaphores, waitStages, signalSemaphores, fence] = sync.AsTuple();

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    // FYI: if buffer was created with VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT flag vkBeginCommandBuffer
    // implicitly resets the buffer to the initial state
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    commands(commandBuffer);
    VkResult result = vkEndCommandBuffer(commandBuffer);
    Assert(result == VK_SUCCESS);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
    submitInfo.pSignalSemaphores = signalSemaphores.data();

    result = vkQueueSubmit(queue, 1, &submitInfo, fence);
    Assert(result == VK_SUCCESS);
}

VkFence VulkanHelpers::CreateFence(VkDevice device, VkFenceCreateFlags flags)
{
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = flags;

    VkFence fence;
    const VkResult result = vkCreateFence(device, &fenceInfo, nullptr, &fence);
    Assert(result == VK_SUCCESS);

    return fence;
}

std::vector<VkFence> VulkanHelpers::CreateFences(VkDevice device, VkFenceCreateFlags flags, size_t count)
{
    std::vector<VkFence> fences(count);

    std::ranges::generate(fences, [&]() {
        return CreateFence(device, flags);
    });

    return fences;
}

void VulkanHelpers::DestroyFences(VkDevice device, std::vector<VkFence>& fences)
{
    std::ranges::for_each(fences, [&](VkFence fence) {
        vkDestroyFence(device, fence, nullptr);
        });

    fences.clear();
}

VkSemaphore VulkanHelpers::CreateSemaphore(VkDevice device)
{
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkSemaphore semaphore;
    const VkResult result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore);
    Assert(result == VK_SUCCESS);
    
    return semaphore;
}

std::vector<VkSemaphore> VulkanHelpers::CreateSemaphores(VkDevice device, size_t count)
{
    std::vector<VkSemaphore> semaphores(count);
    
    std::ranges::generate(semaphores, [&]() {
        return CreateSemaphore(device);
    });

    return semaphores;
}

void VulkanHelpers::DestroySemaphores(VkDevice device, std::vector<VkSemaphore>& semaphores)
{
    std::ranges::for_each(semaphores, [&](VkSemaphore semaphore) {
        if (semaphore != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(device, semaphore, nullptr);
        }        
    });

    semaphores.clear();
}

VkSampler VulkanHelpers::CreateSampler(VkDevice device, const VkPhysicalDeviceProperties& properties, 
    uint32_t mipLevelsCount)
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(mipLevelsCount);

    VkSampler sampler;
    VkResult result = vkCreateSampler(device, &samplerInfo, nullptr, &sampler);
    Assert(result == VK_SUCCESS);

    return sampler;
}

void VulkanHelpers::DestroySampler(VkDevice device, VkSampler sampler)
{
    if (sampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(device, sampler, nullptr);
    }
}

VkFramebuffer VulkanHelpers::CreateFrameBuffer(const RenderPass& renderPass, const VkExtent2D extent,
    const std::vector<VkImageView>& attachments, const VulkanContext& vulkanContext)
{
    const VkDevice device = vulkanContext.GetDevice().GetVkDevice();

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass.GetVkRenderPass();
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = extent.width;
    framebufferInfo.height = extent.height;
    framebufferInfo.layers = 1;

    VkFramebuffer framebuffer;
    const VkResult result = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer);
    Assert(result == VK_SUCCESS);

    return framebuffer;
}

void VulkanHelpers::DestroyFramebuffers(std::vector<VkFramebuffer>& framebuffers, const VulkanContext& vulkanContext)
{
    std::ranges::for_each(framebuffers, [&](VkFramebuffer framebuffer) {
        vkDestroyFramebuffer(vulkanContext.GetDevice().GetVkDevice(), framebuffer, nullptr);
    });

    framebuffers.clear();
}

std::unique_ptr<Image> VulkanHelpers::CreateColorAttachment(VkExtent2D extent, const VulkanContext& vulkanContext)
{
    ImageDescription imageDescription{
        .extent = { extent.width, extent.height, 1 },
        .mipLevelsCount = 1,
        .samples = vulkanContext.GetDevice().GetMaxSampleCount(),
        .format = vulkanContext.GetSwapchain().GetSurfaceFormat().format,
        .usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };

    return std::make_unique<Image>(imageDescription, vulkanContext);
}

std::unique_ptr<Image> VulkanHelpers::CreateDepthAttachment(VkExtent2D extent, const VulkanContext& vulkanContext)
{
    ImageDescription imageDescription{
        .extent = { extent.width, extent.height, 1 },
        .mipLevelsCount = 1,
        .samples = vulkanContext.GetDevice().GetMaxSampleCount(),
        .format = VulkanConfig::depthImageFormat,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };

    return std::make_unique<Image>(imageDescription, vulkanContext);
}

std::tuple<std::unique_ptr<Buffer>, uint32_t> VulkanHelpers::CreateIndirectBuffer(const Scene& scene,
    const VulkanContext& vulkanContext)
{
    std::vector<VkDrawIndexedIndirectCommand> indirectCommands;

    const auto createDrawCommand = [&](const Primitive& primitive) {
        VkDrawIndexedIndirectCommand indirectCommand{};
        indirectCommand.instanceCount = 1;
        indirectCommand.firstIndex = primitive.firstIndex;
        indirectCommand.indexCount = primitive.indexCount;
        indirectCommand.firstInstance = primitive.nodeId; // Using this to access transforms SSBO in shaders

        return indirectCommand;
    };

    std::ranges::for_each(scene.GetNodes(), [&](const SceneNode* node) {

        if (!node->visible)
        {
            return;
        }

        std::ranges::transform(node->mesh.primitives, std::back_inserter(indirectCommands), createDrawCommand);
    });

    std::span indirectCommandsSpan(std::as_const(indirectCommands));

    BufferDescription indirectBufferDescription{ .size = static_cast<VkDeviceSize>(indirectCommandsSpan.size_bytes()),
        .usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };

    return { std::make_unique<Buffer>(indirectBufferDescription, true, indirectCommandsSpan, vulkanContext),
        static_cast<uint32_t>(indirectCommands.size()) };
}

VkViewport VulkanHelpers::GetViewport(const float width, const float height)
{
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = width;
    viewport.height = height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    return viewport;
}

VkRect2D VulkanHelpers::GetScissor(const VkExtent2D extent)
{
    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = extent;

    return scissor;
}

VkPipelineLayout VulkanHelpers::CreatePipelineLayout(const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
    const std::vector<VkPushConstantRange>& pushConstantRanges, const VkDevice device)
{
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
    pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();

    VkPipelineLayout pipelineLayout;

    const VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
    Assert(result == VK_SUCCESS);

    return pipelineLayout;
}

const VkDescriptorSetLayoutBinding& VulkanHelpers::GetBinding(const std::vector<VkDescriptorSetLayoutBinding>& bindings, 
    const uint32_t index)
{
    const auto it = std::ranges::find_if(bindings, [&](const auto& binding) { return binding.binding == index; });

    Assert(it != bindings.end());

    return *it;
}
