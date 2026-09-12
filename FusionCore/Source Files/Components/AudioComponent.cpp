#include "../../Header Files/Components/AudioComponent.h"

AudioComponent::AudioComponent(Object* parent) : ComponentBase<AudioComponent>(parent) {
	Name = "Audio Component";
}

void AudioComponent::Activate() {
	Component::Activate();
	if (audioPath != "") {
		soundHandle = AudioManager::getInstance().LoadSound(audioPath, streaming, loop);
	}
}

void AudioComponent::Deactivate() {
	Component::Deactivate();
	AudioManager::getInstance().UnloadSound(soundHandle);
}

void AudioComponent::OnDelete() {
	Deactivate();
}

void AudioComponent::CopyTo(Object* other) {
	AudioComponent* target = other->GetComponent<AudioComponent>();
	if (!target) {
		other->AddComponent(std::make_unique<AudioComponent>(other));
		target = other->GetComponent<AudioComponent>();
	}

	target->SetAudioPath(audioPath);
}

void AudioComponent::Serialize(BinaryWriter& w) {
	Component::Serialize(w);
	w.WriteString(audioPath);
	w.Write(volume);
	w.Write(streaming);
	w.Write(loop);
}

void AudioComponent::Deserialize(BinaryReader& r) {
	Component::Deserialize(r);
	audioPath = r.ReadString();
	volume = r.Read<float>();
	streaming = r.Read<bool>();
	loop = r.Read<bool>();
}

void AudioComponent::ProcessInspectorUI() {
	ImGui::Text("Audio File");
	ImGui::SameLine();
	char selected_audio_path[128] = "None (click to choose...)";
	if (!audioPath.empty()) {
		std::string displayPath = FileManager::getInstance().AbsoluteToVirtual(audioPath.c_str());
#if defined(_MSC_VER)
		strcpy_s(selected_audio_path, displayPath.c_str());
#else
		strncpy(selected_audio_path, displayPath.c_str(), sizeof(selected_audio_path) - 1);
#endif
	}
	ImGui::InputText("##Audio path", selected_audio_path, IM_ARRAYSIZE(selected_audio_path), ImGuiInputTextFlags_ReadOnly);
	if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	if (ImGui::IsItemClicked()) {
		FileDialogOptions opts;
		opts.title = "Choose Audio";
		opts.filters = {
			{ "Audio Files", "*.wav;*.mp3" },
			{ "All Files", "*.*" }
		};
		if (auto path = FileDialog::ShowOpenDialog(opts)) {
			EditorManager::getInstance().BeginEdit({ parent });
			SetAudioPath(*path);
			EditorManager::getInstance().EndEdit({ parent });
		}
	}

	if (ImGui::BeginDragDropTarget()) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(FileManager::kResourceDragDropPayloadType)) {
			std::string virtualPath(static_cast<const char*>(payload->Data));
			FileManager& fm = FileManager::getInstance();
			if (!fm.IsDirectory(virtualPath)) {
				EditorManager::getInstance().BeginEdit({ parent });
				SetAudioPath(fm.VirtualToAbsolute(virtualPath).string());
				EditorManager::getInstance().EndEdit({ parent });
			}
		}
		ImGui::EndDragDropTarget();
	}
	
	EditorField::CheckboxScene(parent, "Streaming", "##Streaming", &streaming, [&] {
		if (audioPath != "") {
			SetStreaming(streaming);
		}
	});
	
	EditorField::CheckboxScene(parent, "Loop", "##Loop", &loop, [&] {
		if (audioPath != "") {
			SetLooping(loop);
		}
	});

	EditorField::CheckboxScene(parent, "Playing", "##Playing", &isPlaying, [&] {
		if (isPlaying) {
			PlayAudio();
		}
		else {
			StopAudio();
		}
		});

	EditorField::InputFloatScene(parent, "Volume", "##Volume", &volume, [&] {
		SetVolume(volume);
	});
}

void AudioComponent::SetAudioPath(const std::string& path) {
	audioPath = path;
	if (isActive && !EngineManager::getInstance().isHeadless) {
		AudioManager::getInstance().UnloadSound(soundHandle);
		soundHandle = AudioManager::getInstance().LoadSound(audioPath, streaming, loop);
	}
}

void AudioComponent::SetStreaming(bool streaming) {
	this->streaming = streaming;
	if (isActive && audioPath != "" && !EngineManager::getInstance().isHeadless) {
		AudioManager::getInstance().UnloadSound(soundHandle);
		soundHandle = AudioManager::getInstance().LoadSound(audioPath, streaming, loop);
	}
}

void AudioComponent::SetLooping(bool loop) {
	this->loop = loop;
	if (isActive && audioPath != "") {
		AudioManager::getInstance().UnloadSound(soundHandle);
		soundHandle = AudioManager::getInstance().LoadSound(audioPath, streaming, loop);
	}
}

void AudioComponent::PlayAudio() {
	if (soundHandle.loaded && !EngineManager::getInstance().isHeadless) {
		ma_result result = ma_sound_start(soundHandle.sound.get());
		if (result != MA_SUCCESS) {
			Console::PrintError("AudioComponent: Unable to play sound '{}', error {}").Format(audioPath, std::to_string(result));
		}
		else {
			isPlaying = true;
		}
	}
}

void AudioComponent::StopAudio() {
	if (soundHandle.loaded && !EngineManager::getInstance().isHeadless) {
		ma_result result = ma_sound_stop(soundHandle.sound.get());
		if (result != MA_SUCCESS) {
			Console::PrintError("AudioComponent: Unable to stop sound '{}', error {}").Format(audioPath, std::to_string(result));
		}
		else {
			isPlaying = false;
		}
	}
}

void AudioComponent::SetVolume(float volume) {
	if (soundHandle.loaded && !EngineManager::getInstance().isHeadless) {
		ma_sound_set_volume(soundHandle.sound.get(), volume);
		this->volume = volume;
	}
}