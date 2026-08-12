#pragma once
#include "../Objects/Object.h"
#include "../Core/Physics/Constraint/PGSConstraint/Constraint.h"
#include "Editor/EditorManager.h"

struct OpenScene {
	std::string filePath;                         
	std::string displayName = "New Scene.fscene";
	bool isDirty = false;

	std::vector<std::unique_ptr<Object>> objects;
	std::vector<Constraint*> constraints;           
	Object* selectedObject = nullptr;
};

class SceneManager
{
public:
	static SceneManager& getInstance() {
		static SceneManager instance;
		return instance;
	}

	SceneManager(const SceneManager&) = delete;
	void operator=(const SceneManager&) = delete;

	static constexpr uint32_t sceneMagicByte = 0x4A52504A;
	static constexpr uint32_t sceneVersion = 1;

	int OpenSceneTab(const std::string& path);     
	int NewSceneTab();     
	int ConsumeFocusRequest();
	void CloseSceneTab(int index, bool discardUnsaved = false); 
	void SwitchToScene(int index);

	int GetActiveIndex() const { return activeIndex; }
	int GetSceneCount() const { return (int)openScenes.size(); }
	OpenScene& GetScene(int index) { return openScenes[index]; }
	int FindSceneByPath(const std::string& path) const;

	void SaveScene(const std::string& path);        
	bool SaveActiveScene();                           
	void LoadSceneFromFile(const std::string& path);  
	void NewScene();          

	void ActivateLiveScene();
	void DeactivateLiveScene();

	bool IsActiveSceneDirty() const { return activeIndex >= 0 && openScenes[activeIndex].isDirty; }
	void MarkActiveDirty();
	bool AnySceneDirty() const;
	const std::string& GetCurrentSceneFile() const;

private:
	SceneManager();
	~SceneManager() = default;

	void ClearLiveScene();        
	void SwapActiveWithLive();   
	void SwapLiveWithScene(int index); 

	std::vector<OpenScene> openScenes;
	int activeIndex = -1;
	int focusRequestIndex = -1;
};