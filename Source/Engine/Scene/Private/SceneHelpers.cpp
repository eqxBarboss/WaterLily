#include "Engine/Scene/SceneHelpers.hpp"

#include "Utils/Helpers.hpp"

DISABLE_WARNINGS_BEGIN
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#include <glm/gtc/type_ptr.hpp>
DISABLE_WARNINGS_END

#include <meshoptimizer.h>

namespace SceneHelpersDetails
{
    static constexpr size_t positionComponents = 3;
    static constexpr size_t normalComponents = 3;
    static constexpr size_t tangentComponents = 4;
    static constexpr size_t uvComponents = 2;
    static constexpr size_t colorComponents = 4;

    static constexpr size_t vertexSize = sizeof(gpu::Vertex);

    constexpr float coneWeight = 0.0f; // 0.25 will be default when we start using cone culling

    static void LoadVertices(const cgltf_primitive& primitive, std::span<gpu::Vertex> vertices)
    {
        const size_t vertexCount = vertices.size();

        Assert(primitive.attributes[0].data->count == vertexCount);

        std::vector<float> rawData(vertexCount * 4);

        if (const cgltf_accessor* positionAccessor = cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0))
        {
            Assert(cgltf_num_components(positionAccessor->type) == positionComponents);

            cgltf_accessor_unpack_floats(positionAccessor, rawData.data(), vertexCount * positionComponents);

            for (size_t i = 0; i < vertexCount; ++i)
            {
                vertices[i].posAndU = glm::vec4(rawData[i * 3 + 0], rawData[i * 3 + 1], rawData[i * 3 + 2], 0.0f);
            }
        }

        if (const cgltf_accessor* normalAccessor = cgltf_find_accessor(&primitive, cgltf_attribute_type_normal, 0))
        {
            Assert(cgltf_num_components(normalAccessor->type) == normalComponents);

            cgltf_accessor_unpack_floats(normalAccessor, rawData.data(), vertexCount * normalComponents);

            for (size_t i = 0; i < vertexCount; ++i)
            {
                vertices[i].normalAndV = glm::vec4(rawData[i * 3 + 0], rawData[i * 3 + 1], rawData[i * 3 + 2], 0.0f);
            }
        }

        if (const cgltf_accessor* tangentAccessor = cgltf_find_accessor(&primitive, cgltf_attribute_type_tangent, 0))
        {
            Assert(cgltf_num_components(tangentAccessor->type) == tangentComponents);

            cgltf_accessor_unpack_floats(tangentAccessor, rawData.data(), vertexCount * tangentComponents);

            for (size_t i = 0; i < vertexCount; ++i)
            {
                vertices[i].tangent = glm::vec4(rawData[i * 4 + 0], rawData[i * 4 + 1], rawData[i * 4 + 2], 
                    rawData[i * 4 + 3]);
            }
        }

