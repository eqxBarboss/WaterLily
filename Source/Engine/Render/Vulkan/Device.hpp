#pragma once

#include "Engine/Render/Vulkan/VulkanHelpers.hpp"
#include "Engine/Render/Vulkan/Resources/CommandBufferSync.hpp"

#include <volk.h>

class VulkanContext;

struct QueueFamilyIndices
{
	uint32_t graphicsFamily;
	uint32_t presentFamily;
};

struct Queues
{	
	VkQueue graphics;
	VkQueue present;
	QueueFamilyIndices familyIndices;
};

class Device
{
public:
	Device(const VulkanContext& aVulkanContext);
	~Device();

	Device(const Device&) = delete;
	Device& operator=(const Device&) = delete;

	Device(Device&&) = delete;
	Device& operator=(Device&&) = delete;

	void WaitIdle() const;

	VkCommandPool GetCommandPool(CommandBufferType type) const;
	void ExecuteOneTimeCommandBuffer(DeviceCommands commands) const;

	VkDevice GetVkDevice() const
	{
		return device;
	}
	
	VkPhysicalDevice GetPhysicalDevice() const
	{
		return physicalDevice;
	}

	const Queues& GetQueues() const
	{
		return queues;
	}

private:
	const VulkanContext& vulkanContext;

	VkDevice device;
	VkPhysicalDevice physicalDevice;

	Queues queues;

	mutable std::unordered_map<CommandBufferType, VkCommandPool> commandPools;

	VkCommandBuffer oneTimeCommandBuffer;
	CommandBufferSync oneTimeCommandBufferSync;
};