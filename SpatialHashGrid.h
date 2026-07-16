#pragma once
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include <cmath>
class SpatialHashGrid
{
public:
	SpatialHashGrid(float cellSize = 1.0f);
	SpatialHashGrid() = default;

	float cellSize;

	void Build(const std::vector<glm::vec3>& positions);
	void Query(const glm::vec3& position, float radius, std::vector<int>& outIndices);
	void QueryNeighbourCells(const glm::vec3& position, std::vector<int>& outIndices);
	void Clear();
private:
	using CellCoord = std::pair<int32_t, int32_t>;

	static int64_t HashKey(int32_t cx, int32_t cy);
	CellCoord ToCellCoord(const glm::vec3& position);
	std::unordered_map<int64_t, std::vector<int>> cells;
	const std::vector<glm::vec3>* boundPositions = nullptr;
};

