#pragma once

#include "Shaders/Common.h"
#include "Utils/Constants.hpp"

#include <volk.h>

struct Primitive
{
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    uint32_t vertexOffset = 0;

    uint32_t firstMeshletIndex = 0;
    uint32_t meshletCount = 0;

    uint32_t materialIndex = 0; // TODO: Implement real materials
};

// TODO: Another scene elements structs - I don't like these ones
struct Mesh
{
    uint32_t firstPrimitiveIndex = 0;
    uint32_t primitiveCount = 0;

    uint32_t transformIndex = 0;
};

struct RawScene
{
    std::vector<gpu::Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<glm::mat4> transforms;

    std::vector<uint32_t> meshletData;
    std::vector<gpu::Meshlet> meshlets;

    std::vector<Primitive> primitives;
    std::vector<Mesh> meshes;

    std::vector<gpu::Primitive> gpuPrimitives;
    std::vector<gpu::Draw> draws;

    std::vector<VkDrawIndexedIndirectCommand> indirectCommands;
};