#pragma once

enum class BroadPhaseMode {
	AABB,
	BoundingCircle
};

struct BroadPhaseHandle {
	void* node = nullptr;
	bool isBox = false;

	bool IsValid() const { return node != nullptr; }
	void Reset() { node = nullptr; isBox = false; }
};
