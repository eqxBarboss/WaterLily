#pragma once

#include "Shaders/Common.h"
#include "Utils/Constants.hpp"
#include "Engine/Render/Vulkan/Buffer/Buffer.hpp"
#include "Engine/Render/Vulkan/Image/RenderTarget.hpp"
#include "Engine/Render/Vulkan/DescriptorSets/DescriptorSetLayout.hpp"

struct RenderContext
{
    RenderTarget colorTarget;
    RenderTarget depthTarget;

    gpu::PushConstants globals = { .view = Matrix4::identity, .projection = Matrix4::identity, .viewPos = Vector3::zero };

    // TODO: Initialize after scene initialization in the proper stage and do not just take from scene itself
    Buffer vertexBuffer;
    Buffer indexBuffer;
    Buffer transformBuffer;
    Buffer meshletDataBuffer;
    Buffer meshletBuffer;
    Buffer primitiveBuffer;
    Buffer drawBuffer;
    Buffer indirectBuffer;
    
    uint32_t drawCount = 0;
    uint32_t indirectDrawCount = 0;
    
    std::vector<VkDescriptorSet> globalDescriptors;
    DescriptorSetLayout globalDescriptorSetLayout;
};
