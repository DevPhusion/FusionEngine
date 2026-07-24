#pragma once
#include "../EditorWindow.h"
#include "../../../Components/RenderComponent.h"
class Inspector : public EditorWindow
{
public:
	Inspector(std::string name);

	char m_SearchBuffer[128] = {};

	virtual void ProcessWindow();
};

