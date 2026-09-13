#pragma once
#include <miniaudio/miniaudio.h>
#include "../Core/Editor/Windows/Console.h"

class AudioComponent;

struct AudioHandle {
	std::unique_ptr<ma_sound> sound;
	bool loaded = false;

	AudioHandle() = default;
	AudioHandle(const AudioHandle&) = delete;
	AudioHandle& operator=(const AudioHandle&) = delete;
	AudioHandle(AudioHandle&&) = default;
	AudioHandle& operator=(AudioHandle&&) = default;
};

class AudioManager
{
public:
	AudioManager(const AudioManager&) = delete;
	void operator=(const AudioManager&) = delete;

	static AudioManager& getInstance() {
		static AudioManager instance;
		return instance;
	}

	void Initialize();
	void Shutdown();

	AudioHandle LoadSound(const std::string& path, bool stream = false, bool loop = false);
	void UnloadSound(AudioHandle& handle);

	ma_engine audioEngine;
	AudioComponent* activeListener;

private:
	AudioManager() = default;
};

