#include "Engine/Render/Vulkan/Swapchain.hpp"

#include "Engine/Render/Vulkan/VulkanContext.hpp"
#include "Engine/Render/Vulkan/VulkanUtils.hpp"

namespace SwapchainDetails
{
    static constexpr ImageDescription staticSwapchainImageDescription = {
        .mipLevelsCount = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        // Runtime params:
        // VkExtent3D extent
        // VkFormat format
    };

    static VkSurfaceCapabilitiesKHR GetSurfaceCapabilities(const VulkanContext& vulkanContext)
    {
        const VkPhysicalDevice physicalDevice = vulkanContext.GetDevice().GetPhysicalDevice();
        const VkSurfaceKHR surface = vulkanContext.GetSurface();

        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);
        return capabilities;
    }
    
    static SwapchainSupportDetails GetSwapchainSupportDetails(const VulkanContext& vulkanContext)
    {
        const VkPhysicalDevice physicalDevice = vulkanContext.GetDevice().GetPhysicalDevice();
        const VkSurfaceKHR surface = vulkanContext.GetSurface();

        SwapchainSupportDetails details;
        
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        Assert(formatCount != 0);
    
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
        Assert(presentModeCount != 0);
                
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount,
            details.presentModes.data());
        
        return details;
    }

    static VkSurfaceFormatKHR SelectSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
    {
        Assert(!availableFormats.empty());

        const auto isPreferredSurfaceFormat = [](const auto& surfaceFormat) {
            return surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
                surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        };

        const auto it = std::ranges::find_if(availableFormats, isPreferredSurfaceFormat);

        if (it != availableFormats.end())
        {
            return *it;
        }
        
        return availableFormats.front();
    }

    static VkPresentModeKHR SelectPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
    {
        const auto isPreferredPresentMode = [](const auto& presentMode) {
            return presentMode == VK_PRESENT_MODE_MAILBOX_KHR;
        };

        const auto it = std::ranges::find_if(availablePresentModes, isPreferredPresentMode);

        if (it != availablePresentModes.end())
        {
            return *it;
        }
        
        // This one is guaranteed to be supported
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    static VkExtent2D SelectExtent(const VkSurfaceCapabilitiesKHR& capabilities, const VkExtent2D& requiredExtentInPixels)
    {
        // currentExtent is the current width and height of the surface, or the special value (0xFFFFFFFF, 0xFFFFFFFF)
        // indicating that the surface size will be determined by the extent of a swapchain targeting the surface.
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }
        else
        {
            const VkExtent2D actualExtent =
            {
                std::clamp(requiredExtentInPixels.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
                std::clamp(requiredExtentInPixels.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
            };
            
            return actualExtent;
        }
    }

    static uint32_t SelectImageCount(const VkSurfaceCapabilitiesKHR& capabilities)
    {
        uint32_t imageCount = capabilities.minImageCount + 1;

        // A value of 0 means that there is no limit on the number of images,
        // though there may be limits related to the total amount of memory used by presentable images.
        if (capabilities.maxImageCount != 0 && imageCount > capabilities.maxImageCount)
        {
            imageCount = capabilities.maxImageCount;
        }

        return imageCount;
    }

    static VkSwapchainKHR CreateSwapchain(const VulkanContext& vulkanContext, const SwapchainSupportDetails& supportDetails, 
        const VkSurfaceCapabilitiesKHR& capabilities, const VkSurfaceFormatKHR surfaceFormat, const VkExtent2D extent)
    {
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = vulkanContext.GetSurface();
        createInfo.minImageCount = SelectImageCount(capabilities);
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = staticSwapchainImageDescription.usage;

        const QueueFamilyIndices& familyIndices = vulkanContext.GetDevice().GetQueues().familyIndices;
        const uint32_t queueFamilyIndices[] = { familyIndices.graphicsAndComputeFamily, familyIndices.presentFamily };

        if (familyIndices.graphicsAndComputeFamily != familyIndices.presentFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        createInfo.preTransform = capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = SelectPresentMode(supportDetails.presentModes);
        createInfo.clipped = VK_TRUE;
        // TODO: Create swapchain with the use of the old one
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        VkSwapchainKHR swapchain;
        const VkResult result = vkCreateSwapchainKHR(vulkanContext.GetDevice(), &createInfo, nullptr, &swapchain);
        Assert(result == VK_SUCCESS);

        return swapchain;
    }

    static std::vector<VkImage> GetSwapchainImages(VkDevice device, VkSwapchainKHR swapchain)
    {
        uint32_t imageCount;
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);

        std::vector<VkImage> images(imageCount);
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data());
        
        return images;
    }

    static std::vector<RenderTarget> CreateRenderTargets(VkSwapchainKHR swapchain, ImageDescription description,
        const VulkanContext& vulkanContext)
    {
        const std::vector<VkImage> images = GetSwapchainImages(vulkanContext.GetDevice(), swapchain);
        
        std::vector<RenderTarget> renderTargets;
        renderTargets.reserve(images.size());
        
        std::ranges::transform(images, std::back_inserter(renderTargets), [&](const VkImage image) {
            return RenderTarget(image, description, VK_IMAGE_ASPECT_COLOR_BIT, vulkanContext, true);
        });
        
        return renderTargets;
    }
}

Swapchain::Swapchain(const VkExtent2D& requiredExtentInPixels, const VulkanContext& aVulkanContext)
    : vulkanContext{aVulkanContext}
{
    using namespace SwapchainDetails;

    supportDetails = GetSwapchainSupportDetails(vulkanContext);
    surfaceFormat = SelectSurfaceFormat(supportDetails.formats);

    Create(requiredExtentInPixels);
}

Swapchain::~Swapchain()
{
    Cleanup();
}

void Swapchain::Recreate(const VkExtent2D& requiredExtentInPixels)
{
    Cleanup();
    Create(requiredExtentInPixels);
}

void Swapchain::Create(const VkExtent2D& requiredExtentInPixels)
{
    using namespace SwapchainDetails;

    const VkSurfaceCapabilitiesKHR surfaceCapabilities = GetSurfaceCapabilities(vulkanContext);
    extent = SelectExtent(surfaceCapabilities, requiredExtentInPixels);

    swapchain = CreateSwapchain(vulkanContext, supportDetails, surfaceCapabilities, surfaceFormat, extent);

    renderTargets = CreateRenderTargets(swapchain, GetRenderTargetDescription(), vulkanContext);
}

void Swapchain::Cleanup()
{
    renderTargets.clear();

    vkDestroySwapchainKHR(vulkanContext.GetDevice(), swapchain, nullptr);
}

ImageDescription Swapchain::GetRenderTargetDescription() const
{
    ImageDescription swapchainImageDescription = SwapchainDetails::staticSwapchainImageDescription;
    swapchainImageDescription.extent = { extent.width, extent.height, 1 };
    swapchainImageDescription.format = surfaceFormat.format;
    
    return swapchainImageDescription;
}
