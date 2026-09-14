#include "../../Header Files/Components/AudioComponent.h"
#include "../../Header Files/Core/AudioManager.h"
#include "../../Header Files/Core/ObjectManager.h"

AudioComponent::AudioComponent(Object* parent) : ComponentBase<AudioComponent>(parent) {
	Name = "Audio Component";
}

void AudioComponent::UpdatePosition() {
	TransformComponent* transform = parent->GetComponent<TransformComponent>();
	if (!transform) return;

	glm::vec3 pos = transform->GetWorldPosition();

	for (auto& entry : audioEntries) {
		if (entry.handle.loaded && entry.spatialAudio) {
			ma_sound_set_position(entry.handle.sound.get(), pos.x, pos.y, pos.z);
		}
	}

	if (listener) {
		AudioManager::getInstance().SetListenerPosition(pos);
	}
}

void AudioComponent::OnPhysicsModeChanged() {
	if (EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Pause) {
		for (auto& entry : audioEntries) {
			if (entry.isPlaying) {
				entry.previouslyPlaying = true;
				PauseAudioTrack(entry.name);
			}
		}
	}
	if (EngineManager::getInstance().EnginePrevPhysicsMode == EngineManager::PhysicsMode::Pause
		&& EngineManager::getInstance().EnginePhysicsMode == EngineManager::PhysicsMode::Simulate) {
		for (auto& entry : audioEntries) {
			if (entry.previouslyPlaying) {
				entry.previouslyPlaying = false;
				PlayAudioTrack(entry.name);
			}
		}
	}
}

void AudioComponent::Activate() {
	Component::Activate();
	physicsModeChangedCallbackId = EngineManager::getInstance().AddPhysicsModeChangedEvent([&]() { OnPhysicsModeChanged(); });

	for (auto& entry : audioEntries) {
		if (entry.audioPath != "") {
			entry.handle = AudioManager::getInstance().LoadSound(entry.audioPath, entry.streaming, entry.loop);
			ApplySpatialSettings(&entry);
		}
	}

	if (TransformComponent* transform = parent->GetComponent<TransformComponent>()) {
		transformCallbackId = transform->AddTransformCallback([this]() { UpdatePosition(); });
		UpdatePosition(); 
	}
}

void AudioComponent::Deactivate() {
	Component::Deactivate();
	EngineManager::getInstance().RemovePhysicsModeChangedEvent(physicsModeChangedCallbackId);

	if (transformCallbackId != -1) {
		if (TransformComponent* transform = parent->GetComponent<TransformComponent>()) {
			transform->RemoveTransformCallback(transformCallbackId);
		}
		transformCallbackId = -1;
	}

	if (AudioManager::getInstance().activeListener == this) {
		AudioManager::getInstance().activeListener = nullptr;
	}

	for (auto& entry : audioEntries) {
		AudioManager::getInstance().UnloadSound(entry.handle);
	}
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

	for (const auto& entry : audioEntries) {
		target->AddAudioTrack(entry.name, entry.audioPath, entry.streaming, entry.loop, entry.volume);
	}
}

void AudioComponent::Serialize(BinaryWriter& w) {
	Component::Serialize(w);
	w.Write((int)audioEntries.size());
	for (auto& entry : audioEntries) {
		w.WriteString(entry.name);
		w.WriteString(entry.audioPath);
		w.Write(entry.volume);
		w.Write(entry.streaming);
		w.Write(entry.loop);
		w.Write(entry.spatialAudio);
		w.Write(entry.minDistance);
		w.Write(entry.maxDistance);
	}
	w.Write(listener);
}

void AudioComponent::Deserialize(BinaryReader& r) {
	Component::Deserialize(r);
	int count = r.Read<int>();
	for (int i = 0; i < count; i++) {
		std::string name = r.ReadString();
		std::string path = r.ReadString();
		float volume = r.Read<float>();
		bool streaming = r.Read<bool>();
		bool loop = r.Read<bool>();
		bool spatialAudio = r.Read<bool>();
		float minDist = r.Read<float>();
		float maxDist = r.Read<float>();

		AddAudioTrack(name, path, streaming, loop, volume);
		AudioEntry* entry = FindAudioEntry(name);
		if (entry) {
			entry->spatialAudio = spatialAudio;
			entry->minDistance = minDist;
			entry->maxDistance = maxDist;
			ApplySpatialSettings(entry);
		}
	}
	listener = r.Read<bool>();
	if (listener) {
		AudioManager::getInstance().activeListener = this;
	}
}

