#pragma once
#include <miniaudio/miniaudio.h>
#include "../Core/Editor/Windows/Console.h"

struct SoundHandle {
	std::unique_ptr<ma_sound> sound;
	bool loaded = false;

	SoundHandle() = default;
	SoundHandle(const SoundHandle&) = delete;
	SoundHandle& operator=(const SoundHandle&) = delete;
	SoundHandle(SoundHandle&&) = default;
	SoundHandle& operator=(SoundHandle&&) = default;
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

	SoundHandle LoadSound(const std::string& path, bool stream = false, bool loop = false);
	void UnloadSound(SoundHandle& handle);

	ma_engine audioEngine;

private:
	AudioManager() = default;
};

