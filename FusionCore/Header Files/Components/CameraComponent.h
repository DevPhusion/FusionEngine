#pragma once
#include "Component.h"
#include "../Objects/Object.h"
#include "../Core/Camera.h"
class CameraComponent : public ComponentBase<CameraComponent>
{
public:
	CameraComponent(Object* parent);
	CameraComponent() = default;

	virtual void Activate();
	virtual void Deactivate();
	virtual void SetEnabled(bool enabled);
	virtual void OnDelete();
	virtual void ProcessInspectorUI();
	virtual void CopyTo(Object* other);
	virtual void Serialize(BinaryWriter& w);
	virtual void Deserialize(BinaryReader& r);

	void DrawDebug();
	void SetRange(float range);
	float GetRange() const { return range; }
private:
	float range = 5.0f;
	bool isMain = true;

	int transformCallbackID = -1;
	int physicsModeChangedID = -1;
	bool isDrivingCamera = false;

	void SyncCamera();
	void OnPhysicsModeChanged();
	void RegisterCallbacks();
	void UnregisterCallbacks();
};

