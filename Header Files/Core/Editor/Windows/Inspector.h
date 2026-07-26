#pragma once
#include "../EditorWindow.h"
#include "../../../Components/RenderComponent.h"
#include "../../../Core/Scripting/ScriptManager.h"
#include <filesystem>
class Inspector : public EditorWindow
{
public:
	Inspector(std::string name);

	char m_SearchBuffer[128] = {};
	virtual void ProcessWindow();
};

