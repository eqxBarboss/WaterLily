#include "Engine/Render/Ui/UiRenderer.hpp"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>

#include "Engine/EngineConfig.hpp"
#include "Engine/EventSystem.hpp"
#include "Engine/Window.hpp"
#include "Engine/Render/Vulkan/VulkanContext.hpp"
#include "Engine/Render/Vulkan/VulkanHelpers.hpp"
#include "Engine/Render/Resources/ResourceHelpers.hpp"
#include "Engine/Render/Resources/Pipelines/GraphicsPipelineBuilder.hpp"

namespace UiRendererDetails
{
    static constexpr std::string_view vertexShaderPath = "~/Shaders/ui.vert";
    static constexpr std::string_view fragmentShaderPath = "~/Shaders/ui.frag";

    static bool showDemoWindow = false;

    static Image CreateFontImage(const ImGuiIO& io, const VulkanContext& vulkanContext)
    {
        using namespace ResourceHelpers;
        
        unsigned char* fontData = nullptr;
        int width = 0;
        int height = 0;

        io.Fonts->GetTexDataAsRGBA32(&fontData, &width, &height);

        const auto dataSize = static_cast<size_t>(width) * height * 4;
        const std::span<const unsigned char> fontDataSpan = { fontData, dataSize };

        const BufferDescription bufferDescription = { .size = dataSize,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };

        const Buffer fontDataBuffer(bufferDescription, false, fontDataSpan, vulkanContext);
         
        const ImageDescription fontImageDescription{
            .extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 },
            .mipLevelsCount = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };

        auto fontImage = Image(fontImageDescription, vulkanContext);
        
        vulkanContext.GetDevice().ExecuteOneTimeCommandBuffer([&](VkCommandBuffer commandBuffer) {
            CopyBufferToImage(commandBuffer, fontDataBuffer, fontImage);
            TransitionLayout(commandBuffer, fontImage, ImageLayoutTransitions::dstOptimalToShaderReadOnlyOptimal);
        });

        return fontImage;
    }

    static VkSampler CreateFontImageSampler(const VkDevice device)
    {
        VkSamplerCreateInfo samplerInfo = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

        VkSampler sampler;
        const VkResult result = vkCreateSampler(device, &samplerInfo, nullptr, &sampler);
        Assert(result == VK_SUCCESS);

        return sampler;
    }

    static RenderPass CreateRenderPass(const VulkanContext& vulkanContext)
    {
        AttachmentDescription colorAttachmentDescription = {
            .format = vulkanContext.GetSwapchain().GetSurfaceFormat().format,
            .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .actualLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR };

        return RenderPassBuilder(vulkanContext)
            .SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .AddColorAttachment(colorAttachmentDescription)
            .Build();
    }

    static std::vector<VkFramebuffer> CreateFramebuffers(const RenderPass& renderPass, const VulkanContext& vulkanContext)
    {
        using namespace VulkanHelpers;
        
        const Swapchain& swapchain = vulkanContext.GetSwapchain();
        
        std::vector<VkFramebuffer> framebuffers;
        framebuffers.reserve(swapchain.GetImageViews().size());

        std::ranges::transform(swapchain.GetImageViews(), std::back_inserter(framebuffers), [&](const ImageView& view) {
            return CreateFrameBuffer(renderPass, swapchain.GetExtent(), { view.GetVkImageView() }, vulkanContext);
        });
        
        return framebuffers;
    }

    static std::vector<ShaderModule> GetShaderModules(const ShaderManager& shaderManager)
    {
        std::vector<ShaderModule> shaderModules;
        shaderModules.reserve(2);

        shaderModules.emplace_back(shaderManager.CreateShaderModule(FilePath(vertexShaderPath), ShaderType::eVertex));
        shaderModules.emplace_back(shaderManager.CreateShaderModule(FilePath(fragmentShaderPath), ShaderType::eFragment));

        return shaderModules;
    }

    std::vector<VkVertexInputBindingDescription> GetVertexBindings()
    {
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(ImDrawVert);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return { binding };
    }

    std::vector<VkVertexInputAttributeDescription> GetVertexAttributes()
    {
        std::vector<VkVertexInputAttributeDescription> attributes(3);

        VkVertexInputAttributeDescription& posAttribute = attributes[0];
        posAttribute.binding = 0;
        posAttribute.location = 0;
        posAttribute.format = VK_FORMAT_R32G32_SFLOAT;
        posAttribute.offset = offsetof(ImDrawVert, pos);

        VkVertexInputAttributeDescription& uvAttribute = attributes[1];
        uvAttribute.binding = 0;
        uvAttribute.location = 1;
        uvAttribute.format = VK_FORMAT_R32G32_SFLOAT;
        uvAttribute.offset = offsetof(ImDrawVert, uv);

        VkVertexInputAttributeDescription& colorAttribute = attributes[2];
        colorAttribute.binding = 0;
        colorAttribute.location = 2;
        colorAttribute.format = VK_FORMAT_R8G8B8A8_UNORM;
        colorAttribute.offset = offsetof(ImDrawVert, col);

        return attributes;
    }

    static VkRect2D GetScissor(const ImDrawCmd& command, const ImVec2& clipScale)
    {
        VkRect2D scissor;
        scissor.offset.x = std::max(static_cast<int32_t>(command.ClipRect.x * clipScale.x), 0);
        scissor.offset.y = std::max(static_cast<int32_t>(command.ClipRect.y * clipScale.y), 0);
        scissor.extent.width = static_cast<uint32_t>((command.ClipRect.z - command.ClipRect.x) * clipScale.x);
        scissor.extent.height = static_cast<uint32_t>((command.ClipRect.w - command.ClipRect.y) * clipScale.y);

        return scissor;
    }
}