void AudioComponent::ProcessInspectorUI() {
	ImGui::Text("Audio Tracks (%d)", (int)audioEntries.size());
	ImGui::SameLine();
	if (ImGui::SmallButton("+ Add Track")) {
		EditorManager::getInstance().BeginEdit({ parent });
		AddAudioTrack("Track");
		EditorManager::getInstance().EndEdit({ parent });
	}

	ImGui::Separator();

	std::string trackToRemove = "";
	for (auto& entry : audioEntries) {
		ImGui::PushID((void*)&entry);

		bool open = ImGui::TreeNodeEx((void*)&entry,
			ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_AllowOverlap,
			"%s", entry.name.c_str());

		ImGui::SameLine();
		if (ImGui::SmallButton("Remove")) {
			trackToRemove = entry.name;
		}

		if (open) {
			ImGui::Indent();

			char nameBuf[64];
#if defined(_MSC_VER)
			strcpy_s(nameBuf, entry.name.c_str());
#else
			strncpy(nameBuf, entry.name.c_str(), sizeof(nameBuf) - 1);
			nameBuf[sizeof(nameBuf) - 1] = '\0';
#endif
			EditorField::InputTextScene(parent, "Name", "##TrackName", nameBuf, IM_ARRAYSIZE(nameBuf), [&] {
				std::string desired = nameBuf;
				if (desired.empty()) desired = "Track";
				entry.name = GenerateUniqueTrackName(desired, &entry.name);
				});

			ImGui::Text("Audio File");
			ImGui::SameLine();
			char selected_audio_path[128] = "None (click to choose...)";
			if (!entry.audioPath.empty()) {
				std::string displayPath = FileManager::getInstance().AbsoluteToVirtual(entry.audioPath.c_str());
#if defined(_MSC_VER)
				strcpy_s(selected_audio_path, displayPath.c_str());
#else
				strncpy(selected_audio_path, displayPath.c_str(), sizeof(selected_audio_path) - 1);
#endif
			}
			ImGui::InputText("##AudioPath", selected_audio_path, IM_ARRAYSIZE(selected_audio_path), ImGuiInputTextFlags_ReadOnly);
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
					SetAudioPath(entry.name, *path);
					EditorManager::getInstance().EndEdit({ parent });
				}
			}

			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(FileManager::kResourceDragDropPayloadType)) {
					std::string virtualPath(static_cast<const char*>(payload->Data));
					FileManager& fm = FileManager::getInstance();
					if (!fm.IsDirectory(virtualPath)) {
						EditorManager::getInstance().BeginEdit({ parent });
						SetAudioPath(entry.name, fm.VirtualToAbsolute(virtualPath).string());
						EditorManager::getInstance().EndEdit({ parent });
					}
				}
				ImGui::EndDragDropTarget();
			}

			EditorField::CheckboxScene(parent, "Streaming", "##Streaming", &entry.streaming, [&] {
				if (entry.audioPath != "") {
					SetStreaming(entry.name, entry.streaming);
				}
				});

			EditorField::CheckboxScene(parent, "Loop", "##Loop", &entry.loop, [&] {
				if (entry.audioPath != "") {
					SetLooping(entry.name, entry.loop);
				}
				});

			EditorField::CheckboxScene(parent, "Playing", "##Playing", &entry.isPlaying, [&] {
				if (entry.isPlaying) {
					PlayAudioTrack(entry.name);
				}
				else {
					StopAudioTrack(entry.name);
				}
				});

			EditorField::InputFloatScene(parent, "Volume", "##Volume", &entry.volume, [&] {
				SetVolume(entry.name, entry.volume);
				});


			ImGui::Separator();

			EditorField::CheckboxScene(parent, "Spatial Audio", "##Is3D", &entry.spatialAudio, [&] {
				SetSpatialAudio(entry.name, entry.spatialAudio);
				});

			if (entry.spatialAudio) {
				EditorField::InputFloatScene(parent, "Min Distance", "##MinDist", &entry.minDistance, [&] {
					SetMinDistance(entry.name, entry.minDistance);
					});
				EditorField::InputFloatScene(parent, "Max Distance", "##MaxDist", &entry.maxDistance, [&] {
					SetMaxDistance(entry.name, entry.maxDistance);
					});
			}

			ImGui::Unindent();
			ImGui::TreePop();
		}

		ImGui::PopID();
		ImGui::Spacing();
	}

	if (!trackToRemove.empty()) {
		EditorManager::getInstance().BeginEdit({ parent });
		RemoveAudioTrack(trackToRemove);
		EditorManager::getInstance().EndEdit({ parent });
	}

	ImGui::Separator();

	EditorField::CheckboxScene(parent, "Listener", "##Listener", &listener, [&] {
		if (listener) {
			AudioManager::getInstance().activeListener = this;
			for (auto& obj : ObjectManager::getInstance().allObjects) {
				if (obj.get() != parent && obj->HasComponent<AudioComponent>()) {
					auto audioComp = obj->GetComponent<AudioComponent>();
					if (audioComp && audioComp->listener) {
						audioComp->listener = false;
					}
				}
			}
		}
		else if (AudioManager::getInstance().activeListener == this) {
			AudioManager::getInstance().activeListener = nullptr;
		}
		});
}

