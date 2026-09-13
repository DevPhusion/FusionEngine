#pragma once
#include "Component.h"
#include "../Objects/Object.h"
#include "../Core/Editor/EditorField.h"
#include "../Core/AudioManager.h"

struct AudioEntry {
	AudioEntry() = default;
	AudioEntry(const AudioEntry&) = delete;
	AudioEntry& operator=(const AudioEntry&) = delete;
	AudioEntry(AudioEntry&&) = default;
	AudioEntry& operator=(AudioEntry&&) = default;

	AudioHandle handle;
	std::string name = "";
	std::string audioPath = "";
	bool streaming = false;
	bool loop = false;
	bool isPlaying = false;
	bool previouslyPlaying = false; // used for physics mode changed event only
	float volume = 1.0f;
};

class AudioComponent : public ComponentBase<AudioComponent>
{
public:
	AudioComponent(Object* parent);
	AudioComponent() = default;
	AudioComponent(const AudioComponent&) = delete;
	AudioComponent& operator=(const AudioComponent&) = delete;
	AudioComponent(AudioComponent&&) = default;
	AudioComponent& operator=(AudioComponent&&) = default;

	virtual void Activate();
	virtual void Deactivate();
	virtual void OnDelete();
	virtual void ProcessInspectorUI();
	virtual void CopyTo(Object* other);
	virtual void Serialize(BinaryWriter& w);
	virtual void Deserialize(BinaryReader& r);

	std::vector<AudioEntry> audioEntries;
	bool listener = false;

	AudioEntry* FindAudioEntry(const std::string& name);
	void SetAudioPath(std::string name, const std::string& path);
	void SetStreaming(std::string name, bool streaming);
	void SetLooping(std::string name, bool loop);
	void AddAudioTrack(std::string name, const std::string& audioPath = "", 
		bool streaming = false, bool loop = false, float volume = 1.0f);
	void RemoveAudioTrack(std::string name);
	void PauseAudioTrack(std::string name);
	void PlayAudioTrack(std::string name);
	void StopAudioTrack(std::string name);
	void SetVolume(std::string name, float volume);
private:
	int physicsModeChangedCallbackId = -1;
	std::string GenerateUniqueTrackName(const std::string& baseName, const std::string* exclude = nullptr);
	void OnPhysicsModeChanged();
};