UiRenderer::UiRenderer(const Window& aWindow, EventSystem& aEventSystem, const VulkanContext& aVulkanContext)
    : vulkanContext{ &aVulkanContext }
    , eventSystem{ &aEventSystem }
    , window{ &aWindow }
    , vertexBuffers{ VulkanConfig::maxFramesInFlight }
    , indexBuffers{ VulkanConfig::maxFramesInFlight }
{
    using namespace UiRendererDetails;

    ImGui::CreateContext();
    ImGui::StyleColorsClassic();
    
    // Using InitForOther intentionally to make ImGui handle all
    // of the i/o for us while implementing our own rendering
    ImGui_ImplGlfw_InitForOther(window->GetGlfwWindow(), true);

    UpdateImGuiInputState();

    renderPass = CreateRenderPass(*vulkanContext);
    framebuffers = CreateFramebuffers(renderPass, *vulkanContext);

    fontImage = CreateFontImage(ImGui::GetIO(), *vulkanContext);
    fontImageView = ImageView(fontImage, VK_IMAGE_ASPECT_COLOR_BIT, *vulkanContext);
    fontImageSampler = CreateFontImageSampler(vulkanContext->GetDevice().GetVkDevice());

    std::tie(descriptor, layout) = vulkanContext->GetDescriptorSetsManager().GetDescriptorSetBuilder()
        .Bind(0, fontImageView, fontImageSampler, VK_SHADER_STAGE_FRAGMENT_BIT)
        .Build();

    std::vector<ShaderModule> shaderModules = GetShaderModules(vulkanContext->GetShaderManager());
    Assert(std::ranges::all_of(shaderModules, &ShaderModule::IsValid));

    CreateGraphicsPipeline(std::move(shaderModules));

    eventSystem->Subscribe<ES::BeforeSwapchainRecreated>(this, &UiRenderer::OnBeforeSwapchainRecreated);
    eventSystem->Subscribe<ES::SwapchainRecreated>(this, &UiRenderer::OnSwapchainRecreated);
    eventSystem->Subscribe<ES::BeforeWindowRecreated>(this, &UiRenderer::OnBeforeWindowRecreated);
    eventSystem->Subscribe<ES::WindowRecreated>(this, &UiRenderer::OnWindowRecreated);
    eventSystem->Subscribe<ES::TryReloadShaders>(this, &UiRenderer::OnTryReloadShaders);
    eventSystem->Subscribe<ES::BeforeCursorModeUpdated>(this, &UiRenderer::OnBeforeCursorModeUpdated);
}

UiRenderer::~UiRenderer()
{
    eventSystem->UnsubscribeAll(this);

    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    VulkanHelpers::DestroySampler(vulkanContext->GetDevice().GetVkDevice(), fontImageSampler);
    VulkanHelpers::DestroyFramebuffers(framebuffers, *vulkanContext);
}

