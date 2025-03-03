#pragma once

#include "Engine/Window.hpp"
#include "Engine/Render/Vulkan/Resources/Image.hpp"
#include "Engine/Render/Vulkan/Resources/ImageView.hpp"

#include <volk.h>

class VulkanContext;

struct SwapchainSupportDetails
{
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class Swapchain
{
public:
    Swapchain(const VkExtent2D& requiredExtentInPixels, const VulkanContext& aVulkanContext);
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    Swapchain(Swapchain&&) = delete;
    Swapchain& operator=(Swapchain&&) = delete;

    // Call in case of simple resize (not when window's been recreated!)
    void Recreate(const VkExtent2D& requiredExtentInPixels);

    VkSwapchainKHR GetVkSwapchainKHR() const
    {
        return swapchain;
    }

    VkSurfaceFormatKHR GetSurfaceFormat() const
    {
        return surfaceFormat;
    }

    VkExtent2D GetExtent() const
    {
        return extent;
    }

    const std::vector<Image>& GetImages() const
    {
        return images;
    }

    const std::vector<ImageView>& GetImageViews() const
    {
        return imageViews;
    }

private:
    void Create(const VkExtent2D& requiredExtentInPixels);
    void Cleanup();
    
    ImageDescription GetImageDescription() const;

    const VulkanContext& vulkanContext;
    
    SwapchainSupportDetails supportDetails;

    VkSwapchainKHR swapchain;

    VkSurfaceFormatKHR surfaceFormat; 
    VkExtent2D extent;

    // TODO: Render target???
    std::vector<Image> images;
    std::vector<ImageView> imageViews;
};
