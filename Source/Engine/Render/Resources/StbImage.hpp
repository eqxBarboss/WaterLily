#pragma once

#include <volk.h>
#include <stb_image.h>

#include "Engine/FileSystem/FilePath.hpp"

// Now always RGBA8
class StbImage
{
public:
    using Pixels = std::span<const stbi_uc>;
    
    explicit StbImage(const FilePath& path);
    ~StbImage();
    
    StbImage(const StbImage&) = delete;
    StbImage& operator=(const StbImage&) = delete;

    StbImage(StbImage&& other) noexcept;
    StbImage& operator=(StbImage&& other) noexcept;
    
    Pixels GetPixels() const;
    
    VkExtent3D GetExtent() const;
    
private:
    stbi_uc* pixels = nullptr;
    
    int width = 0;
    int height = 0;
    
    int textureChannels = 0;
};
