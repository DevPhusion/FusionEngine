#include "../../../Header Files/Core/Scripting/PyBindings.h"
#include "../../../Header Files/Core/Scripting/PackageManager.h"
#include <cstdlib>

PYBIND11_MODULE(fusion, m) {
	if (const char* projectDir = std::getenv("FUSION_PROJECT_DIR")) {
		PackageManager::getInstance().LoadForProject(projectDir);
	}
	RegisterEngineBindings(m);
}