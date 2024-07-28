#pragma once

#include <glm/vec3.hpp>
#include <glm/gtc/epsilon.hpp>

using Vector3 = glm::vec3;

constexpr const Vector3 VECTOR3_UP = {0, 1, 0};

inline bool fuzzy_eq(const Vector3& a, const Vector3& b, float epsilon) {
	auto c = glm::epsilonEqual(a, b, Vector3(epsilon, epsilon, epsilon));
	return c[0] || c[1] || c[2];
}

