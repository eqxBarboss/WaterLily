#pragma once

#include <volk.h>

class VulkanContext;

enum class ShaderType
{
    eVertex,
    eFragment,
    eCompute,
};

class ShaderModule
{
public:
    ShaderModule(const VkShaderModule shaderModule, const ShaderType shaderType, const VulkanContext& aVulkanContext);
    ~ShaderModule();

    ShaderModule(const ShaderModule&) = delete;
    ShaderModule& operator=(const ShaderModule&) = delete;

    ShaderModule(ShaderModule&& other) noexcept;
    ShaderModule& operator=(ShaderModule&& other) noexcept;

    VkPipelineShaderStageCreateInfo GetVkPipelineShaderStageCreateInfo() const;

    bool IsValid() const;

private:
    const VulkanContext* vulkanContext = nullptr;

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    ShaderType shaderType;
};
