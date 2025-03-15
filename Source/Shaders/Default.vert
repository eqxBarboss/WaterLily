#version 450

#extension GL_GOOGLE_include_directive : require

#include "Common.h"

layout(location = 0) in vec4 inPosAndU;
layout(location = 1) in vec4 inNormalAndV;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec4 inColor;

layout(push_constant) uniform Globals
{
    PushConstants globals;
};

layout(set = 0, binding = 1, std430) readonly buffer Transforms 
{
    mat4 transforms[]; 
};

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec4 outTangent;
layout(location = 2) out vec2 outUv;
layout(location = 3) out vec4 outColor;

void main() 
{
    vec3 position = inPosAndU.xyz;
    vec3 normal = inNormalAndV.xyz;
    vec4 tangent = inTangent;
    vec2 uv = vec2(inPosAndU.w, inNormalAndV.w);
    vec4 color = inColor;

    mat4 transform = transforms[gl_InstanceIndex];
    
    vec4 pos = globals.projection * globals.view * transform * vec4(position, 1.0);

    normal = mat3(transform) * normal;

    gl_Position = pos;
    outNormal = normal;
    outTangent = tangent;
    outUv = uv;
    outColor = color;
}
