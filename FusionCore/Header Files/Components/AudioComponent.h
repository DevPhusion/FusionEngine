#pragma once
#include "Component.h"
#include "../../Header Files/Core/AudioManager.h"
#include "../../Header Files/Objects/Object.h"
#include "../../Header Files/Core/Editor/EditorField.h"
 
class AudioComponent : public ComponentBase<AudioComponent>
{
public:
	AudioComponent(Object* parent);
	AudioComponent() = default;

	virtual void Activate();
	virtual void Deactivate();
	virtual void OnDelete();
	virtual void ProcessInspectorUI();
	virtual void CopyTo(Object* other);
	virtual void Serialize(BinaryWriter& w);
	virtual void Deserialize(BinaryReader& r);

	SoundHandle soundHandle;
	std::string audioPath = "";
	float volume = 1.0f;
	bool streaming = false;
	bool loop = false;
	bool isPlaying = false;

	void SetAudioPath(const std::string& path);
	void SetStreaming(bool streaming);
	void SetLooping(bool loop);
	void PlayAudio();
	void StopAudio();
	void SetVolume(float volume);

};