        if (const cgltf_accessor* uvAccessor = cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, 0))
        {
            Assert(cgltf_num_components(uvAccessor->type) == uvComponents);

            cgltf_accessor_unpack_floats(uvAccessor, rawData.data(), vertexCount * uvComponents);

            for (size_t i = 0; i < vertexCount; ++i)
            {
                vertices[i].posAndU.w = rawData[i * 2 + 0];
                vertices[i].normalAndV.w = rawData[i * 2 + 1];
            }
        }

        if (const cgltf_accessor* colorAccessor = cgltf_find_accessor(&primitive, cgltf_attribute_type_color, 0))
        {
            Assert(cgltf_num_components(colorAccessor->type) == colorComponents);

            cgltf_accessor_unpack_floats(colorAccessor, rawData.data(), vertexCount * colorComponents);

            for (size_t i = 0; i < vertexCount; ++i)
            {
                vertices[i].color = glm::vec4(rawData[i * 4 + 0], rawData[i * 4 + 1], rawData[i * 4 + 2], 
                    rawData[i * 4 + 3]);
            }
        }
    }

    static void LoadIndices(const cgltf_primitive& primitive, std::span<uint32_t> indices)
    {
        Assert(primitive.indices->count == indices.size());

        cgltf_accessor_unpack_indices(primitive.indices, indices.data(), sizeof(uint32_t), indices.size());
    }

    static size_t OptimizePrimitive(std::span<gpu::Vertex>& vertices, std::span<uint32_t> indices)
    {
        using namespace SceneHelpersDetails;

        std::vector<uint32_t> remap(vertices.size());
        const size_t uniqueVertices = meshopt_generateVertexRemap(remap.data(), indices.data(), indices.size(),
            vertices.data(), vertices.size(), vertexSize);

        meshopt_remapVertexBuffer(vertices.data(), vertices.data(), vertices.size(), vertexSize, remap.data());
        meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), remap.data());

        const size_t removedVertices = vertices.size() - uniqueVertices;
        vertices = vertices.first(uniqueVertices);

        meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertices.size());

        meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indices.size(), vertices.data(), vertices.size(), vertexSize);

        return removedVertices;
    }

    static gpu::Meshlet GenerateMeshlet(const meshopt_Meshlet& meshlet, const std::vector<unsigned int>& vertices,
        const std::vector<unsigned char>& triangles, std::vector<uint32_t>& meshletData,
        const uint32_t baseVertexIndex = 0)
    {
        const auto dataOffset = static_cast<uint32_t>(meshletData.size());

        auto meshletVertices = 
            vertices | std::views::drop(meshlet.vertex_offset) | std::views::take(meshlet.vertex_count);

        const auto& [minVertexIt, maxVertexIt] = std::ranges::minmax_element(meshletVertices);
        const auto [minVertex, maxVertex] = std::tie(*minVertexIt, *maxVertexIt);

        const bool bShortVertexOffsets = maxVertex - minVertex < (1 << 16);

        for (unsigned int i = 0; i < meshlet.vertex_count; ++i)
        {
            const unsigned int vertexOffset = meshletVertices[i] - minVertex;

            if (bShortVertexOffsets && i % 2)
            {
                meshletData.back() |= vertexOffset << 16;
            }
            else
            {
                meshletData.push_back(vertexOffset);
            }
        }

        const auto triangleIndexGroups = reinterpret_cast<const unsigned int*>(&triangles[meshlet.triangle_offset]);
        const unsigned int triangleIndexGroupsCount = (meshlet.triangle_count * 3 + 3) / 4;

        for (unsigned int i = 0; i < triangleIndexGroupsCount; ++i)
        {
            meshletData.push_back(triangleIndexGroups[i]);
        }

        return {
            .dataOffset = dataOffset,
            .baseVertexIndex = baseVertexIndex + minVertex,
            .vertexCount = static_cast<uint8_t>(meshlet.vertex_count),
            .triangleCount = static_cast<uint8_t>(meshlet.triangle_count),
            .bShortVertexOffsets = bShortVertexOffsets, };
    }

    static size_t GenerateMeshlets(const std::span<const gpu::Vertex> vertices, const std::span<const uint32_t> indices, 
        std::vector<gpu::Meshlet>& meshlets, std::vector<uint32_t>& meshletData,
        const uint32_t baseVertexIndex = 0 /* index of the 1st meshlet vertex in global vertex buffer */)
    {
        using namespace SceneHelpersDetails;

        std::vector<meshopt_Meshlet> meshoptMeshlets(indices.size() / 3);
        // These are actually indices to the original vertex array, but that's meshoptimizer naming
        std::vector<unsigned int> meshletVertices(meshoptMeshlets.size() * gpu::maxMeshletVertices);
        std::vector<unsigned char> meshletTriangles(meshoptMeshlets.size() * gpu::maxMeshletTriangles * 3);

        std::vector<glm::vec3> positions;
        positions.reserve(vertices.size());

        // meshopt_buildMeshlets needs positions, not vertices
        std::ranges::transform(vertices, std::back_inserter(positions), [](const gpu::Vertex& v)
        {
            return glm::vec3(v.posAndU.x, v.posAndU.y, v.posAndU.z);
        });

        const size_t meshletCount = meshopt_buildMeshlets(meshoptMeshlets.data(), meshletVertices.data(),
            meshletTriangles.data(), indices.data(), indices.size(), &positions[0].x, positions.size(), 
            sizeof(glm::vec3), gpu::maxMeshletVertices, gpu::maxMeshletTriangles, coneWeight);

        meshoptMeshlets.resize(meshletCount);
        meshletVertices.resize(meshletCount * gpu::maxMeshletVertices);
        meshletTriangles.resize(meshletCount * gpu::maxMeshletTriangles * 3);

        for (const meshopt_Meshlet& meshlet : meshoptMeshlets)
        {
            meshopt_optimizeMeshlet(&meshletVertices[meshlet.vertex_offset], &meshletTriangles[meshlet.triangle_offset], 
                meshlet.triangle_count, meshlet.vertex_count);

            meshlets.push_back(GenerateMeshlet(meshlet, meshletVertices, meshletTriangles, meshletData, 
                baseVertexIndex));
        }

        return meshoptMeshlets.size();
    }

    // Returns mapping from gltf mesh to mesh in our raw scene
    static std::unordered_map<size_t, size_t> ProcessGeometry(const cgltf_data& gltfData, RawScene& rawScene)
    {
        std::unordered_map<size_t, size_t> gltfMeshToMesh;

        for (size_t i = 0; i < gltfData.meshes_count; ++i)
        {
            const cgltf_mesh& mesh = gltfData.meshes[i];

            uint32_t primitiveCount = 0;

            for (size_t j = 0; j < mesh.primitives_count; ++j)
            {
                const cgltf_primitive& primitive = mesh.primitives[j];

                if (primitive.type != cgltf_primitive_type_triangles || !primitive.indices)
                {
                    continue;
                }

                const auto baseVertexIndex = static_cast<uint32_t>(rawScene.vertices.size());

                const size_t vertexCount = primitive.attributes[0].data->count;
                const size_t indexCount = primitive.indices->count;

                rawScene.vertices.resize(rawScene.vertices.size() + vertexCount);
                rawScene.indices.resize(rawScene.indices.size() + indexCount);

                auto vertices = std::span(rawScene.vertices).last(vertexCount);
                auto indices = std::span(rawScene.indices).last(indexCount);

                LoadVertices(primitive, vertices);
                LoadIndices(primitive, indices);

                const size_t removedVertices = OptimizePrimitive(vertices, indices);

                rawScene.vertices.resize(rawScene.vertices.size() - removedVertices);

                const auto meshletCount = GenerateMeshlets(vertices, indices, rawScene.meshlets, rawScene.meshletData,
                    baseVertexIndex);

                rawScene.primitives.emplace_back(static_cast<uint32_t>(rawScene.indices.size() - indexCount), 
                    static_cast<uint32_t>(indexCount), static_cast<uint32_t>(baseVertexIndex),
                    static_cast<uint32_t>(rawScene.meshlets.size() - meshletCount), 
                    static_cast<uint32_t>(meshletCount));

                ++primitiveCount;
            }

            if (primitiveCount != 0)
            {
                gltfMeshToMesh.emplace(i, rawScene.meshes.size());
                rawScene.meshes.emplace_back(static_cast<uint32_t>(rawScene.primitives.size() - primitiveCount), 
                    primitiveCount);
            }
        }

        return gltfMeshToMesh;
    }

    static void LoadTransforms(const cgltf_data& gltfData, RawScene& rawScene, 
        const std::unordered_map<size_t, size_t>& gltfMeshToMesh)
    {
        for (size_t i = 0; i < gltfData.nodes_count; ++i)
        {
            const cgltf_node& node = gltfData.nodes[i];

            if (node.mesh && gltfMeshToMesh.contains(cgltf_mesh_index(&gltfData, node.mesh)))
            {
                Mesh& mesh = rawScene.meshes[gltfMeshToMesh.at(cgltf_mesh_index(&gltfData, node.mesh))];

                float matrix[16];

                cgltf_node_transform_world(&node, matrix);

                rawScene.transforms.push_back(glm::make_mat4x4(matrix));

                mesh.transformIndex = static_cast<uint32_t>(rawScene.transforms.size() - 1);
            }
        }
    }

    static void GenerateGpuPrimitivesAndDraws(RawScene& rawScene)
    {
        for (const Mesh& mesh : rawScene.meshes)
        {
            for (const uint32_t primitiveIndex : std::ranges::views::iota(mesh.firstPrimitiveIndex,
                mesh.firstPrimitiveIndex + mesh.primitiveCount))
            {
                const Primitive& primitive = rawScene.primitives[primitiveIndex];

                // Mesh pipeline draw data
                rawScene.gpuPrimitives.emplace_back(mesh.transformIndex, primitive.materialIndex);

                uint32_t remainingMeshlets = primitive.meshletCount;
                uint32_t firstMeshletIndex = primitive.firstMeshletIndex;

                while (remainingMeshlets > 0)
                {
                    const uint32_t meshletCount = std::min(remainingMeshlets, gpu::meshWgSize);

                    rawScene.draws.emplace_back(firstMeshletIndex, meshletCount, primitiveIndex, 0);

                    firstMeshletIndex += meshletCount;
                    remainingMeshlets -= meshletCount;
                }

                // Vertex pipeline draw data
                rawScene.indirectCommands.emplace_back(primitive.indexCount, 1, primitive.firstIndex, 
                    primitive.vertexOffset, mesh.transformIndex);
            }
        }
    }
}

