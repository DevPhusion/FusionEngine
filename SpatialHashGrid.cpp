#include "SpatialHashGrid.h"

SpatialHashGrid::SpatialHashGrid(float cellSize) {
	this->cellSize = cellSize;
}

void SpatialHashGrid::Build(const std::vector<glm::vec3>& positions) {
	cells.clear();
	boundPositions = &positions;
	
	for (int i = 0; i < positions.size(); i++)
	{
		CellCoord cell = ToCellCoord(positions[i]);
		cells[HashKey(cell.first, cell.second)].push_back(i);
	}
}

void SpatialHashGrid::QueryNeighbourCells(const glm::vec3& position, std::vector<int>& outIndices) {
	if (!boundPositions) return;

	CellCoord center = ToCellCoord(position);

	for (int32_t dx = -1; dx <= 1; ++dx) {
		for (int32_t dy = -1; dy <= 1; ++dy) {
			auto it = cells.find(HashKey(center.first + dx, center.second + dy));
			if (it == cells.end()) continue;
			outIndices.insert(outIndices.end(), it->second.begin(), it->second.end());
		}
	}
}

void SpatialHashGrid::Query(const glm::vec3& position, float radius, std::vector<int>& outIndices) {
	if (!boundPositions) return;

	std::vector<int> candidates;
	QueryNeighbourCells(position, candidates);

	float r2 = radius * radius;
	for (int idx : candidates) {
		const glm::vec3& p = (*boundPositions)[idx];
		glm::vec3 diff = p - position;
		float d2 = diff.x * diff.x + diff.y * diff.y; 
		if (d2 <= r2) outIndices.push_back(idx);
	}
}

int64_t SpatialHashGrid::HashKey(int32_t cx, int32_t cy) {
	return (static_cast<int64_t>(cx) << 32) ^ (static_cast<uint32_t>(cy));
}

SpatialHashGrid::CellCoord SpatialHashGrid::ToCellCoord(const glm::vec3& position) {
	int x = std::floor(position.x / cellSize);
	int y = std::floor(position.y / cellSize);
	return { x, y };
}