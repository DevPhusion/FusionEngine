#include "../../Header Files/Resources/Audio.h"
#include "../../Header Files/Core/Editor/EditorField.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace {
	constexpr int kWaveformBuckets = 1024;
	constexpr ma_uint64 kReadChunkFrames = 4096;

	Audio* g_activePreview = nullptr;   

	std::string ChannelLayout(int ch) {
		switch (ch) {
		case 1: return "Mono";
		case 2: return "Stereo";
		case 6: return "5.1";
		case 8: return "7.1";
		default: return std::to_string(ch) + " channels";
		}
	}

	std::string FormatDb(float linear) {
		if (linear <= 1e-6f) return "-inf dBFS";
		char buf[32];
		std::snprintf(buf, sizeof(buf), "%.1f dBFS", 20.0f * std::log10(linear));
		return buf;
	}
}

std::string Audio::GetTypeName() const {
	std::string ext = GetAbsolutePath().extension().string();
	if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::toupper(c); });
	return ext.empty() ? "Audio" : ext + " audio";
}

void Audio::Unload() {
	if (g_activePreview == this) g_activePreview = nullptr;
	AudioManager::getInstance().UnloadSound(preview);
}

void Audio::OnReload() {
	Unload();   

	valid = false;
	channels = sampleRate = 0;
	frameCount = 0;
	duration = 0.0;
	peak = rms = 0.0f;
	waveX.clear(); waveMin.clear(); waveMax.clear();

	if (!Exists()) return;

	ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 0, 0);   // native channels / rate
	ma_decoder decoder;
#ifdef _WIN32
	ma_result result = ma_decoder_init_file_w(GetAbsolutePath().c_str(), &cfg, &decoder);
#else
	ma_result result = ma_decoder_init_file(GetAbsolutePath().c_str(), &cfg, &decoder);
#endif
	if (result != MA_SUCCESS) return;

	ma_format fmt;
	ma_uint32 ch = 0, sr = 0;
	ma_decoder_get_data_format(&decoder, &fmt, &ch, &sr, nullptr, 0);

	ma_uint64 total = 0;
	ma_decoder_get_length_in_pcm_frames(&decoder, &total);

	if (ch == 0 || sr == 0) {
		ma_decoder_uninit(&decoder);
		return;
	}

	channels = (int)ch;
	sampleRate = (int)sr;
	frameCount = total;
	duration = (double)total / (double)sr;
	valid = true;

	if (total > 0) {
		const int buckets = (int)std::min<ma_uint64>(kWaveformBuckets, total);
		waveMin.assign(buckets, FLT_MAX);
		waveMax.assign(buckets, -FLT_MAX);
		waveX.resize(buckets);

		std::vector<float> chunk((size_t)(kReadChunkFrames * ch));
		ma_uint64 frameIdx = 0;
		double sumSq = 0.0;

		while (true) {
			ma_uint64 got = 0;
			ma_result rr = ma_decoder_read_pcm_frames(&decoder, chunk.data(), kReadChunkFrames, &got);
			if (got == 0) break;

			for (ma_uint64 f = 0; f < got; f++) {
				size_t b = (size_t)(((frameIdx + f) * (ma_uint64)buckets) / total);
				if (b >= (size_t)buckets) b = buckets - 1;

				for (ma_uint32 c = 0; c < ch; c++) {
					const float s = chunk[(size_t)(f * ch + c)];
					waveMin[b] = std::min(waveMin[b], s);
					waveMax[b] = std::max(waveMax[b], s);
					peak = std::max(peak, std::fabs(s));
					sumSq += (double)s * s;
				}
			}
			frameIdx += got;
			if (rr != MA_SUCCESS) break;   
		}

		if (frameIdx > 0) rms = (float)std::sqrt(sumSq / ((double)frameIdx * ch));

		for (int i = 0; i < buckets; i++) {
			if (waveMin[i] > waveMax[i]) waveMin[i] = waveMax[i] = 0.0f;  
			waveX[i] = (float)((i + 0.5) * duration / buckets);
		}
	}

	ma_decoder_uninit(&decoder);
}

bool Audio::EnsurePreview() {
	if (preview.loaded) return true;

	preview = AudioManager::getInstance().LoadSound(GetAbsolutePath().string(), true, false);
	if (!preview.loaded) return false;

	ma_sound_set_spatialization_enabled(preview.sound.get(), MA_FALSE); 
	return true;
}

bool Audio::IsPlaying() const {
	return preview.loaded
		&& ma_sound_is_playing(preview.sound.get())
		&& !ma_sound_at_end(preview.sound.get());
}

float Audio::GetPlaybackPosition() const {
	if (!preview.loaded) return 0.0f;
	float s = 0.0f;
	ma_sound_get_cursor_in_seconds(preview.sound.get(), &s);
	return std::clamp(s, 0.0f, (float)duration);
}

void Audio::TogglePlayback() {
	if (IsPlaying()) {
		ma_sound_stop(preview.sound.get());  
		return;
	}
	if (!EnsurePreview()) return;

	if (g_activePreview && g_activePreview != this && g_activePreview->preview.loaded)
		ma_sound_stop(g_activePreview->preview.sound.get());
	g_activePreview = this;

	if (ma_sound_at_end(preview.sound.get()))
		ma_sound_seek_to_pcm_frame(preview.sound.get(), 0);
	ma_sound_start(preview.sound.get());
}

void Audio::SeekTo(float seconds) {
	if (!EnsurePreview()) return;

	ma_uint32 sr = 0;   
	ma_sound_get_data_format(preview.sound.get(), nullptr, nullptr, &sr, nullptr, 0);
	if (sr == 0) return;
	ma_sound_seek_to_pcm_frame(preview.sound.get(), (ma_uint64)((double)seconds * sr));
}

void Audio::DrawPreview() {
	if (!valid) {
		ImGui::TextDisabled("Preview unavailable (unsupported or corrupt audio)");
		return;
	}

	const auto r = EditorField::Detail::AudioPlayer("##audioPlayer", IsPlaying(), GetPlaybackPosition(),
		(float)duration, waveX.data(), waveMin.data(), waveMax.data(), (int)waveX.size(), previewMaxHeight);

	if (r.togglePressed) TogglePlayback();
	if (r.seeked) SeekTo(r.seekTo);
}

void Audio::DrawProperties() {
	if (!valid) return;

	using EditorField::PropertyRow;
	using EditorField::Detail::FormatTime;

	PropertyRow("Duration", FormatTime(duration, true));
	PropertyRow("Sample Rate", std::to_string(sampleRate) + " Hz");
	PropertyRow("Channels", std::to_string(channels) + " (" + ChannelLayout(channels) + ")");
	PropertyRow("Frames", std::to_string(frameCount));

	if (duration > 0.0) {
		char buf[64];
		std::snprintf(buf, sizeof(buf), "%.0f kbps", (double)GetFileSize() * 8.0 / duration / 1000.0);
		PropertyRow("Avg Bitrate", buf);
	}

	if (!waveX.empty()) {
		PropertyRow("Peak", FormatDb(peak));
		PropertyRow("RMS", FormatDb(rms));
	}

	PropertyRow("Uncompressed", FormatBytes((uintmax_t)frameCount * channels * 2) + " (16-bit PCM)");
}