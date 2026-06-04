#include "GpuParticleEditor.h"

#include "GpuParticleEffectSerializer.h"
#include "GpuParticleRenderer.h"
#include "GpuParticleResources.h"
#include "GpuParticleTypes.h"

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

namespace {

template<typename TValue>
void ClampSelection(int& selection, const std::vector<TValue>& values) {
	selection = values.empty() ? -1 : std::clamp(selection, 0, static_cast<int>(values.size() - 1));
}

struct ParticleTypeEditResult {
	bool changed = false;
	bool textureChanged = false;
};

const char* ToBlendName(D3D12_BLEND blend) {
	switch (blend) {
	case D3D12_BLEND_ZERO:
		return "ZERO";
	case D3D12_BLEND_ONE:
		return "ONE";
	case D3D12_BLEND_SRC_ALPHA:
		return "SRC_ALPHA";
	case D3D12_BLEND_INV_SRC_ALPHA:
		return "INV_SRC_ALPHA";
	default:
		return "Other";
	}
}

const char* ToEmitterShapeLabel(GpuParticle::EmitterShape shape) {
	switch (GpuParticle::ClampEmitterShape(shape)) {
	case GpuParticle::EmitterShape::Box:
		return "Box";
	case GpuParticle::EmitterShape::Cone:
		return "Cone";
	case GpuParticle::EmitterShape::Sphere:
	default:
		return "Sphere";
	}
}

GpuParticle::ParticleType MakeResetParticleType(size_t index, const std::string& currentName) {
	GpuParticle::State defaultState;
	GpuParticle::EnsureDefaultParticleTypes(defaultState);
	GpuParticle::ParticleType value = GpuParticle::GetParticleType(defaultState, static_cast<uint32_t>(index));
	value.name = currentName.empty() ? "Particle Type " + std::to_string(index) : currentName;
	return value;
}

void RequestPreviewRefresh(GpuParticle::State& state) {
	if (state.useFreeListEmit) {
		GpuParticle::ResetListsForFreeListMode(state);
		GpuParticle::RequestInitialize(state);
		GpuParticle::RequestEmit(state);
		return;
	}
	GpuParticle::RequestInitialize(state);
}

std::string GetEffectPathOrDefault(char* pathBuffer, size_t pathBufferSize) {
	if (pathBuffer[0] == '\0') {
		strncpy_s(pathBuffer, pathBufferSize, GpuParticle::GpuParticleEffectSerializer::kDefaultPath, _TRUNCATE);
	}
	return pathBuffer;
}

void DrawEmitterList(const GpuParticle::State& state, int& selectedEmitterIndex) {
#ifdef _DEBUG
	ImGui::BeginChild("EmitterList", ImVec2(0.0f, 116.0f), ImGuiChildFlags_Borders);
	for (size_t emitterIndex = 0; emitterIndex < state.emitters.size(); ++emitterIndex) {
		const GpuParticle::Emitter& emitter = state.emitters[emitterIndex];
		char label[96]{};
		snprintf(
			label,
			sizeof(label),
			"%zu: %s  %s  Count %u  Type %u",
			emitterIndex,
			emitter.enabled ? "Enabled" : "Disabled",
			ToEmitterShapeLabel(emitter.shape),
			emitter.emitCount,
			emitter.particleTypeIndex);
		if (ImGui::Selectable(label, selectedEmitterIndex == static_cast<int>(emitterIndex))) {
			selectedEmitterIndex = static_cast<int>(emitterIndex);
		}
	}
	ImGui::EndChild();
#else
	(void)state;
	(void)selectedEmitterIndex;
#endif
}

bool DrawEmitterInspector(GpuParticle::State& state, int selectedEmitterIndex) {
#ifdef _DEBUG
	if (selectedEmitterIndex < 0 || selectedEmitterIndex >= static_cast<int>(state.emitters.size())) {
		ImGui::TextUnformatted("編集するエミッターを選択してください。");
		return false;
	}

	GpuParticle::Emitter& emitter = state.emitters[selectedEmitterIndex];
	bool changed = false;
	ImGui::Text("選択中エミッター (Selected Emitter): %d", selectedEmitterIndex);
	changed |= ImGui::Checkbox("エミッター有効 (Enabled)", &emitter.enabled);
	changed |= ImGui::DragFloat3("発生位置 (Position)", &emitter.position.x, 0.05f, -10.0f, 10.0f, "%.2f");
	changed |= ImGui::DragFloat("発生半径 (Radius)", &emitter.radius, 0.01f, 0.0f, 5.0f, "%.2f");
	const char* shapeItems[] = {
		"Sphere",
		"Box",
		"Cone",
	};
	int shapeIndex = static_cast<int>(GpuParticle::ClampEmitterShape(emitter.shape));
	if (ImGui::Combo("発生形状 (Emitter Shape)", &shapeIndex, shapeItems, IM_ARRAYSIZE(shapeItems))) {
		emitter.shape = static_cast<GpuParticle::EmitterShape>(std::clamp(shapeIndex, 0, static_cast<int>(GpuParticle::kEmitterShapeCount - 1)));
		changed = true;
	}
	if (emitter.shape == GpuParticle::EmitterShape::Box) {
		changed |= ImGui::DragFloat3("箱サイズ (Box Size)", &emitter.boxSize.x, 0.01f, 0.0f, 10.0f, "%.2f");
	} else if (emitter.shape == GpuParticle::EmitterShape::Cone) {
		changed |= ImGui::DragFloat("円錐高さ (Cone Height)", &emitter.coneHeight, 0.01f, 0.001f, 10.0f, "%.2f");
	}
	int emitCount = static_cast<int>(emitter.emitCount);
	if (ImGui::DragInt("発生数 (Emit Count)", &emitCount, 1.0f, 0, static_cast<int>(GpuParticle::kParticleCount))) {
		emitter.emitCount = static_cast<uint32_t>(std::clamp(emitCount, 0, static_cast<int>(GpuParticle::kParticleCount)));
		changed = true;
	}
	if (ImGui::DragFloat("発生間隔 (Emit Interval)", &emitter.emitInterval, 0.01f, 0.0f, 10.0f, "%.2f sec")) {
		emitter.emitInterval = (std::max)(0.0f, emitter.emitInterval);
		emitter.emitTimer = 0.0f;
		changed = true;
	}
	if (ImGui::DragFloat("発生レート (Emission Rate)", &emitter.emissionRate, 0.1f, 0.0f, 5000.0f, "%.1f / sec")) {
		emitter.emissionRate = (std::max)(emitter.emissionRate, 0.0f);
		emitter.emissionAccumulator = 0.0f;
		changed = true;
	}
	changed |= ImGui::InputScalar("乱数シード (Random Seed)", ImGuiDataType_U32, &emitter.randomSeed);
	const GpuParticle::ParticleType& selectedType = GpuParticle::GetParticleType(state, emitter.particleTypeIndex);
	const char* typeName = selectedType.name.empty() ? "Unnamed" : selectedType.name.c_str();
	if (ImGui::BeginCombo("パーティクル種類 (Particle Type)", typeName)) {
		for (size_t particleTypeIndex = 0; particleTypeIndex < state.particleTypes.size(); ++particleTypeIndex) {
			const GpuParticle::ParticleType& type = state.particleTypes[particleTypeIndex];
			const bool isSelected = emitter.particleTypeIndex == particleTypeIndex;
			const char* itemName = type.name.empty() ? "Unnamed" : type.name.c_str();
			if (ImGui::Selectable(itemName, isSelected)) {
				emitter.particleTypeIndex = static_cast<uint32_t>(particleTypeIndex);
				changed = true;
			}
			if (isSelected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	ImGui::Text("発生タイマー (Emit Timer): %.2f / %.2f", emitter.emitTimer, emitter.emitInterval);
	ImGui::Text("発生レート蓄積 (Emission Accumulator): %.3f", emitter.emissionAccumulator);
	ImGui::Text("発生待ち数 (Pending Emit Count): %u", emitter.pendingEmitCount);
	ImGui::Text("発生待ち (Pending Emit): %s", emitter.pendingEmit ? "true" : "false");
	emitter.shape = GpuParticle::ClampEmitterShape(emitter.shape);
	emitter.boxSize.x = (std::max)(emitter.boxSize.x, 0.0f);
	emitter.boxSize.y = (std::max)(emitter.boxSize.y, 0.0f);
	emitter.boxSize.z = (std::max)(emitter.boxSize.z, 0.0f);
	emitter.coneHeight = (std::max)(emitter.coneHeight, 0.001f);
	emitter.emissionRate = (std::max)(emitter.emissionRate, 0.0f);
	return changed;
#else
	(void)state;
	(void)selectedEmitterIndex;
	return false;
#endif
}

void DrawParticleTypeList(const GpuParticle::State& state, int& selectedParticleTypeIndex) {
#ifdef _DEBUG
	ImGui::BeginChild("ParticleTypeList", ImVec2(0.0f, 116.0f), ImGuiChildFlags_Borders);
	for (size_t particleTypeIndex = 0; particleTypeIndex < state.particleTypes.size(); ++particleTypeIndex) {
		const GpuParticle::ParticleType& type = state.particleTypes[particleTypeIndex];
		char label[96]{};
		snprintf(label, sizeof(label), "%zu: %s%s", particleTypeIndex, type.name.empty() ? "Unnamed" : type.name.c_str(), type.useAtlas ? "  [Atlas]" : "");
		if (ImGui::Selectable(label, selectedParticleTypeIndex == static_cast<int>(particleTypeIndex))) {
			selectedParticleTypeIndex = static_cast<int>(particleTypeIndex);
		}
	}
	ImGui::EndChild();
#else
	(void)state;
	(void)selectedParticleTypeIndex;
#endif
}

ParticleTypeEditResult DrawParticleTypeInspector(GpuParticle::State& state, int selectedParticleTypeIndex, const GpuParticleRenderer& renderer) {
#ifdef _DEBUG
	if (selectedParticleTypeIndex < 0 || selectedParticleTypeIndex >= static_cast<int>(state.particleTypes.size())) {
		ImGui::TextUnformatted("編集するパーティクル種類を選択してください。");
		return {};
	}

	GpuParticle::ParticleType& type = state.particleTypes[selectedParticleTypeIndex];
	ParticleTypeEditResult result;
	ImGui::Text("選択中種類 (Selected ParticleType): %d", selectedParticleTypeIndex);
	char nameBuffer[64]{};
	snprintf(nameBuffer, sizeof(nameBuffer), "%s", type.name.c_str());
	if (ImGui::InputText("名前 (Name)", nameBuffer, sizeof(nameBuffer))) {
		type.name = nameBuffer;
		result.changed = true;
	}
	char texturePathBuffer[260]{};
	snprintf(texturePathBuffer, sizeof(texturePathBuffer), "%s", type.texturePath.c_str());
	if (ImGui::InputText("テクスチャパス (Texture Path)", texturePathBuffer, sizeof(texturePathBuffer))) {
		type.texturePath = texturePathBuffer;
		result.changed = true;
		result.textureChanged = true;
	}
	if (ImGui::Button("テクスチャ再読込 (Reload Texture)")) {
		result.textureChanged = true;
	}
	ImGui::Text("テクスチャ番号 (Texture Index): %d", type.textureIndex);
	ImGui::Text("フォールバック使用中 (Using Fallback): %s", renderer.IsUsingFallbackTexture(type) ? "true" : "false");
	result.changed |= ImGui::ColorEdit4("基本色 (Base Color)", &type.baseColor.x);
	result.changed |= ImGui::ColorEdit4("開始色 (Start Color)", &type.startColor.x);
	result.changed |= ImGui::ColorEdit4("終了色 (End Color)", &type.endColor.x);
	result.changed |= ImGui::DragFloat("開始サイズ (Start Scale)", &type.startScale, 0.005f, 0.001f, 1.0f, "%.3f");
	result.changed |= ImGui::DragFloat("終了サイズ (End Scale)", &type.endScale, 0.005f, 0.001f, 1.0f, "%.3f");
	result.changed |= ImGui::DragFloat("寿命 最小 (LifeTime Min)", &type.lifeTimeMin, 0.01f, 0.01f, 30.0f, "%.2f sec");
	result.changed |= ImGui::DragFloat("寿命 最大 (LifeTime Max)", &type.lifeTimeMax, 0.01f, 0.01f, 30.0f, "%.2f sec");
	result.changed |= ImGui::DragFloat("速度 最小 (Speed Min)", &type.speedMin, 0.01f, 0.0f, 20.0f, "%.2f");
	result.changed |= ImGui::DragFloat("速度 最大 (Speed Max)", &type.speedMax, 0.01f, 0.0f, 20.0f, "%.2f");
	result.changed |= ImGui::DragFloat("重力 (Gravity)", &type.gravity, 0.01f, -20.0f, 20.0f, "%.2f");
	result.changed |= ImGui::DragFloat("空気抵抗 (Drag)", &type.drag, 0.01f, 0.0f, 10.0f, "%.2f");
	result.changed |= ImGui::Checkbox("アトラス使用 (Use Atlas)", &type.useAtlas);
	int atlasRows = static_cast<int>(type.atlasRows);
	if (ImGui::DragInt("アトラス行数 (Atlas Rows)", &atlasRows, 0.05f, 1, 64)) {
		type.atlasRows = static_cast<uint32_t>(std::clamp(atlasRows, 1, 64));
		result.changed = true;
	}
	int atlasColumns = static_cast<int>(type.atlasColumns);
	if (ImGui::DragInt("アトラス列数 (Atlas Columns)", &atlasColumns, 0.05f, 1, 64)) {
		type.atlasColumns = static_cast<uint32_t>(std::clamp(atlasColumns, 1, 64));
		result.changed = true;
	}
	int frameCount = static_cast<int>(type.frameCount);
	if (ImGui::DragInt("コマ数 (Atlas Frame Count)", &frameCount, 0.05f, 1, 4096)) {
		type.frameCount = static_cast<uint32_t>(std::clamp(frameCount, 1, 4096));
		result.changed = true;
	}
	result.changed |= ImGui::DragFloat("コマ速度 (Frame Speed)", &type.frameSpeed, 0.05f, 0.0f, 120.0f, "%.2f fps");
	result.changed |= ImGui::Checkbox("ループ再生 (Loop Atlas)", &type.loopAtlas);
	type.startScale = (std::max)(type.startScale, 0.001f);
	type.endScale = (std::max)(type.endScale, 0.001f);
	type.lifeTimeMin = (std::max)(type.lifeTimeMin, 0.01f);
	type.lifeTimeMax = (std::max)(type.lifeTimeMax, type.lifeTimeMin);
	type.speedMin = (std::max)(type.speedMin, 0.0f);
	type.speedMax = (std::max)(type.speedMax, type.speedMin);
	type.drag = (std::max)(type.drag, 0.0f);
	type.atlasRows = (std::max)(type.atlasRows, 1u);
	type.atlasColumns = (std::max)(type.atlasColumns, 1u);
	type.frameCount = std::clamp(type.frameCount, 1u, type.atlasRows * type.atlasColumns);
	type.frameSpeed = (std::max)(type.frameSpeed, 0.0f);
	ImGui::Text("アトラス最大コマ数 (Atlas Capacity): %u", type.atlasRows * type.atlasColumns);
	return result;
#else
	(void)state;
	(void)selectedParticleTypeIndex;
	(void)renderer;
	return {};
#endif
}

}

GpuParticleEditor::GpuParticleEditor() {
	strncpy_s(effectPath_, sizeof(effectPath_), GpuParticle::GpuParticleEffectSerializer::kDefaultPath, _TRUNCATE);
	GpuParticle::State defaultState;
	GpuParticle::EnsureDefaultParticleTypes(defaultState);
	GpuParticle::EnsureDefaultEmitter(defaultState);
	editingEffectData_ = GpuParticle::CreateParticleEffectDataFromState(defaultState);
}

void GpuParticleEditor::SyncEditingEffectDataFromState(const GpuParticle::State& state) {
	editingEffectData_ = GpuParticle::CreateParticleEffectDataFromState(state);
}

void GpuParticleEditor::ApplyEditingEffectDataToState(GpuParticle::State& state, GpuParticleResources& resources, GpuParticleRenderer& renderer) {
	GpuParticle::ApplyParticleEffectDataToState(editingEffectData_, state);
	renderer.ReloadParticleTypeTextures(state);
	resources.UploadParticleTypes(state);
}

void GpuParticleEditor::DrawImGui(GpuParticle::State& state, GpuParticleResources& resources, GpuParticleRenderer& renderer, uint32_t particleTextureIndex) {
#ifdef _DEBUG
	GpuParticle::EnsureDefaultParticleTypes(state);
	GpuParticle::EnsureDefaultEmitter(state);
	renderer.RefreshParticleTypeTextures(state);
	ClampSelection(selectedEmitterIndex_, state.emitters);
	ClampSelection(selectedParticleTypeIndex_, state.particleTypes);

	if (ImGui::Begin("GPUパーティクルエディタ (GPU Particle Editor)")) {
		bool needsPreviewRefresh = false;
		bool particleTypesChanged = false;

		ImGui::SeparatorText("プレビュー (Preview)");
		ImGui::Checkbox("GPUパーティクル表示 (Show GPU Particle)", &state.isEnabled);
		ImGui::Text("生存数 推定 (Alive Count Approx): %u", state.activeCountEstimate);
		ImGui::Text("エミッター数 (Emitter Count): %zu", state.emitters.size());
		ImGui::Text("パーティクル種類数 (ParticleType Count): %zu / %u", state.particleTypes.size(), GpuParticle::kMaxParticleTypes);

		ImGui::SeparatorText("描画診断 (Render Diagnostics)");
		const char* debugViewItems[] = {
			"Normal",
			"Solid Color No Texture",
			"Show Texture Alpha",
			"Show Texture RGB",
			"Show Final Alpha",
			"Force Magenta",
			"Force Discard All",
			"Show Particle Color",
			"Show Texture Alpha Transparent",
			"Procedural Circle Mask",
		};
		int debugViewMode = static_cast<int>(state.particleDebugViewMode);
		if (ImGui::Combo("PS表示モード (PS Debug View)", &debugViewMode, debugViewItems, IM_ARRAYSIZE(debugViewItems))) {
			state.particleDebugViewMode = static_cast<uint32_t>(std::clamp(debugViewMode, 0, IM_ARRAYSIZE(debugViewItems) - 1));
		}
		ImGui::Text("BlendEnable: %s", renderer.IsParticleBlendEnabled() ? "true" : "false");
		ImGui::Text("Color Blend: Src=%s Dest=%s", ToBlendName(renderer.GetParticleSrcBlend()), ToBlendName(renderer.GetParticleDestBlend()));
		ImGui::Text("Alpha Blend: Src=%s Dest=%s", ToBlendName(renderer.GetParticleSrcBlendAlpha()), ToBlendName(renderer.GetParticleDestBlendAlpha()));
		if (selectedParticleTypeIndex_ >= 0 && selectedParticleTypeIndex_ < static_cast<int>(state.particleTypes.size())) {
			const GpuParticle::ParticleType& selectedType = state.particleTypes[selectedParticleTypeIndex_];
			ImGui::Text("選択Type TextureIndex: %d", selectedType.textureIndex);
			ImGui::TextWrapped("選択Type TexturePath: %s", selectedType.texturePath.empty() ? "(fallback circle2.png)" : selectedType.texturePath.c_str());
			ImGui::Text("選択Type Fallback: %s", renderer.IsUsingFallbackTexture(selectedType) ? "true" : "false");
			ImGui::Text("選択Type BaseColor RGBA: %.3f, %.3f, %.3f, %.3f",
				selectedType.baseColor.x, selectedType.baseColor.y, selectedType.baseColor.z, selectedType.baseColor.w);
			ImGui::Text("選択Type StartColor RGBA: %.3f, %.3f, %.3f, %.3f",
				selectedType.startColor.x, selectedType.startColor.y, selectedType.startColor.z, selectedType.startColor.w);
			ImGui::Text("選択Type EndColor RGBA: %.3f, %.3f, %.3f, %.3f",
				selectedType.endColor.x, selectedType.endColor.y, selectedType.endColor.z, selectedType.endColor.w);
			ImGui::Text("選択Type Drag: %.3f", selectedType.drag);
			ImGui::TextUnformatted("Normal color は VS で StartColor から EndColor へ寿命補間されます。");
		}
		if (selectedEmitterIndex_ >= 0 && selectedEmitterIndex_ < static_cast<int>(state.emitters.size())) {
			const GpuParticle::Emitter& selectedEmitter = state.emitters[selectedEmitterIndex_];
			const GpuParticle::ParticleType& emitterType = GpuParticle::GetParticleType(state, selectedEmitter.particleTypeIndex);
			ImGui::Text("選択Emitter ParticleTypeIndex: %u", selectedEmitter.particleTypeIndex);
			ImGui::Text("選択Emitter Sample TextureIndex: %d", emitterType.textureIndex);
			ImGui::TextWrapped("選択Emitter Sample TexturePath: %s", emitterType.texturePath.empty() ? "(fallback circle2.png)" : emitterType.texturePath.c_str());
			ImGui::Text("選択Emitter Fallback: %s", renderer.IsUsingFallbackTexture(emitterType) ? "true" : "false");
		}

		ImGui::SeparatorText("実行状態 (Runtime)");
		ImGui::Checkbox("更新CS有効 (Update Particle CS)", &state.isUpdateEnabled);
		ImGui::DragFloat("経過時間 (Delta Time)", &state.deltaTime, 0.001f, 0.0f, 1.0f / 15.0f, "%.4f");
		ImGui::Checkbox("乱数初期化 (Random Initialize)", &state.isRandomInitializeEnabled);
		ImGui::Checkbox("エミッターシステム有効 (Emitter System)", &state.isEmitterEnabled);
		if (ImGui::Checkbox("未使用リスト発生 (Use FreeList Emit)", &state.useFreeListEmit)) {
			GpuParticle::ResetListsForFreeListMode(state);
			GpuParticle::RequestInitialize(state);
			if (state.useFreeListEmit) {
				GpuParticle::RequestEmit(state);
			}
		}
		if (ImGui::Checkbox("死亡リスト使用 (Use DeadList)", &state.useDeadList)) {
			GpuParticle::ResetListsForFreeListMode(state);
			if (state.useFreeListEmit) {
				GpuParticle::RequestInitialize(state);
				GpuParticle::RequestEmit(state);
			}
		}
		ImGui::Checkbox("死亡リスト自動再利用 (Auto Recycle DeadList)", &state.autoRecycleDeadList);
		int recycleCount = static_cast<int>(state.recycleCount);
		if (ImGui::DragInt("再利用数 (Recycle Count)", &recycleCount, 1.0f, 0, static_cast<int>(GpuParticle::kParticleCount))) {
			state.recycleCount = static_cast<uint32_t>(std::clamp(recycleCount, 0, static_cast<int>(GpuParticle::kParticleCount)));
		}
		if (ImGui::Button("死亡リストを未使用リストへ再利用 (Recycle)")) {
			GpuParticle::RequestRecycle(state);
		}
		ImGui::Text("前回再利用Dispatch数 (Last Recycle Dispatch Count): %u", state.lastRecycleDispatchCount);

		ImGui::SeparatorText("エフェクトJSON (Effect JSON)");
		ImGui::InputText("保存/読込パス (Effect Path)", effectPath_, sizeof(effectPath_));
		if (ImGui::Button("エフェクト保存 (Save Effect)")) {
			const std::string path = GetEffectPathOrDefault(effectPath_, sizeof(effectPath_));
			SyncEditingEffectDataFromState(state);
			if (GpuParticle::GpuParticleEffectSerializer::Save(editingEffectData_, path)) {
				effectIoStatus_ = "Saved effect JSON: " + path;
			} else {
				effectIoStatus_ = "Save failed: could not write effect JSON.";
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("エフェクト読込 (Load Effect)")) {
			const std::string path = GetEffectPathOrDefault(effectPath_, sizeof(effectPath_));
			if (GpuParticle::GpuParticleEffectSerializer::Load(path, editingEffectData_)) {
				ApplyEditingEffectDataToState(state, resources, renderer);
				GpuParticle::ClampEmitterParticleTypeIndices(state);
				ClampSelection(selectedEmitterIndex_, state.emitters);
				ClampSelection(selectedParticleTypeIndex_, state.particleTypes);
				effectIoStatus_ = "Loaded effect JSON: " + path;
			} else {
				effectIoStatus_ = "Load failed: could not parse effect JSON.";
			}
		}
		if (!effectIoStatus_.empty()) {
			ImGui::TextWrapped("JSON状態 (JSON Status): %s", effectIoStatus_.c_str());
		}

		ImGui::SeparatorText("パーティクル種類 (Particle Types)");
		ImGui::BeginDisabled(state.particleTypes.size() >= GpuParticle::kMaxParticleTypes);
		if (ImGui::Button("パーティクル種類を追加 (Add)")) {
			GpuParticle::ParticleType type = MakeResetParticleType(state.particleTypes.size(), "Particle Type " + std::to_string(state.particleTypes.size()));
			state.particleTypes.push_back(type);
			selectedParticleTypeIndex_ = static_cast<int>(state.particleTypes.size() - 1);
			particleTypesChanged = true;
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(selectedParticleTypeIndex_ < 0 || state.particleTypes.size() >= GpuParticle::kMaxParticleTypes);
		if (ImGui::Button("複製 (Duplicate)##ParticleType")) {
			GpuParticle::ParticleType copy = state.particleTypes[selectedParticleTypeIndex_];
			copy.name += " Copy";
			state.particleTypes.push_back(copy);
			selectedParticleTypeIndex_ = static_cast<int>(state.particleTypes.size() - 1);
			particleTypesChanged = true;
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(selectedParticleTypeIndex_ < 0);
		if (ImGui::Button("初期化 (Reset)##ParticleType")) {
			const std::string name = state.particleTypes[selectedParticleTypeIndex_].name;
			state.particleTypes[selectedParticleTypeIndex_] = MakeResetParticleType(static_cast<size_t>(selectedParticleTypeIndex_), name);
			particleTypesChanged = true;
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(state.particleTypes.size() <= 1 || selectedParticleTypeIndex_ < 0);
		if (ImGui::Button("削除 (Remove)##ParticleType")) {
			state.particleTypes.erase(state.particleTypes.begin() + selectedParticleTypeIndex_);
			GpuParticle::EnsureDefaultParticleTypes(state);
			ClampSelection(selectedParticleTypeIndex_, state.particleTypes);
			GpuParticle::ClampEmitterParticleTypeIndices(state);
			particleTypesChanged = true;
			needsPreviewRefresh |= !state.useFreeListEmit;
		}
		ImGui::EndDisabled();
		DrawParticleTypeList(state, selectedParticleTypeIndex_);
		ImGui::SeparatorText("種類詳細 (ParticleType Inspector)");
		const ParticleTypeEditResult particleTypeEditResult = DrawParticleTypeInspector(state, selectedParticleTypeIndex_, renderer);
		particleTypesChanged |= particleTypeEditResult.changed;
		if (particleTypeEditResult.textureChanged) {
			renderer.ReloadParticleTypeTextures(state);
			resources.UploadParticleTypes(state);
		}

		ImGui::SeparatorText("エミッター (Emitters)");
		if (ImGui::Button("エミッターを追加 (Add)")) {
			GpuParticle::Emitter emitter;
			emitter.position.x = static_cast<float>(state.emitters.size()) * 1.5f;
			emitter.randomSeed = static_cast<uint32_t>(state.emitters.size()) + 1u;
			emitter.particleTypeIndex = static_cast<uint32_t>(state.emitters.size() % state.particleTypes.size());
			state.emitters.push_back(emitter);
			selectedEmitterIndex_ = static_cast<int>(state.emitters.size() - 1);
			needsPreviewRefresh |= !state.useFreeListEmit;
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(selectedEmitterIndex_ < 0);
		if (ImGui::Button("複製 (Duplicate)##Emitter")) {
			GpuParticle::Emitter copy = state.emitters[selectedEmitterIndex_];
			copy.pendingEmit = false;
			copy.emitTimer = 0.0f;
			copy.randomSeed += 1u;
			state.emitters.push_back(copy);
			selectedEmitterIndex_ = static_cast<int>(state.emitters.size() - 1);
			needsPreviewRefresh |= !state.useFreeListEmit;
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(selectedEmitterIndex_ < 0);
		if (ImGui::Button("初期化 (Reset)##Emitter")) {
			const uint32_t particleTypeIndex = state.emitters[selectedEmitterIndex_].particleTypeIndex;
			state.emitters[selectedEmitterIndex_] = GpuParticle::Emitter{};
			state.emitters[selectedEmitterIndex_].particleTypeIndex = particleTypeIndex;
			needsPreviewRefresh |= !state.useFreeListEmit;
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(state.emitters.size() <= 1 || selectedEmitterIndex_ < 0);
		if (ImGui::Button("削除 (Remove)##Emitter")) {
			state.emitters.erase(state.emitters.begin() + selectedEmitterIndex_);
			GpuParticle::EnsureDefaultEmitter(state);
			ClampSelection(selectedEmitterIndex_, state.emitters);
			needsPreviewRefresh |= !state.useFreeListEmit;
		}
		ImGui::EndDisabled();
		DrawEmitterList(state, selectedEmitterIndex_);
		ImGui::SeparatorText("エミッター詳細 (Emitter Inspector)");
		needsPreviewRefresh |= !state.useFreeListEmit && DrawEmitterInspector(state, selectedEmitterIndex_);
		ImGui::Text("エミッター生存上限 推定 (Active Limit Approx): %u", state.isEmitterEnabled ? GpuParticle::GetEnabledEmitterEmitCountSum(state) : 0u);

		if (particleTypesChanged) {
			GpuParticle::ClampEmitterParticleTypeIndices(state);
			resources.UploadParticleTypes(state);
		}
		if (needsPreviewRefresh) {
			RequestPreviewRefresh(state);
		}
		if (particleTypesChanged || needsPreviewRefresh) {
			SyncEditingEffectDataFromState(state);
		}

		ImGui::SeparatorText("GPUリソース (Resources)");
		ImGui::Text("未使用リスト準備済み (FreeList Ready): %s", state.isFreeListInitialized ? "true" : "false");
		ImGui::Text("未使用リストUAV番号 (FreeList UAV Index): %u", resources.GetFreeListUavIndex());
		ImGui::Text("未使用リストカウンタ準備 (FreeList Counter Ready): %s", resources.HasFreeListCounter() ? "true" : "false");
		ImGui::Text("未使用残数 推定 (FreeList Remaining Approx): %u", state.freeListRemainingEstimate);
		ImGui::Text("死亡リスト準備済み (DeadList Ready): %s", state.isDeadListReady ? "true" : "false");
		ImGui::Text("死亡リストUAV番号 (DeadList UAV Index): %u", resources.GetDeadListUavIndex());
		ImGui::Text("死亡リストカウンタ準備 (DeadList Counter Ready): %s", resources.HasDeadListCounter() ? "true" : "false");
		ImGui::Text("死亡数 推定 (DeadList Count Approx): %u", state.deadListCountEstimate);
		if (state.isCounterReadbackValid) {
			ImGui::Text("未使用数 実値 (FreeList Actual): %u", state.actualFreeListCount);
			ImGui::Text("死亡数 実値 (DeadList Actual): %u", state.actualDeadListCount);
		} else {
			ImGui::TextUnformatted("未使用数 実値 (FreeList Actual): N/A");
			ImGui::TextUnformatted("死亡数 実値 (DeadList Actual): N/A");
		}
		ImGui::Checkbox("カウンタ自動GPU読込 (Auto Readback Counters)", &state.autoReadbackCounters);
		if (ImGui::Button("カウンタをGPU読込 (Readback Counters)")) {
			GpuParticle::RequestCounterReadback(state);
		}
		ImGui::Text("GPU読込有効 (Readback Valid): %s", state.isCounterReadbackValid ? "true" : "false");
		ImGui::Text("GPU読込待ち (Readback Pending): %s", state.isCounterReadbackPending ? "true" : "false");
		ImGui::Text("パーティクル総数 (Particle Count): %u", GpuParticle::kParticleCount);
		ImGui::Text("描画呼び出し (Draw): DrawInstanced(6, %u, 0, 0)", GpuParticle::kParticleCount);
		ImGui::Text("初期化Dispatchグループ数: %u", (GpuParticle::kParticleCount + 1023) / 1024);
		ImGui::Text("更新Dispatchグループ数: %u", (GpuParticle::kParticleCount + 1023) / 1024);
		ImGui::Text("未使用リスト初期化Dispatchグループ数: %u", (GpuParticle::kParticleCount + 1023) / 1024);
		ImGui::Text("再利用Dispatch (Recycle): %s", state.needsRecycleDispatch ? "pending" : "idle");
		ImGui::Text("発生Dispatch (Emit): %s", state.needsEmitDispatch ? "pending" : "idle");
		ImGui::Text("前回発生Dispatch数 (Last Emit): %u", state.lastEmitDispatchCount);
		ImGui::Text("CS初期化状態 (Initialized By CS): %s", state.needsInitializeDispatch ? "pending" : "done");
		ImGui::Text("パーティクルSRV番号 (Particle SRV): %u", resources.GetParticleSrvIndex());
		ImGui::Text("パーティクルUAV番号 (Particle UAV): %u", resources.GetParticleUavIndex());
		ImGui::Text("種類SRV番号 (Particle Type SRV): %u", resources.GetParticleTypeSrvIndex());
		ImGui::Text("アトラステクスチャ番号 (Atlas Texture): %u", particleTextureIndex);
		ImGui::TextUnformatted("カウンタ実値は少なくとも1フレーム遅れでGPU読込します。パーティクル本体は未読込です。");

		ImGui::SeparatorText("デバッグ操作 (Debug Actions)");
		if (ImGui::Button("未使用リストから発生 (Emit From FreeList)")) {
			GpuParticle::RequestEmit(state);
		}
		if (ImGui::Button("GPUパーティクル再初期化 (Reinitialize)")) {
			RequestPreviewRefresh(state);
		}
		if (ImGui::Button("次シードで再初期化 (Next Seed)")) {
			for (GpuParticle::Emitter& emitter : state.emitters) {
				++emitter.randomSeed;
			}
			RequestPreviewRefresh(state);
		}
		if (ImGui::Button("リスト初期化 / 全再初期化 (Reset All)")) {
			GpuParticle::ResetListsForFreeListMode(state);
			GpuParticle::RequestInitialize(state);
			if (state.useFreeListEmit) {
				GpuParticle::RequestEmit(state);
			}
		}
	}
	ImGui::End();
#else
	(void)state;
	(void)resources;
	(void)renderer;
	(void)particleTextureIndex;
#endif
}
