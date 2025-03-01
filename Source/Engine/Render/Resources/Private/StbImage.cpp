#include "Engine/Render/Resources/StbImage.hpp"

StbImage::StbImage(const FilePath& path)
{
    Assert(path.Exists());

    pixels = stbi_load(path.GetAbsolute().c_str(), &width, &height, &textureChannels, STBI_rgb_alpha);
    Assert(pixels != nullptr);
}

StbImage::~StbImage()
{
    if (pixels)
    {
        stbi_image_free(pixels);
    }
}

StbImage::StbImage(StbImage&& other) noexcept
    : pixels{ other.pixels }
    , width{ other.width }
    , height{ other.height }
    , textureChannels{ other.textureChannels }
{
    other.pixels = nullptr;
    other.width = 0;
    other.height = 0;
    other.textureChannels = 0;
}

StbImage& StbImage::operator=(StbImage&& other) noexcept
{
    if (this != &other)
    {
        std::swap(pixels, other.pixels);
        std::swap(width, other.width);
        std::swap(height, other.height);
        std::swap(textureChannels, other.textureChannels);
    }
    return *this;
}

StbImage::Pixels StbImage::GetPixels() const
{
    return { pixels, static_cast<size_t>(width) * height * 4 };
}

VkExtent3D StbImage::GetExtent() const
{
    return { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
}
