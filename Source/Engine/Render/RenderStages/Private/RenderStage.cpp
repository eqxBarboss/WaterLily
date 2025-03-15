#include "Engine/Render/RenderStages/RenderStage.hpp"

#include "Engine/Render/RenderContext.hpp"
#include "Engine/Render/Vulkan/VulkanContext.hpp"

RenderStage::RenderStage(const VulkanContext& aVulkanContext, RenderContext& aRenderContext)
    : vulkanContext{ &aVulkanContext }
    , renderContext{ &aRenderContext }
{}

RenderStage::~RenderStage()
{}

void RenderStage::Execute(const Frame& frame)
{}

void RenderStage::RecreateFramebuffers()
{}

void RenderStage::TryReloadShaders()
{}