void UiRenderer::Process(const float deltaSeconds)
{
    std::ranges::rotate(frameTimes, frameTimes.begin() + 1);
    frameTimes.back() = deltaSeconds;
    
    // Handles i/o and other different window events
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    // All our UI elements
    BuildUI();

    // Creates all abstract rendering commands
    ImGui::Render();
    
    // Don't forget to update push constants
    // Ignore translate for now as there's only 1 viewport
    const ImGuiIO& io = ImGui::GetIO();
    pushConstants.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
}

void UiRenderer::Render(const Frame& frame)
{
    using namespace VulkanHelpers;

    UpdateBuffers(frame.index);

    const Buffer& vertexBuffer = vertexBuffers[frame.index];
    const Buffer& indexBuffer = indexBuffers[frame.index];

    const VkExtent2D extent = vulkanContext->GetSwapchain().GetExtent();

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass.GetVkRenderPass();
    renderPassInfo.framebuffer = framebuffers[frame.swapchainImageIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = extent;
    
    const VkCommandBuffer commandBuffer = frame.commandBuffer;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.Get());

    const VkViewport viewport = GetViewport(static_cast<float>(extent.width), static_cast<float>(extent.height));
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.GetLayout(),
        0, 1, &descriptor, 0, nullptr);
    
    vkCmdPushConstants(commandBuffer, graphicsPipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0,
        sizeof(PushConstants), &pushConstants);

    if (const ImDrawData* drawData = ImGui::GetDrawData(); !drawData->CmdLists.empty())
    {
        const VkBuffer vkVertexBuffers[] = { vertexBuffer.Get() };
        constexpr VkDeviceSize offsets[] = { 0 };

        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vkVertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer.Get(), 0, VK_INDEX_TYPE_UINT16);
        
        int32_t vertexOffset = 0;
        int32_t indexOffset = 0;
        
        const ImVec2 clipScale = ImGui::GetIO().DisplayFramebufferScale;
        VkRect2D scissorRect;

        const auto issueDraw = [&](const ImDrawCmd& command) {
            scissorRect = UiRendererDetails::GetScissor(command, clipScale);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissorRect);

            vkCmdDrawIndexed(commandBuffer, command.ElemCount, 1, indexOffset, vertexOffset, 0);

            indexOffset += static_cast<int32_t>(command.ElemCount);
        };

        const auto processDrawList = [&](const ImDrawList* drawList) {
            std::ranges::for_each(drawList->CmdBuffer, issueDraw);
            vertexOffset += static_cast<int32_t>(drawList->VtxBuffer.Size);
        };

        std::ranges::for_each(drawData->CmdLists, processDrawList);
    }

    vkCmdEndRenderPass(commandBuffer);
}

void UiRenderer::CreateGraphicsPipeline(std::vector<ShaderModule>&& shaderModules)
{
    graphicsPipeline = GraphicsPipelineBuilder(*vulkanContext)
        .SetDescriptorSetLayouts({ layout.Get() })
        .AddPushConstantRange({ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants) })
        .SetShaderModules(std::move(shaderModules))
        .SetVertexData(UiRendererDetails::GetVertexBindings(), UiRendererDetails::GetVertexAttributes())
        .SetInputTopology(InputTopology::eTriangleList)
        .SetPolygonMode(PolygonMode::eFill)
        .SetCullMode(CullMode::eNone, false)
        .SetMultisampling(VK_SAMPLE_COUNT_1_BIT)
        .EnableBlending()
        .SetDepthState(false, false, VK_COMPARE_OP_NEVER)
        .SetRenderPass(renderPass)
        .Build();
}

void UiRenderer::BuildUI()
{
    using namespace EngineConfig;

    ImGui::Begin(engineName);

    const VkPhysicalDeviceProperties& deviceProperties = vulkanContext->GetDevice().GetPhysicalDeviceProperties();

    ImGui::TextUnformatted(deviceProperties.deviceName);
    
    ImGui::Text("Vulkan API %i.%i.%i", VK_API_VERSION_MAJOR(deviceProperties.apiVersion),
        VK_API_VERSION_MINOR(deviceProperties.apiVersion), VK_API_VERSION_PATCH(deviceProperties.apiVersion));
    
    const float avgFrameTime = std::accumulate(frameTimes.begin(), frameTimes.end(), 0.0f) / frameTimes.size();
    const float fps = (avgFrameTime > 0.0f) ? (1.0f / avgFrameTime) : 0.0f;
    
    ImGui::Text("Fps: %.2f", fps);

    ImGui::Checkbox("Show demo window", &UiRendererDetails::showDemoWindow);
    
    ImGui::End();

    if (UiRendererDetails::showDemoWindow)
    {
        ImGui::ShowDemoWindow();
    }
}

