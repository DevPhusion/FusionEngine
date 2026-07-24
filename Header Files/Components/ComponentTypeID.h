#pragma once
#include <cstddef>

class ComponentTypeID
{
public:
	template <typename T>
	static size_t Get() {
		static const size_t id = Next();
		return id;
	}
private:
	static size_t Next() {
		static size_t counter = 0;
		return counter++;
	}
};