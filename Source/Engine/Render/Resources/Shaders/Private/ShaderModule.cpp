#include "Engine/Render/Resources/Shaders/ShaderModule.hpp"

#include "Engine/Render/Vulkan/VulkanContext.hpp"

namespace ShaderModuleDetails
{
    static VkShaderStageFlagBits GetVkShaderStageFlagBits(const ShaderType shaderType)
    {
        VkShaderStageFlagBits result{};
        
        switch (shaderType)
        {
            case ShaderType::eVertex:
                result = VK_SHADER_STAGE_VERTEX_BIT;
                break;
            case ShaderType::eFragment:
                result = VK_SHADER_STAGE_FRAGMENT_BIT;
                break;
            case ShaderType::eCompute:
                result = VK_SHADER_STAGE_COMPUTE_BIT;
                break;
        }

        return result;
    }
}

ShaderModule::ShaderModule(const VkShaderModule aShaderModule, const ShaderType aShaderType, 
    const VulkanContext& aVulkanContext)
    : vulkanContext{ &aVulkanContext }
    , shaderModule{ aShaderModule }
    , shaderType{ aShaderType }
{}

ShaderModule::~ShaderModule()
{
    if (shaderModule != VK_NULL_HANDLE)
    { 
        vkDestroyShaderModule(vulkanContext->GetDevice().GetVkDevice(), shaderModule, nullptr);
    }    
}

ShaderModule::ShaderModule(ShaderModule&& other) noexcept
    : vulkanContext{ other.vulkanContext }
    , shaderModule{ other.shaderModule }
    , shaderType{ other.shaderType }
{
    other.shaderModule = VK_NULL_HANDLE;
}

ShaderModule& ShaderModule::operator=(ShaderModule&& other) noexcept
{
    if (this != &other)
    {
        std::swap(vulkanContext, other.vulkanContext);
        std::swap(shaderModule, other.shaderModule);
        std::swap(shaderType, other.shaderType);
    }    
    return *this;
}

VkPipelineShaderStageCreateInfo ShaderModule::GetVkPipelineShaderStageCreateInfo() const
{
    Assert(IsValid());

    VkPipelineShaderStageCreateInfo shaderStageCreateInfo{};
    shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCreateInfo.stage = ShaderModuleDetails::GetVkShaderStageFlagBits(shaderType);
    shaderStageCreateInfo.module = shaderModule;
    shaderStageCreateInfo.pName = "main"; // Let's use "main" entry point by default

    return shaderStageCreateInfo;
}

bool ShaderModule::IsValid() const
{
    return shaderModule != VK_NULL_HANDLE;
}
