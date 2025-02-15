#include "Engine/Scene/Scene.hpp"

#include "Engine/Scene/SceneHelpers.hpp"
#include "Engine/Render/Vulkan/VulkanContext.hpp"
#include "Engine/Render/Resources/ResourceHelpers.hpp"
#include "Engine/FileSystem/FileSystem.hpp"
#include "Utils/Helpers.hpp"

DISABLE_WARNINGS_BEGIN
#include <tiny_gltf.h>
DISABLE_WARNINGS_END

namespace SceneDetails
{
    static constexpr std::string_view imagePath = "~/Assets/texture.png";
}

Scene::Scene(FilePath aPath, const VulkanContext& aVulkanContext)
    : vulkanContext{ aVulkanContext }
    , root{ std::make_unique<SceneNode>() }
    , path{ std::move(aPath) }
{
    InitFromGltfScene();
    InitBuffers();
    InitTextureResources();
}

Scene::~Scene()
{
    VkDevice device = vulkanContext.GetDevice().GetVkDevice();
    
    VulkanHelpers::DestroySampler(device, sampler);
}

void Scene::InitFromGltfScene()
{
    using namespace SceneHelpers;

    std::unique_ptr<tinygltf::Model> gltfModel = LoadGltfScene(path);

    {
        ScopeTimer timer("Convert gltf scene");

        for (size_t i = 0; i < gltfModel->scenes[0].nodes.size(); i++)
        {
            root->children.emplace_back(LoadGltfHierarchy(gltfModel->nodes[i], *gltfModel, indices, vertices));
        }

        nodes = GetFlattenNodes(*root);

        AssignNodeIdsToPrimitives(nodes);
    }
}

void Scene::InitBuffers()
{
    std::span verticesSpan(std::as_const(vertices));
    std::span indicesSpan(std::as_const(indices));

    BufferDescription vertexBufferDescription{ .size = static_cast<VkDeviceSize>(verticesSpan.size_bytes()),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };

    vertexBuffer = Buffer(vertexBufferDescription, true, verticesSpan, &vulkanContext);

    BufferDescription indexBufferDescription{ .size = static_cast<VkDeviceSize>(indicesSpan.size_bytes()),
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };

    indexBuffer = Buffer(indexBufferDescription, true, indicesSpan, &vulkanContext);

    const std::vector<glm::mat4> transforms = SceneHelpers::GetBakedTransforms(*root);
    std::span transformsSpan(std::as_const(transforms));

    BufferDescription transformsBufferDescription{ .size = static_cast<VkDeviceSize>(transformsSpan.size_bytes()),
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };

    transformsBuffer = Buffer(transformsBufferDescription, true, transformsSpan, &vulkanContext);
}

void Scene::InitTextureResources()
{
    const Device& device = vulkanContext.GetDevice();

    const auto& [buffer, extent] = ResourceHelpers::LoadImageToBuffer(FilePath(SceneDetails::imagePath), vulkanContext);

    const int maxDimension = std::max(extent.width, extent.height);
    const uint32_t mipLevelsCount = static_cast<uint32_t>(std::floor(std::log2(maxDimension))) + 1;

    ImageDescription imageDescription{ .extent = extent, .mipLevelsCount = mipLevelsCount,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT ,
        .memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };

    image = Image(imageDescription, &vulkanContext);
    image.FillMipLevel0(buffer, true);

    imageView = ImageView(image, VK_IMAGE_ASPECT_COLOR_BIT, &vulkanContext);

    sampler = VulkanHelpers::CreateSampler(device.GetVkDevice(), device.GetPhysicalDeviceProperties(), mipLevelsCount);
}
