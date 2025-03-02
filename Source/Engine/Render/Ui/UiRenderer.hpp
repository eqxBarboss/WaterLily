#pragma once

#include <volk.h>

DISABLE_WARNINGS_BEGIN
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
DISABLE_WARNINGS_END

#include "Engine/Render/Vulkan/Pipeline.hpp"
#include "Engine/Render/Vulkan/RenderPass.hpp"
#include "Engine/Render/Resources/Buffer.hpp"
#include "Engine/Render/Resources/Image.hpp"
#include "Engine/Render/Resources/ImageView.hpp"
#include "Engine/Render/Resources/Shaders/ShaderModule.hpp"
#include "Engine/Render/Resources/DescriptorSets/DescriptorSetLayout.hpp"
#include "Engine/Render/Frame.hpp"
#include "Utils/Constants.hpp"

class VulkanContext;
class EventSystem;
class Window;

namespace ES
{
    struct BeforeSwapchainRecreated;
    struct SwapchainRecreated;
	struct BeforeWindowRecreated;
	struct WindowRecreated;
	struct TryReloadShaders;
	struct BeforeCursorModeUpdated;
}

class UiRenderer
{
public:
	struct PushConstants
	{
		glm::vec2 scale = Vector2::allOnes;
		glm::vec2 translate = Vector2::allMinusOnes;
	};

	UiRenderer(const Window& window, EventSystem& eventSystem, const VulkanContext& vulkanContext);
	~UiRenderer();

	UiRenderer(const UiRenderer&) = delete;
	UiRenderer& operator=(const UiRenderer&) = delete;

	UiRenderer(UiRenderer&&) = delete;
	UiRenderer& operator=(UiRenderer&&) = delete;

	void Process(float deltaSeconds);

	void Render(const Frame& frame);

private:
	void CreateGraphicsPipeline(std::vector<ShaderModule>&& shaderModules);

	void BuildUI();
    
	void UpdateBuffers(uint32_t frameIndex);
    
    void UpdateImGuiInputState() const;

    void OnBeforeSwapchainRecreated(const ES::BeforeSwapchainRecreated& event);
    void OnSwapchainRecreated(const ES::SwapchainRecreated& event);
	void OnBeforeWindowRecreated(const ES::BeforeWindowRecreated& event);
	void OnWindowRecreated(const ES::WindowRecreated& event);
	void OnTryReloadShaders(const ES::TryReloadShaders& event);
	void OnBeforeCursorModeUpdated(const ES::BeforeCursorModeUpdated& event);

	const VulkanContext* vulkanContext = nullptr;

	EventSystem* eventSystem = nullptr;

	const Window* window = nullptr;

	RenderPass renderPass;
	Pipeline graphicsPipeline;

	std::vector<VkFramebuffer> framebuffers;

	std::vector<Buffer> vertexBuffers;
	std::vector<Buffer> indexBuffers;

	Image fontImage;
	ImageView fontImageView;
	VkSampler fontImageSampler = VK_NULL_HANDLE;

	VkDescriptorSet descriptor = VK_NULL_HANDLE;
	DescriptorSetLayout layout;

	PushConstants pushConstants;

	CursorMode cursorMode = CursorMode::eDisabled;
    
    std::array<float, 50> frameTimes = {};
};
