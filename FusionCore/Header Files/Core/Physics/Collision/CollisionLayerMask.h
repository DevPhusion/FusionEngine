#pragma once
#include <cstdint>

enum class CollisionLayer : uint16_t {
	LAYER_1 = (1 << 0),
	LAYER_2 = (1 << 1),
	LAYER_3 = (1 << 2),
	LAYER_4 = (1 << 3),
	LAYER_5 = (1 << 4),
	LAYER_6 = (1 << 5),
	LAYER_7 = (1 << 6),
	LAYER_8 = (1 << 7),
	LAYER_9 = (1 << 8),
	LAYER_10 = (1 << 9),
	LAYER_11 = (1 << 10),
	LAYER_12 = (1 << 11),
	LAYER_13 = (1 << 12),
	LAYER_14 = (1 << 13),
	LAYER_15 = (1 << 14),
	LAYER_16 = (1 << 15)
};

enum class CollisionMask : uint16_t {
	LAYER_1 = (1 << 0),
	LAYER_2 = (1 << 1),
	LAYER_3 = (1 << 2),
	LAYER_4 = (1 << 3),
	LAYER_5 = (1 << 4),
	LAYER_6 = (1 << 5),
	LAYER_7 = (1 << 6),
	LAYER_8 = (1 << 7),
	LAYER_9 = (1 << 8),
	LAYER_10 = (1 << 9),
	LAYER_11 = (1 << 10),
	LAYER_12 = (1 << 11),
	LAYER_13 = (1 << 12),
	LAYER_14 = (1 << 13),
	LAYER_15 = (1 << 14),
	LAYER_16 = (1 << 15)
};

constexpr CollisionLayer operator|(CollisionLayer a, CollisionLayer b) {
	return static_cast<CollisionLayer>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}
constexpr CollisionLayer operator&(CollisionLayer a, CollisionLayer b) {
	return static_cast<CollisionLayer>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}
constexpr CollisionLayer operator^(CollisionLayer a, CollisionLayer b) {
	return static_cast<CollisionLayer>(static_cast<uint16_t>(a) ^ static_cast<uint16_t>(b));
}
constexpr CollisionLayer operator~(CollisionLayer a) {
	return static_cast<CollisionLayer>(~static_cast<uint16_t>(a));
}

constexpr CollisionMask operator|(CollisionMask a, CollisionMask b) {
	return static_cast<CollisionMask>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}
constexpr CollisionMask operator&(CollisionMask a, CollisionMask b) {
	return static_cast<CollisionMask>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}
constexpr CollisionMask operator^(CollisionMask a, CollisionMask b) {
	return static_cast<CollisionMask>(static_cast<uint16_t>(a) ^ static_cast<uint16_t>(b));
}
constexpr CollisionMask operator~(CollisionMask a) {
	return static_cast<CollisionMask>(~static_cast<uint16_t>(a));
}

inline bool layerOverlap(uint16_t objALayer, uint16_t objAMask, uint16_t objBLayer, uint16_t objBMask) {
	if ((objAMask & objBLayer) || (objBMask & objALayer)) {
		return true;
	}

	return false;
}