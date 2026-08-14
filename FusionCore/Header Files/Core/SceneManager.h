#pragma once
#include "../Objects/Object.h"
#include "../Core/Physics/Constraint/PGSConstraint/Constraint.h"
#include "Editor/EditorManager.h"

struct OpenScene {
	int uid = 0;
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

	std::string ToComparablePath(const std::string& path) const;
	std::string NormalizeToVirtualPath(const std::string& path) const;

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

	Object* AddScene(const std::string& path, Object* parent = nullptr);
	void RemoveScene(Object* root);

	void RequestLoadScene(const std::string& path);
	void ProcessPendingSceneLoad();

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

	bool ParseSceneObjects(const std::string& path, std::vector<std::unique_ptr<Object>>& outObjects, uint32_t& outObjectCount);

	bool ParseTopLevelSceneObjects(const std::string& path, std::vector<std::unique_ptr<Object>>& outObjects,
		std::vector<Constraint*>& outConstraints, uint32_t& outObjectCount);

	bool ExpandSceneRoot(Object* root, std::vector<std::unique_ptr<Object>>& ownerObjects,
		std::vector<Constraint*>& ownerConstraints, bool activateNow, std::vector<std::string>& expansionStack);

	bool IsInsideSceneInstance(Object* obj) const;
	void RefreshSceneInstances(std::vector<std::unique_ptr<Object>>& objects, std::vector<Constraint*>& constraints);
	void RefreshSceneRootInstance(Object* root, std::vector<std::unique_ptr<Object>>& objects, std::vector<Constraint*>& constraints);

	std::string pendingSceneLoadPath;
	bool hasPendingSceneLoad = false;

	std::vector<OpenScene> openScenes;
	int activeIndex = -1;
	int focusRequestIndex = -1;
	int nextSceneUid = 1;

	int AllocateSceneUid() { return nextSceneUid++; }
};