#pragma once

class DirectXCommon;
class WinApp;

class ImGuiManager {
public:
	/// <summary>
	/// Initialize ImGui backends.
	/// </summary>
	void Initialize(WinApp* winApp, DirectXCommon* dxCommon);

	/// <summary>
	/// Begin an ImGui frame.
	/// </summary>
	void Begin();

	/// <summary>
	/// End an ImGui frame.
	/// </summary>
	void End();

	/// <summary>
	/// Draw ImGui to the current command list.
	/// </summary>
	void Draw();

	/// <summary>
	/// Release ImGui resources.
	/// </summary>
	void Finalize();

private:
#ifdef _DEBUG
	DirectXCommon* dxCommon_ = nullptr;
	bool showDemoWindow_ = false;
	bool enableDockSpacePassthrough_ = false;

	void ApplyStyle_();
	void BeginDockSpace_();
#endif
};
