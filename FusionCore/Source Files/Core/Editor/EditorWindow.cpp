#include "../../../Header Files/Core/Editor/EditorWindow.h"

EditorWindow::EditorWindow(std::string name) {
	this->name = name;
}

void EditorWindow::Show() {
	this->hidden = false;
}

void EditorWindow::Hide() {
	this->hidden = true;
}