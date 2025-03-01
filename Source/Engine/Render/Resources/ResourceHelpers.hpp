#pragma once

#include <volk.h>
#include <glm/glm.hpp>

#include "Engine/Render/Resources/Buffer.hpp"
#include "Engine/Render/Resources/Image.hpp"

struct ImageLayoutTransition
{
    VkImageLayout srcLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout dstLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    
    inline bool operator==(const ImageLayoutTransition& other) const
    {
        return srcLayout == other.srcLayout && dstLayout == other.dstLayout;
    }
    
    inline bool operator!=(const ImageLayoutTransition& other) const
    {
        return !(*this == other);
    }
};

namespace ImageLayoutTransitions
{
    constexpr ImageLayoutTransition undefinedToGeneral = { VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL };
    constexpr ImageLayoutTransition undefinedToDstOptimal = { VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL };
    constexpr ImageLayoutTransition generalToSrcOptimal = { VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL };
    constexpr ImageLayoutTransition dstOptimalToSrcOptimal = { VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL };
    constexpr ImageLayoutTransition srcOptimalToShaderReadOnlyOptimal = { VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    constexpr ImageLayoutTransition dstOptimalToShaderReadOnlyOptimal = { VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
}

namespace ResourceHelpers
{
    void TransitionLayout(VkCommandBuffer commandBuffer, const Image& image, ImageLayoutTransition transition);
    void TransitionLayout(VkCommandBuffer commandBuffer, VkImage image, ImageLayoutTransition transition, uint32_t baseMipLevel, uint32_t mipLevelsCount = 1);

    void BlitImageToImage(VkCommandBuffer commandBuffer, const Image& source, const Image& destination);
    void BlitImageToImage(VkCommandBuffer commandBuffer, const Image& source, VkImage destination, VkExtent3D destinationExtent);
    void BlitImageToImage(VkCommandBuffer commandBuffer, VkImage source, VkImage destination, VkOffset3D sourceOffset,
        VkOffset3D destinationOffset, uint32_t sourceMipLevel, uint32_t destinationMipLevel);
    // Only mip 0 for now
    void CopyBufferToImage(const VkCommandBuffer commandBuffer, const Buffer& buffer, const Image& image);
    void GenerateMipMaps(const VkCommandBuffer commandBuffer, const Image& image);
    void FillImage(const VkCommandBuffer commandBuffer, const Image& image, const glm::vec4& color);
}
