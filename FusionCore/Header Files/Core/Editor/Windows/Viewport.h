#pragma once
#include "../EditorWindow.h"

class Viewport : public EditorWindow
{
public:
	Viewport(std::string name);
	Viewport() = default;
	~Viewport();

	ImVec2 panelPos = ImVec2(0, 0);   
	ImVec2 panelSize = ImVec2(0, 0);

	unsigned int colorTexture = 0;
	int textureWidth = 0;
	int textureHeight = 0;

	virtual void ProcessWindow();

	void Resize(int width, int height);

	void BeginRenderGame();  
	void EndRenderGame();    

	bool IsHovered() const { return isHovered; }
	bool IsFocused() const { return isFocused; }

private:
	unsigned int fbo = 0;
	unsigned int depthStencilRBO = 0;

	bool isHovered = false;
	bool isFocused = false;

	void CreateFramebuffer(int width, int height);
	void DestroyFramebuffer();
};