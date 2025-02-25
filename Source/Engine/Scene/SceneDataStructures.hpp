#pragma once

#include "Utils/Constants.hpp"

#include <volk.h>

DISABLE_WARNINGS_BEGIN
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
DISABLE_WARNINGS_END

struct Vertex
{
    static std::vector<VkVertexInputBindingDescription> GetBindings();
    static std::vector<VkVertexInputAttributeDescription> GetAttributes();

    bool operator==(const Vertex& other) const;

    glm::vec3 pos = Vector3::zero;
    glm::vec3 normal = Vector3::zero;
    glm::vec2 uv = Vector2::zero;
    glm::vec3 color = Vector3::zero;
    glm::vec4 tangent = Vector4::zero;
};

struct Primitive
{
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    uint32_t nodeId = 0; // Id of the parent node in the scene nodes array
};

struct Mesh
{
    std::vector<Primitive> primitives;
};

struct SceneNode
{
    SceneNode* parent = nullptr;
    std::vector<std::unique_ptr<SceneNode>> children;

    Mesh mesh;
    glm::mat4 transform = Matrix4::identity;

    bool visible = true;
    std::string name;
};

struct PushConstants
{
    uint32_t tbd;
};

namespace std
{
    template<>
    struct hash<Vertex>
    {
        size_t operator()(Vertex const& vertex) const
        {
            size_t seed = 0; // Golden Ratio Hashing (Fibonacci Hashing)
            seed ^= std::hash<glm::vec3>()(vertex.pos) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= std::hash<glm::vec3>()(vertex.normal) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= std::hash<glm::vec2>()(vertex.uv) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= std::hash<glm::vec3>()(vertex.color) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= std::hash<glm::vec4>()(vertex.tangent) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };
}