#pragma once

#include <cstdint>

enum class InstanceClass : uint32_t {
	INSTANCE,

	BASE_PART,
	PART,
	WEDGE_PART,

	NUM_TYPES
};

