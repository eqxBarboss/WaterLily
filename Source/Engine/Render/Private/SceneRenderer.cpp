#include "Engine/Render/SceneRenderer.hpp"

#include "Engine/Render/Vulkan/VulkanContext.hpp"
#include "Engine/Render/Vulkan/VulkanConfig.hpp"
#include "Engine/EventSystem.hpp"
#include "Engine/Render/Resources/Image.hpp"
#include "Engine/Render/Resources/ImageView.hpp"
#include "Engine/Render/Resources/Buffer.hpp"
#include "Engine/Render/Resources/Pipelines/GraphicsPipelineBuilder.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace SceneRendererDetails
{
    static constexpr std::string_view vertexShaderPath = "~/Shaders/shader.vert";
    static constexpr std::string_view fragmentShaderPath = "~/Shaders/shader.frag";

    static std::vector<ShaderModule> GetShaderModules(const ShaderManager& shaderManager)
    {
        std::vector<ShaderModule> shaderModules;
        shaderModules.reserve(2);

        shaderModules.emplace_back(shaderManager.CreateShaderModule(FilePath(vertexShaderPath), ShaderType::eVertex));
        shaderModules.emplace_back(shaderManager.CreateShaderModule(FilePath(fragmentShaderPath), ShaderType::eFragment));

        return shaderModules;
    }

    static RenderPass CreateRenderPass(const VulkanContext& vulkanContext)
    {
        AttachmentDescription colorAttachmentDescription = {
            .format = vulkanContext.GetSwapchain().GetSurfaceFormat().format,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .actualLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

        AttachmentDescription resolveAttachmentDescription = {
            .format = vulkanContext.GetSwapchain().GetSurfaceFormat().format,
            .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .actualLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

        AttachmentDescription depthStencilAttachmentDescription = {
            .format = VulkanConfig::depthImageFormat,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .actualLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

        return RenderPassBuilder(vulkanContext)
            .SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .SetMultisampling(vulkanContext.GetDevice().GetMaxSampleCount())
            .AddColorAndResolveAttachments(colorAttachmentDescription, resolveAttachmentDescription)
            .AddDepthStencilAttachment(depthStencilAttachmentDescription)
            .Build();
    }

    static Buffer CreateUniformBuffer(const VulkanContext& vulkanContext)
    {
        BufferDescription bufferDescription{ sizeof(gpu::UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };

        return { bufferDescription, false, vulkanContext };
    }

    static std::tuple<std::vector<VkDescriptorSet>, DescriptorSetLayout> CreateDescriptors(
        const std::vector<Buffer>& uniformBuffers, const VulkanContext& vulkanContext)
    {
        DescriptorSetLayout layout = vulkanContext.GetDescriptorSetsManager().GetDescriptorSetLayoutBuilder()
            .AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
            .Build();

        std::vector<VkDescriptorSet> descriptors;

        std::ranges::transform(uniformBuffers, std::back_inserter(descriptors), [&](const Buffer& uniformBuffer) {
            return std::get<0>(vulkanContext.GetDescriptorSetsManager().GetDescriptorSetBuilder(layout)
            .Bind(0, uniformBuffer)
            .Build());
        });

        return { descriptors, layout };
    }
}

SceneRenderer::SceneRenderer(EventSystem& aEventSystem, const VulkanContext& aVulkanContext)
    : vulkanContext{ &aVulkanContext }
    , eventSystem{ &aEventSystem }
{
    using namespace SceneRendererDetails;

    renderPass = CreateRenderPass(*vulkanContext);

    CreateAttachmentsAndFramebuffers();

    uniformBuffers.resize(VulkanConfig::maxFramesInFlight);
    std::ranges::generate(uniformBuffers, [&]() { return CreateUniformBuffer(*vulkanContext); });

    std::tie(uniformDescriptors, layout) = CreateDescriptors(uniformBuffers, *vulkanContext);

    eventSystem->Subscribe<ES::BeforeSwapchainRecreated>(this, &SceneRenderer::OnBeforeSwapchainRecreated);
    eventSystem->Subscribe<ES::SwapchainRecreated>(this, &SceneRenderer::OnSwapchainRecreated);
    eventSystem->Subscribe<ES::TryReloadShaders>(this, &SceneRenderer::OnTryReloadShaders);
    eventSystem->Subscribe<ES::SceneOpened>(this, &SceneRenderer::OnSceneOpen);
    eventSystem->Subscribe<ES::SceneClosed>(this, &SceneRenderer::OnSceneClose);
}

SceneRenderer::~SceneRenderer()
{
    eventSystem->UnsubscribeAll(this);

    DestroyAttachmentsAndFramebuffers();
}

void SceneRenderer::Process(const float deltaSeconds)
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

void SceneRenderer::Render(const Frame& frame)
{
    using namespace VulkanHelpers;

    if (!scene)
    {
        return;
    }

    uniformBuffers[frame.index].Fill(std::as_bytes(std::span(&ubo, 1)));

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass.GetVkRenderPass();
    renderPassInfo.framebuffer = framebuffers[frame.swapchainImageIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = vulkanContext->GetSwapchain().GetExtent();

    std::array<VkClearValue, 3> clearValues{};
    clearValues[0].color = { { 0.73f, 0.95f, 1.0f, 1.0f } };
    clearValues[2].depthStencil = { 1.0f, 0 };

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();
    
    const VkCommandBuffer commandBuffer = frame.commandBuffer;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.Get());

    const VkExtent2D extent = vulkanContext->GetSwapchain().GetExtent();
    VkViewport viewport = GetViewport(static_cast<float>(extent.width), static_cast<float>(extent.height));
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    const VkRect2D scissor = GetScissor(extent);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    VkBuffer vertexBuffers[] = { scene->GetVertexBuffer().Get() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer, scene->GetIndexBuffer().Get(), 0, VK_INDEX_TYPE_UINT32);

    std::vector<VkDescriptorSet> descriptors = { uniformDescriptors[frame.index] };
    std::ranges::copy(scene->GetGlobalDescriptors(), std::back_inserter(descriptors));

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.GetLayout(),
        0, static_cast<uint32_t>(descriptors.size()), descriptors.data(), 0, nullptr);

    vkCmdDrawIndexedIndirect(commandBuffer, indirectBuffer->Get(), 0, indirectDrawCount,
        sizeof(VkDrawIndexedIndirectCommand));

    vkCmdEndRenderPass(commandBuffer);
}

void SceneRenderer::CreateGraphicsPipeline(std::vector<ShaderModule>&& shaderModules)
{
    using namespace VulkanHelpers;

    std::vector layouts = { layout.Get(),
        Scene::GetGlobalDescriptorSetLayout(*vulkanContext).Get() };

    graphicsPipeline = GraphicsPipelineBuilder(*vulkanContext)
        .SetDescriptorSetLayouts(std::move(layouts))
        .AddPushConstantRange({ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants) })
        .SetShaderModules(std::move(shaderModules))
        .SetVertexData(Vertex::GetBindings(), Vertex::GetAttributes())
        .SetInputTopology(InputTopology::eTriangleList)
        .SetPolygonMode(PolygonMode::eFill)
        .SetCullMode(CullMode::eBack, false)
        .SetMultisampling(vulkanContext->GetDevice().GetMaxSampleCount())
        .SetDepthState(true, true, VK_COMPARE_OP_LESS_OR_EQUAL)
        .SetRenderPass(renderPass)
        .Build();
}

void SceneRenderer::CreateAttachmentsAndFramebuffers()
{
    using namespace VulkanHelpers;

    const Swapchain& swapchain = vulkanContext->GetSwapchain();

    // Attachments
    colorAttachment = CreateColorAttachment(swapchain.GetExtent(), *vulkanContext);
    colorAttachmentView = std::make_unique<ImageView>(*colorAttachment, VK_IMAGE_ASPECT_COLOR_BIT, *vulkanContext);

    depthAttachment = CreateDepthAttachment(swapchain.GetExtent(), *vulkanContext);
    depthAttachmentView = std::make_unique<ImageView>(*depthAttachment, VK_IMAGE_ASPECT_DEPTH_BIT, *vulkanContext);

    // Framebuffers
    framebuffers.reserve(swapchain.GetImageViews().size());

    std::vector<VkImageView> attachments = { colorAttachmentView->GetVkImageView(), VK_NULL_HANDLE,
        depthAttachmentView->GetVkImageView() };

    std::ranges::transform(swapchain.GetImageViews(), std::back_inserter(framebuffers), [&](const ImageView& imageView) {
        attachments[1] = imageView.GetVkImageView();
        return CreateFrameBuffer(renderPass, swapchain.GetExtent(), attachments, *vulkanContext);
    });
}

void SceneRenderer::DestroyAttachmentsAndFramebuffers()
{
    using namespace VulkanHelpers;

    DestroyFramebuffers(framebuffers, *vulkanContext);

    colorAttachmentView.reset();
    colorAttachment.reset();

    depthAttachmentView.reset();
    depthAttachment.reset();
}

void SceneRenderer::OnBeforeSwapchainRecreated(const ES::BeforeSwapchainRecreated& event)
{
    DestroyAttachmentsAndFramebuffers();
}

void SceneRenderer::OnSwapchainRecreated(const ES::SwapchainRecreated& event)
{
    CreateAttachmentsAndFramebuffers();
}

void SceneRenderer::OnTryReloadShaders(const ES::TryReloadShaders& event)
{
    using namespace SceneRendererDetails;

    if (std::vector<ShaderModule> shaderModules = GetShaderModules(vulkanContext->GetShaderManager());
        std::ranges::all_of(shaderModules, &ShaderModule::IsValid))
    {
        CreateGraphicsPipeline(std::move(shaderModules));
    }
}

void SceneRenderer::OnSceneOpen(const ES::SceneOpened& event)
{
    using namespace SceneRendererDetails;
    using namespace VulkanHelpers;

    scene = &event.scene;

    if (!graphicsPipeline.IsValid())
    {
        std::vector<ShaderModule> shaderModules = GetShaderModules(vulkanContext->GetShaderManager());

        const bool allValid = std::ranges::all_of(shaderModules, &ShaderModule::IsValid);
        Assert(allValid);

        CreateGraphicsPipeline(std::move(shaderModules));
    }

    std::tie(indirectBuffer, indirectDrawCount) = CreateIndirectBuffer(*scene, *vulkanContext);
}

void SceneRenderer::OnSceneClose(const ES::SceneClosed& event)
{
    scene = nullptr;
}
