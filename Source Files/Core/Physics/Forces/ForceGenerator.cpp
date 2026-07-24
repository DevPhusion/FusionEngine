#include "../../../../Header Files/Core/Physics/Forces/ForceGenerator.h"

void ForceGenerator::setDisplayFunc(std::shared_ptr<std::function<void(int index)>> func) {
	displayFunc = func;
}