AudioEntry* AudioComponent::FindAudioEntry(const std::string& name) {
	for (auto& e : audioEntries) {
		if (e.name == name) return &e;
	}
	return nullptr;
}

void AudioComponent::ApplySpatialSettings(AudioEntry* entry) {
	if (!entry->handle.loaded) return;

	ma_sound_set_spatialization_enabled(entry->handle.sound.get(), entry->spatialAudio ? MA_TRUE : MA_FALSE);
	if (entry->spatialAudio) {
		ma_sound_set_attenuation_model(entry->handle.sound.get(), ma_attenuation_model_inverse);
		ma_sound_set_rolloff(entry->handle.sound.get(), 1.0f);
		ma_sound_set_min_distance(entry->handle.sound.get(), entry->minDistance);
		ma_sound_set_max_distance(entry->handle.sound.get(), entry->maxDistance);
		ma_sound_set_min_gain(entry->handle.sound.get(), 0.0f);
		ma_sound_set_max_gain(entry->handle.sound.get(), 1.0f);
	}
}

void AudioComponent::SetAudioPath(std::string name, const std::string& path) {
	AudioEntry* entry = FindAudioEntry(name);
	if (!entry) return;

	entry->audioPath = path;
	if (isActive && !EngineManager::getInstance().isHeadless) {
		AudioManager::getInstance().UnloadSound(entry->handle);
		entry->handle = AudioManager::getInstance().LoadSound(entry->audioPath, entry->streaming, entry->loop);
		ApplySpatialSettings(entry);
	}
}

void AudioComponent::SetStreaming(std::string name, bool streaming) {
	AudioEntry* entry = FindAudioEntry(name);
	if (!entry) return;

	entry->streaming = streaming;
	if (isActive && entry->audioPath != "" && !EngineManager::getInstance().isHeadless) {
		AudioManager::getInstance().UnloadSound(entry->handle);
		entry->handle = AudioManager::getInstance().LoadSound(entry->audioPath, entry->streaming, entry->loop);
	}
}

void AudioComponent::SetLooping(std::string name, bool loop) {
	AudioEntry* entry = FindAudioEntry(name);
	if (!entry) return;

	entry->loop = loop;
	if (isActive && entry->audioPath != "" && !EngineManager::getInstance().isHeadless) {
		AudioManager::getInstance().UnloadSound(entry->handle);
		entry->handle = AudioManager::getInstance().LoadSound(entry->audioPath, entry->streaming, entry->loop);
	}
}

void AudioComponent::SetSpatialAudio(std::string name, bool enabled) {
	AudioEntry* entry = FindAudioEntry(name);
	if (!entry) return;
	entry->spatialAudio = enabled;
	ApplySpatialSettings(entry);
	if (enabled) UpdatePosition();
}

void AudioComponent::SetMinDistance(std::string name, float distance) {
	AudioEntry* entry = FindAudioEntry(name);
	if (!entry) return;
	entry->minDistance = distance;
	if (entry->handle.loaded) ma_sound_set_min_distance(entry->handle.sound.get(), distance);
}