std::optional<RawScene> SceneHelpers::LoadGltfScene(const FilePath& path)
{
    using namespace SceneHelpersDetails;

    RawScene rawScene;

    const std::string absolutePath = path.GetAbsolute();

    cgltf_options options = {};
    cgltf_data* gltfData = nullptr;

    {
        ScopeTimer timer("Parse gltf scene");

        if (cgltf_parse_file(&options, absolutePath.data(), &gltfData) != cgltf_result_success)
        {
            return std::nullopt;
        }
    }

    std::unique_ptr<cgltf_data, void (*)(cgltf_data*)> gltfDataPtr(gltfData, &cgltf_free);

    {
        ScopeTimer timer("Load gltf scene buffers & validate");

        if (cgltf_load_buffers(&options, gltfData, absolutePath.data()) != cgltf_result_success)
        {
            return std::nullopt;
        }

        if (cgltf_validate(gltfData) != cgltf_result_success)
        {
            return std::nullopt;
        }
    }

    {
        ScopeTimer timer("Process gltf scene");

        std::unordered_map<size_t, size_t> gltfMeshToMesh = ProcessGeometry(*gltfData, rawScene);

        LoadTransforms(*gltfData, rawScene, gltfMeshToMesh);

        GenerateGpuPrimitivesAndDraws(rawScene);
    }

    return rawScene;
}

