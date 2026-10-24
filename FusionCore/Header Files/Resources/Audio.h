#pragma once
#include "Resource.h"
#include "../Core/AudioManager.h"  
#include <vector>
#include <string>
#include <cstdint>

class Audio : public Resource
{
public:
	using Resource::Resource;

	std::string GetTypeName() const override;
	void Unload() override;

	double GetDuration() const { return duration; }
	int GetSampleRate() const { return sampleRate; }
	int GetChannels() const { return channels; }

protected:
	void OnReload() override;
	void DrawPreview() override;
	void DrawProperties() override;

private:
	bool EnsurePreview();
	bool IsPlaying() const;
	float GetPlaybackPosition() const;
	void TogglePlayback();
	void SeekTo(float seconds);

	bool valid = false;
	int channels = 0;
	int sampleRate = 0;
	uint64_t frameCount = 0;
	double duration = 0.0;
	float peak = 0.0f;
	float rms = 0.0f;

	std::vector<float> waveX, waveMin, waveMax;

	AudioHandle preview;   
};