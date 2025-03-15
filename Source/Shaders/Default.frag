#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec4 inTangent;
layout(location = 2) in vec2 inUv;
layout(location = 3) in vec4 inColor;

layout(set = 0, binding = 6) uniform texture2D texImage;
layout(set = 0, binding = 7) uniform sampler texSampler;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 normal = normalize(inNormal);
    vec3 lightDir = normalize(vec3(0.5, 0.5, 1.0));

    float intensity = max(dot(normal, lightDir), 0.0);

    vec4 baseColor = max(vec4(0.5, 0.5, 0.5, 1.0), inColor);
    //baseColor = mix(texture(sampler2D(texImage, texSampler), inUv), baseColor, 0.6);

    vec3 diffuse = baseColor.rgb * intensity;
    vec3 ambient = baseColor.rgb * 0.2;

    outColor = vec4(diffuse + ambient, baseColor.a);
}