#include "Engine/Render/Resources/ResourceHelpers.hpp"

namespace ResourceHelpersDetails
{
    static inline VkOffset3D ToVkOffset3D(const VkExtent3D& extent)
    {
        return { static_cast<int32_t>(extent.width), static_cast<int32_t>(extent.height),
            static_cast<int32_t>(extent.depth) };
    }

    static VkExtent3D GetMipExtent(const Image& image, const uint32_t mipLevel)
    {
        Assert(mipLevel < image.GetDescription().mipLevelsCount);
        
        VkExtent3D extent = image.GetDescription().extent;
        
        for (uint32_t i = 0; i < mipLevel; ++i)
        {
            extent.width = extent.width > 1 ? extent.width / 2 : 1;
            extent.height = extent.height > 1 ? extent.height / 2 : 1;
            extent.depth = extent.depth > 1 ? extent.depth / 2 : 1;
        }
        
        return extent;
    }
}

void ResourceHelpers::TransitionLayout(const VkCommandBuffer commandBuffer, const Image& image,
    const ImageLayoutTransition transition)
{
    TransitionLayout(commandBuffer, image.Get(), transition, 0, image.GetDescription().mipLevelsCount);
}

void ResourceHelpers::TransitionLayout(const VkCommandBuffer commandBuffer, const VkImage image,
    const ImageLayoutTransition transition, const uint32_t baseMipLevel, const uint32_t mipLevelsCount /* = 1 */)
{
    VkAccessFlags srcAccessMask{};
    VkAccessFlags dstAccessMask{};
    VkPipelineStageFlags sourceStage{};
    VkPipelineStageFlags destinationStage{};
    
    if (transition == ImageLayoutTransitions::undefinedToGeneral)
    {
        srcAccessMask = 0;
        dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    else if (transition == ImageLayoutTransitions::undefinedToDstOptimal)
    {
        srcAccessMask = 0;
        dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (transition == ImageLayoutTransitions::generalToSrcOptimal)
    {
        srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (transition == ImageLayoutTransitions::dstOptimalToShaderReadOnlyOptimal)
    {
        srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (transition == ImageLayoutTransitions::dstOptimalToSrcOptimal)
    {
        srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (transition == ImageLayoutTransitions::srcOptimalToShaderReadOnlyOptimal)
    {
        srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        Assert(false);
    }

    VkImageMemoryBarrier barrier = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.oldLayout = transition.srcLayout;
    barrier.newLayout = transition.dstLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = baseMipLevel;
    barrier.subresourceRange.levelCount = mipLevelsCount;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void ResourceHelpers::BlitImageToImage(const VkCommandBuffer commandBuffer, const Image& source, const Image& destination)
{
    const VkOffset3D sourceOffset = ResourceHelpersDetails::ToVkOffset3D(source.GetDescription().extent);
    const VkOffset3D destinationOffset = ResourceHelpersDetails::ToVkOffset3D(destination.GetDescription().extent);
    
    BlitImageToImage(commandBuffer, source.Get(), destination.Get(), sourceOffset, destinationOffset, 0, 0);
}

void ResourceHelpers::BlitImageToImage(const VkCommandBuffer commandBuffer, const Image& source, const VkImage destination,
    VkExtent3D destinationExtent)
{
    const VkOffset3D sourceOffset = ResourceHelpersDetails::ToVkOffset3D(source.GetDescription().extent);
    const VkOffset3D destinationOffset = ResourceHelpersDetails::ToVkOffset3D(destinationExtent);
    
    BlitImageToImage(commandBuffer, source.Get(), destination, sourceOffset, destinationOffset, 0, 0);
}

void ResourceHelpers::BlitImageToImage(const VkCommandBuffer commandBuffer, const VkImage source, const VkImage destination,
    const VkOffset3D sourceOffset, const VkOffset3D destinationOffset, const uint32_t sourceMipLevel,
    const uint32_t destinationMipLevel)
{    
    VkImageBlit blit = {};
    blit.srcOffsets[0] = { 0, 0, 0 };
    blit.srcOffsets[1] = sourceOffset;
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = sourceMipLevel;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    
    blit.dstOffsets[0] = { 0, 0, 0 };
    blit.dstOffsets[1] = destinationOffset;
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = destinationMipLevel;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;

    vkCmdBlitImage(commandBuffer, source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destination,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
    
//    // !!!!!!!!!!!!!!!!!!!!!!!!!!
//    VkImageBlit2 blitRegion{ .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2 };
//
//    blitRegion.srcOffsets[1] = sourceOffset;
//    blitRegion.dstOffsets[1] = destinationOffset;
//
//    blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//    blitRegion.srcSubresource.baseArrayLayer = 0;
//    blitRegion.srcSubresource.layerCount = 1;
//    blitRegion.srcSubresource.mipLevel = sourceMipLevel;
//
//    blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//    blitRegion.dstSubresource.baseArrayLayer = 0;
//    blitRegion.dstSubresource.layerCount = 1;
//    blitRegion.dstSubresource.mipLevel = destinationMipLevel;
//
//    VkBlitImageInfo2 blitInfo{ .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2 };
//    blitInfo.dstImage = destination.Get();
//    blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//    blitInfo.srcImage = source.Get();
//    blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
//    blitInfo.filter = VK_FILTER_LINEAR;
//    blitInfo.regionCount = 1;
//    blitInfo.pRegions = &blitRegion;
//
//    vkCmdBlitImage2(commandBuffer, &blitInfo);
}

void ResourceHelpers::CopyBufferToImage(const VkCommandBuffer commandBuffer, const Buffer& buffer, const Image& image)
{
    // TODO: assert on size inequality?
    Assert((buffer.GetDescription().usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) == VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    Assert((image.GetDescription().usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    
    TransitionLayout(commandBuffer, image, ImageLayoutTransitions::undefinedToDstOptimal);
    
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = image.GetDescription().extent;

    vkCmdCopyBufferToImage(commandBuffer, buffer.Get(), image.Get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void ResourceHelpers::GenerateMipMaps(const VkCommandBuffer commandBuffer, const Image& image)
{
    using namespace ResourceHelpersDetails;
    
    const uint32_t mipLevelsCount = image.GetDescription().mipLevelsCount;
    
    Assert(mipLevelsCount > 1);

    const auto generateNextMipLevel = [&](uint32_t sourceMip) {
        TransitionLayout(commandBuffer, image.Get(), ImageLayoutTransitions::dstOptimalToSrcOptimal, sourceMip);
        BlitImageToImage(commandBuffer, image.Get(), image.Get(), ToVkOffset3D(GetMipExtent(image, sourceMip)),
            ToVkOffset3D(GetMipExtent(image, sourceMip + 1)), sourceMip, sourceMip + 1);
    };
    
    std::ranges::for_each(std::views::iota(static_cast<uint32_t>(0), image.GetDescription().mipLevelsCount - 1),
        generateNextMipLevel);
    
    TransitionLayout(commandBuffer, image.Get(), ImageLayoutTransitions::dstOptimalToSrcOptimal, mipLevelsCount - 1);
}

void ResourceHelpers::FillImage(const VkCommandBuffer commandBuffer, const Image& image, const glm::vec4& color)
{
    const VkClearColorValue clearValue = {
        .float32[0] = color.r, .float32[1] = color.g, .float32[2] = color.b, .float32[3] = color.a };
    
    const VkImageSubresourceRange clearRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    vkCmdClearColorImage(commandBuffer, image.Get(), VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
}
