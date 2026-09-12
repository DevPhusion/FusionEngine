#include "../../Header Files/Core/AudioManager.h"

void AudioManager::Initialize() {
	ma_result result = ma_engine_init(NULL, &audioEngine);
	if (result != MA_SUCCESS) {
		Console::PrintError("AudioManager: Unable to initialize audio engine, error {}").Format(
			std::to_string(result));
	}
}

void AudioManager::Shutdown() {
	ma_engine_uninit(&audioEngine);
}

SoundHandle AudioManager::LoadSound(const std::string& path, bool stream, bool loop) {
	SoundHandle handle;
	handle.sound = std::make_unique<ma_sound>();

	ma_uint32 flags = stream ? MA_SOUND_FLAG_STREAM : 0;
	ma_result result = ma_sound_init_from_file(
		&audioEngine, path.c_str(), flags, NULL, NULL, handle.sound.get()
	);
	if (result != MA_SUCCESS) {
		Console::PrintError("AudioManager: Unable to load sound '{}', error {}").Format(path, std::to_string(result));
		handle.sound.reset();
		return handle;
	}
	ma_sound_set_looping(handle.sound.get(), loop ? MA_TRUE : MA_FALSE);
	handle.loaded = true;

	return handle;
}

void AudioManager::UnloadSound(SoundHandle& handle) {
	if (handle.loaded && handle.sound) {
		ma_sound_uninit(handle.sound.get());
		handle.sound.reset();
		handle.loaded = false;
	}
}