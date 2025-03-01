#pragma once

#include <volk.h>

class VulkanContext;

struct BufferDescription
{
    VkDeviceSize size = 0;
    VkBufferUsageFlags usage = {};
    VkMemoryPropertyFlags memoryProperties = {};
};

class Buffer
{
public:
    Buffer() = default;
    Buffer(BufferDescription description, bool createStagingBuffer, const VulkanContext& vulkanContext);

    template <typename T>
    Buffer(BufferDescription description, bool createStagingBuffer, const std::span<const T> initialData,
        const VulkanContext& vulkanContext);

    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    void CreateStagingBuffer();
    void DestroyStagingBuffer();

    template <typename T>
    void Fill(const std::span<const T> data);

    void Flush() const;

    std::span<std::byte> MapMemory(bool persistentMapping = false) const;
    void UnmapMemory() const;

    VkBuffer Get() const
    {
        return buffer;
    }
    
    const BufferDescription& GetDescription() const
    {
        return description;
    }

    const Buffer* GetStagingBuffer() const
    {
        return stagingBuffer.get();
    }

    const std::span<std::byte> GetMappedMemory() const
    {
        return mappedMemory;
    }
    
    bool IsValid() const
    {
        return buffer != VK_NULL_HANDLE;
    }

private:
    void FillImpl(const std::span<const std::byte> span);

    const VulkanContext* vulkanContext = nullptr;

    VkBuffer buffer = VK_NULL_HANDLE;
    
    BufferDescription description = {};

    std::unique_ptr<Buffer> stagingBuffer;

    mutable std::span<std::byte> mappedMemory;
    mutable bool persistentMapping = false; 
};

template <typename T>
Buffer::Buffer(BufferDescription description, bool createStagingBuffer, const std::span<const T> initialData,
    const VulkanContext& vulkanContext)
    : Buffer{ std::move(description), createStagingBuffer, vulkanContext }
{
    if (!initialData.empty())
    {
        Fill(initialData);
    }
}

template <typename T>
void Buffer::Fill(const std::span<const T> span)
{
    FillImpl(std::as_bytes(span));
}