void UiRenderer::UpdateBuffers(const uint32_t frameIndex)
{
    ImDrawData* drawData = ImGui::GetDrawData();

    const VkDeviceSize vertexBufferSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
    const VkDeviceSize indexBufferSize = drawData->TotalIdxCount * sizeof(ImDrawIdx);

    if (vertexBufferSize == 0 || indexBufferSize == 0) 
    {
        return;
    }

    Buffer& vertexBuffer = vertexBuffers[frameIndex];
    Buffer& indexBuffer = indexBuffers[frameIndex];

    if (!vertexBuffer.IsValid() || vertexBuffer.GetDescription().size < vertexBufferSize) 
    {
        BufferDescription vertexBufferDescription{ vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };

        vertexBuffer = Buffer(vertexBufferDescription, false, *vulkanContext);
        std::ignore = vertexBuffer.MapMemory(true); // persistent mapping
    }

    if (!indexBuffer.IsValid() || indexBuffer.GetDescription().size < indexBufferSize)
    {
        BufferDescription indexBufferDescription{ indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };

        indexBuffer = Buffer(indexBufferDescription, false, *vulkanContext);
        std::ignore = indexBuffer.MapMemory(true); // persistent mapping
    }

    std::span<std::byte> vertexBufferMemory = vertexBuffer.MapMemory();
    std::span<std::byte> indexBufferMemory = indexBuffer.MapMemory();

    const auto processDrawList = [&](const ImDrawList* drawList) {
        const auto vertexData = std::as_bytes(std::span(drawList->VtxBuffer));
        const auto indexData = std::as_bytes(std::span(drawList->IdxBuffer));
        
        std::ranges::copy(vertexData, vertexBufferMemory.begin());
        std::ranges::copy(indexData, indexBufferMemory.begin());
        
        vertexBufferMemory = vertexBufferMemory.subspan(vertexData.size());
        indexBufferMemory = indexBufferMemory.subspan(indexData.size());
    };

    std::ranges::for_each(drawData->CmdLists, processDrawList);
}

void UiRenderer::UpdateImGuiInputState() const
{
    ImGuiIO& io = ImGui::GetIO();
    
    if (cursorMode == CursorMode::eEnabled)
    {
        io.ConfigFlags &= ~(ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoKeyboard);
    }
    else
    {
        io.ConfigFlags |= ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoKeyboard;
    }
}

void UiRenderer::OnBeforeSwapchainRecreated(const ES::BeforeSwapchainRecreated& event)
{
    VulkanHelpers::DestroyFramebuffers(framebuffers, *vulkanContext);
}

void UiRenderer::OnSwapchainRecreated(const ES::SwapchainRecreated& event)
{
    framebuffers = UiRendererDetails::CreateFramebuffers(renderPass, *vulkanContext);
}

void UiRenderer::OnBeforeWindowRecreated(const ES::BeforeWindowRecreated& event)
{
    ImGui_ImplGlfw_Shutdown();
}

void UiRenderer::OnWindowRecreated(const ES::WindowRecreated& event)
{
    ImGui_ImplGlfw_InitForOther(event.window->GetGlfwWindow(), true);
}

void UiRenderer::OnTryReloadShaders(const ES::TryReloadShaders& event)
{
    if (std::vector<ShaderModule> shaderModules = UiRendererDetails::GetShaderModules(vulkanContext->GetShaderManager());
        std::ranges::all_of(shaderModules, &ShaderModule::IsValid))
    {
        CreateGraphicsPipeline(std::move(shaderModules));
    }
}

void UiRenderer::OnBeforeCursorModeUpdated(const ES::BeforeCursorModeUpdated& event)
{
    cursorMode = event.newCursorMode;
    UpdateImGuiInputState();
}
