#include "Engine/Render/Resources/Image.hpp"

#include "Engine/Render/Vulkan/VulkanContext.hpp"

namespace ImageDetails
{
    static VkImage CreateImage(const ImageDescription& description, const VulkanContext& vulkanContext)
    {
        // TODO: move more of these as parameters
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = static_cast<uint32_t>(description.extent.width);
        imageInfo.extent.height = static_cast<uint32_t>(description.extent.height);
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = description.mipLevelsCount;
        imageInfo.arrayLayers = 1;
        imageInfo.format = description.format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = description.usage;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = description.samples;
        imageInfo.flags = 0; // Optional

        return vulkanContext.GetMemoryManager().CreateImage(imageInfo, description.memoryProperties);
    }
}

Image::Image(ImageDescription aDescription, const VulkanContext& aVulkanContext)
    : vulkanContext{ &aVulkanContext }
    , description{ aDescription }
{    
    using namespace ImageDetails;

    Assert(vulkanContext != nullptr);
 
    image = CreateImage(description, *vulkanContext);
}

Image::~Image()
{
    // TODO: Deletion queue
    if (image != VK_NULL_HANDLE)
    {
        vulkanContext->GetMemoryManager().DestroyImage(image);
        image = VK_NULL_HANDLE;
    }
}

Image::Image(Image&& other) noexcept
    : vulkanContext{ other.vulkanContext }
    , image{ other.image }
    , description{ other.description }
{
    other.vulkanContext = nullptr;
    other.image = VK_NULL_HANDLE;
    other.description = {};
}

Image& Image::operator=(Image&& other) noexcept
{
    if (this != &other)
    {
        std::swap(vulkanContext, other.vulkanContext);
        std::swap(image, other.image);
        std::swap(description, other.description);
    }
    return *this;
}