void AudioComponent::SetMaxDistance(std::string name, float distance) {
	AudioEntry* entry = FindAudioEntry(name);
	if (!entry) return;
	entry->maxDistance = distance;
	if (entry->handle.loaded) ma_sound_set_max_distance(entry->handle.sound.get(), distance);
}

void AudioComponent::PlayAudioTrack(std::string name) {
	AudioEntry* entry = FindAudioEntry(name);
	if (!entry) return;

	if (entry->handle.loaded && !EngineManager::getInstance().isHeadless) {
		ma_result result = ma_sound_start(entry->handle.sound.get());
		if (result != MA_SUCCESS) {
			Console::PrintError("AudioComponent: Unable to play sound '{}', error {}").Format(entry->audioPath, std::to_string(result));
		}
		else {
			entry->isPlaying = true;
		}
	}
}

void AudioComponent::StopAudioTrack(std::string name) {
	AudioEntry* entry = FindAudioEntry(name);
	if (!entry) return;

	if (entry->handle.loaded && !EngineManager::getInstance().isHeadless) {
		ma_result result = ma_sound_stop(entry->handle.sound.get());
		ma_sound_seek_to_pcm_frame(entry->handle.sound.get(), 0);
		if (result != MA_SUCCESS) {
			Console::PrintError("AudioComponent: Unable to stop sound '{}', error {}").Format(entry->audioPath, std::to_string(result));
		}
		else {
			entry->isPlaying = false;
		}
	}
}

void AudioComponent::PauseAudioTrack(std::string name) {
	AudioEntry* entry = FindAudioEntry(name);
	if (!entry) return;

	if (entry->handle.loaded && !EngineManager::getInstance().isHeadless) {
		ma_result result = ma_sound_stop(entry->handle.sound.get());
		if (result != MA_SUCCESS) {
			Console::PrintError("AudioComponent: Unable to stop sound '{}', error {}").Format(entry->audioPath, std::to_string(result));
		}
		else {
			entry->isPlaying = false;
		}
	}
}

std::string AudioComponent::GenerateUniqueTrackName(const std::string& baseName, const std::string* exclude) {
	std::string candidate = baseName;
	int suffix = 1;
	bool exists = true;

	while (exists) {
		exists = false;
		for (auto& entry : audioEntries) {
			if (exclude && &entry.name == exclude) continue; 
			if (entry.name == candidate) {
				exists = true;
				break;
			}
		}

		if (exists) {
			candidate = baseName + " (" + std::to_string(suffix) + ")";
			suffix++;
		}
	}

	return candidate;
}

void AudioComponent::AddAudioTrack(std::string name, const std::string& audioPath, bool streaming, bool loop, float volume) {
	AudioEntry newEntry;
	newEntry.name = GenerateUniqueTrackName(name.empty() ? "Track" : name);
	newEntry.audioPath = audioPath;
	newEntry.streaming = streaming;
	newEntry.loop = loop;
	newEntry.volume = volume;

	if (isActive && !EngineManager::getInstance().isHeadless && audioPath != "") {
		newEntry.handle = AudioManager::getInstance().LoadSound(audioPath, streaming, loop);
	}

	if (newEntry.handle.loaded) { 
		ma_sound_set_volume(newEntry.handle.sound.get(), volume);
		ApplySpatialSettings(&newEntry);
	}

	audioEntries.push_back(std::move(newEntry));
}

void AudioComponent::RemoveAudioTrack(std::string name) {
	for (auto it = audioEntries.begin(); it != audioEntries.end(); ++it) {
		if (it->name == name) {
			if (isActive && !EngineManager::getInstance().isHeadless) {
				AudioManager::getInstance().UnloadSound(it->handle);
			}
			audioEntries.erase(it);
			break;
		}
	}
}

void AudioComponent::SetVolume(std::string name, float volume) {
	AudioEntry* entry = FindAudioEntry(name);
	if (!entry) return;

	if (entry->handle.loaded && !EngineManager::getInstance().isHeadless) {
		ma_sound_set_volume(entry->handle.sound.get(), volume);
		entry->volume = volume;
	}
}