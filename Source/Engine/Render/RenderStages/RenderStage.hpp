#pragma once

#include "Engine/Render/Vulkan/Frame.hpp"

class VulkanContext;
struct RenderContext;

class RenderStage
{
public:
    RenderStage(const VulkanContext& vulkanContext, RenderContext& renderContext);
    virtual ~RenderStage();
    
    RenderStage(const RenderStage&) = delete;
    RenderStage& operator=(const RenderStage&) = delete;

    RenderStage(RenderStage&&) = delete;
    RenderStage& operator=(RenderStage&&) = delete;
    
    virtual void Execute(const Frame& frame);
    
    virtual void RecreateFramebuffers();
    virtual void TryReloadShaders();
    
protected:
    const VulkanContext* vulkanContext = nullptr;
    RenderContext* renderContext = nullptr;
};
