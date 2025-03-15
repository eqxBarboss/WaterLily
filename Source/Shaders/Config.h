#ifndef CONFIG_H
#define CONFIG_H

#define TASK_WG_SIZE 64
#define MESH_WG_SIZE 64

#define MAX_MESHLET_VERTICES 64
#define MAX_MESHLET_TRIANGLES 96

#ifdef __cplusplus
#pragma once

namespace gpu 
{
    constexpr uint32_t taskWgSize = TASK_WG_SIZE;
    constexpr uint32_t meshWgSize = MESH_WG_SIZE;

    constexpr uint32_t maxMeshletVertices = MAX_MESHLET_VERTICES;
    constexpr uint32_t maxMeshletTriangles = MAX_MESHLET_TRIANGLES;
}
#endif

#endif