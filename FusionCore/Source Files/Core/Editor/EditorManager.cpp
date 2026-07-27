#include "../../../Header Files/Core/Editor/EditorManager.h"
#include "../../../Header Files/Core/Rendering/Renderer.h"
#include "../../../imgui/imgui_internal.h"

void EditorManager::Setup(GLFWwindow* window) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, false);
	ImGui_ImplOpenGL3_Init("#version 330");

	AddWindow(new Inspector("Inspector"));
	AddWindow(new EngineStatus("Status"));
	AddWindow(new Hierarchy("Hierarchy"));
	AddWindow(new Console("Console"));
	AddWindow(new EngineProfiler("Profiler"));
	AddWindow(new FileSystem("File System"));
}

void EditorManager::AddWindow(EditorWindow* window) {
	Windows.push_back(window);
}

void EditorManager::SetSelectedObject(Object* object) {
	this->selectedObject = object;
}

void EditorManager::ProcessDockSpace() {
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGuiWindowFlags hostFlags =
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBackground;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("##DockSpaceHost", nullptr, hostFlags);
	ImGui::PopStyleVar(3);

	ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
	ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

	static bool builtLayout = false;
	static int focusFrameCountdown = -1; 

	if (!builtLayout) {
		builtLayout = true;

		ImGui::DockBuilderRemoveNode(dockspaceId);
		ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

		ImGuiID dockMain = dockspaceId;
		ImGuiID dockTop = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Up, 0.04f, nullptr, &dockMain);
		ImGuiID dockLeft = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.18f, nullptr, &dockMain);
		ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.25f, nullptr, &dockMain);
		ImGuiID dockBot = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.3, nullptr, &dockMain);
		ImGuiID dockLeftBottom = ImGui::DockBuilderSplitNode(dockLeft, ImGuiDir_Down, 0.5f, nullptr, &dockLeft);

		ImGui::DockBuilderDockWindow("Status", dockTop);
		ImGui::DockBuilderDockWindow("Hierarchy", dockLeft);
		ImGui::DockBuilderDockWindow("Inspector", dockRight);
		ImGui::DockBuilderDockWindow("Console", dockBot);
		ImGui::DockBuilderDockWindow("Profiler", dockBot);
		ImGui::DockBuilderDockWindow("File System", dockLeftBottom);

		ImGui::DockBuilderFinish(dockspaceId);


		focusFrameCountdown = 2;
	}

	if (focusFrameCountdown > 0) {
		focusFrameCountdown--;
		if (focusFrameCountdown == 0) {
			ImGui::SetWindowFocus("Console");
		}
	}

	ImGui::End();
}

void EditorManager::ProcessEditor() {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO();
	WindowHovered = io.WantCaptureMouse;
	WindowTyped = io.WantCaptureKeyboard;

	ProcessDockSpace();

	for (int i = 0; i < Windows.size(); i++)
	{
		Windows[i]->ProcessWindow();
	}

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}