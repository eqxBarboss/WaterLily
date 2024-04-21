#pragma once

#include <glm/glm.hpp>

namespace Vector2
{
    constexpr auto zero = glm::vec2(0.0f, 0.0f);
    constexpr auto unitX = glm::vec2(1.0f, 0.0f);
    constexpr auto unitY = glm::vec2(0.0f, 1.0f);
}

namespace Vector3
{
    constexpr auto zero = glm::vec3(0.0f, 0.0f, 0.0f);
    constexpr auto unitX = glm::vec3(1.0f, 0.0f, 0.0f);
    constexpr auto unitY = glm::vec3(0.0f, 1.0f, 0.0f);
    constexpr auto unitZ = glm::vec3(0.0f, 0.0f, 1.0f);
}

namespace Helpers
{
	std::vector<char> ReadFile(const std::string& filename);
}
