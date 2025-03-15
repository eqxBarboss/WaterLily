#ifndef COMMON_H
#define COMMON_H

#ifdef __cplusplus
#pragma once

DISABLE_WARNINGS_BEGIN
#include <glm/glm.hpp>
DISABLE_WARNINGS_END

#include "Shaders/Config.h"

namespace gpu 
{ 
    using namespace glm;
    using uint = uint32_t;

#else
#extension GL_EXT_shader_8bit_storage: require
#extension GL_EXT_shader_16bit_storage: require
#endif

struct PushConstants 
{
    mat4 view;
    mat4 projection;
    vec3 viewPos;
};

// TODO: Use positions only for shadows pass: measure impact and try to separate, do the packing, now 64 bytes / vertex
struct Vertex
{
    vec4 posAndU;
    vec4 normalAndV;
    vec4 tangent;
    vec4 color;
};

// Meshlet data buffer contains 2 important geometry elements for each meshlet stored one after another:
// 1. uint16_t / uint32_t offsets (check bShortVertexOffsets flag) to meshlet vertices in global vertex buffer.
// These offsets are calculated relative to meshlet baseVertexIndex, so meshlet vertices can be obtained as
// vertexBuffer[baseVertexIndex + vertexOffsets[0]]...vertexBuffer[baseVertexIndex + vertexOffsets[vertexCount - 1]]
// 2. Triangle indices which are used as gl_PrimitiveTriangleIndicesEXT output to produce triangles for meshlets.
// Triangle indices are stored as uint8_t elements.
struct Meshlet
{
    uint dataOffset;
    uint baseVertexIndex;
    uint8_t vertexCount;
    uint8_t triangleCount;
    uint8_t bShortVertexOffsets;
    uint8_t padding1;
    uint padding2;
};

struct Primitive
{
    uint transformIndex;
    uint materialIndex;
};

struct Draw
{
    uint firstMeshletIndex;
    uint meshletCount;
    uint primitiveIndex;
    uint padding1;
};

struct TaskPayload
{
    uint meshletIndex;
    uint primitiveIndex;
};

#ifdef __cplusplus
}
#endif

#endif
