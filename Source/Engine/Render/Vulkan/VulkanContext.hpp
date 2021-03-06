#pragma once

#include "Engine/Window.hpp"
#include "Engine/Render/Vulkan/Instance.hpp"
#include "Engine/Render/Vulkan/Surface.hpp"
#include "Engine/Render/Vulkan/Device.hpp"
#include "Engine/Render/Vulkan/Swapchain.hpp"

#include "Engine/Render/Vulkan/Resources/MemoryManager.hpp"
#include "Engine/Render/Vulkan/Shaders/ShaderManager.hpp"
#include "Engine/Render/Vulkan/Resources/BufferManager.hpp"

class VulkanContext
{
public:
	static void Create(const Window& window);
	static void Destroy();

	static std::unique_ptr<Instance> instance;
	static std::unique_ptr<Surface> surface;
	static std::unique_ptr<Device> device;
	static std::unique_ptr<Swapchain> swapchain;

	static std::unique_ptr<MemoryManager> memoryManager;
	static std::unique_ptr<ShaderManager> shaderManager;
	static std::unique_ptr<BufferManger> bufferManager;
};