std::vector<VkVertexInputBindingDescription> SceneHelpers::GetVertexBindings()
{
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(gpu::Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return { binding };
}

// TODO: (low priority) parse from compiled shader file
std::vector<VkVertexInputAttributeDescription> SceneHelpers::GetVertexAttributes()
{
    std::vector<VkVertexInputAttributeDescription> attributes(4);

    VkVertexInputAttributeDescription& posAttribute = attributes[0];
    posAttribute.binding = 0;
    posAttribute.location = 0;
    posAttribute.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    posAttribute.offset = offsetof(gpu::Vertex, posAndU);

    VkVertexInputAttributeDescription& normalAttribute = attributes[1];
    normalAttribute.binding = 0;
    normalAttribute.location = 1;
    normalAttribute.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    normalAttribute.offset = offsetof(gpu::Vertex, normalAndV);

    VkVertexInputAttributeDescription& tangentAttribute = attributes[2];
    tangentAttribute.binding = 0;
    tangentAttribute.location = 2;
    tangentAttribute.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    tangentAttribute.offset = offsetof(gpu::Vertex, tangent);

    VkVertexInputAttributeDescription& colorAttribute = attributes[3];
    colorAttribute.binding = 0;
    colorAttribute.location = 3;
    colorAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    colorAttribute.offset = offsetof(gpu::Vertex, color);

    return attributes;
}
