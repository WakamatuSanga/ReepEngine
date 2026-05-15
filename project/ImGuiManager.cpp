#include "ImGuiManager.h"

#ifdef _DEBUG
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "WinApp.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

namespace {
	void AllocateImGuiSrvDescriptor(
		[[maybe_unused]] ImGui_ImplDX12_InitInfo* info,
		D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle) {
		SrvManager* srvManager = SrvManager::GetInstance();
		uint32_t srvIndex = srvManager->Allocate();
		*outCpuHandle = srvManager->GetCPUDescriptorHandle(srvIndex);
		*outGpuHandle = srvManager->GetGPUDescriptorHandle(srvIndex);
	}

	void FreeImGuiSrvDescriptor(
		[[maybe_unused]] ImGui_ImplDX12_InitInfo* info,
		[[maybe_unused]] D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
		[[maybe_unused]] D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) {
		// SrvManager currently owns a linear descriptor allocator, so descriptors are kept for the app lifetime.
	}
}
#endif

void ImGuiManager::Initialize([[maybe_unused]] WinApp* winApp, [[maybe_unused]] DirectXCommon* dxCommon) {
#ifdef _DEBUG
	dxCommon_ = dxCommon;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
#ifdef IMGUI_HAS_DOCK
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif
	ApplyStyle_();

	ImGui_ImplWin32_Init(winApp->GetHwnd());

	ImGui_ImplDX12_InitInfo initInfo{};
	initInfo.Device = dxCommon->GetDevice();
	initInfo.CommandQueue = dxCommon->GetCommandQueue();
	initInfo.NumFramesInFlight = static_cast<int>(dxCommon->GetBackBufferCount());
	initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	initInfo.SrvDescriptorHeap = SrvManager::GetInstance()->GetSrvDescriptorHeap();
	initInfo.SrvDescriptorAllocFn = AllocateImGuiSrvDescriptor;
	initInfo.SrvDescriptorFreeFn = FreeImGuiSrvDescriptor;
	ImGui_ImplDX12_Init(&initInfo);
#endif
}

void ImGuiManager::Begin() {
#ifdef _DEBUG
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	BeginDockSpace_();
#endif
}

void ImGuiManager::End() {
#ifdef _DEBUG
	ImGui::Render();
#endif
}

void ImGuiManager::Draw() {
#ifdef _DEBUG
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	ID3D12DescriptorHeap* descriptorHeaps[] = { SrvManager::GetInstance()->GetSrvDescriptorHeap() };
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif
}

void ImGuiManager::Finalize() {
#ifdef _DEBUG
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif
}

#ifdef _DEBUG
void ImGuiManager::ApplyStyle_() {
	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 6.0f;
	style.ChildRounding = 4.0f;
	style.FrameRounding = 4.0f;
	style.PopupRounding = 4.0f;
	style.ScrollbarRounding = 6.0f;
	style.GrabRounding = 4.0f;
	style.TabRounding = 4.0f;
	style.WindowBorderSize = 1.0f;
	style.FrameBorderSize = 0.0f;
	style.WindowPadding = ImVec2(10.0f, 8.0f);
	style.FramePadding = ImVec2(8.0f, 4.0f);
	style.ItemSpacing = ImVec2(8.0f, 6.0f);
}

void ImGuiManager::BeginDockSpace_() {
#ifdef IMGUI_HAS_DOCK
	ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;
	ImGuiWindowFlags windowFlags =
		ImGuiWindowFlags_MenuBar |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus;

	if (enableDockSpacePassthrough_) {
		dockspaceFlags |= ImGuiDockNodeFlags_PassthruCentralNode;
		windowFlags |= ImGuiWindowFlags_NoBackground;
	}

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("Main DockSpace", nullptr, windowFlags);
	ImGui::PopStyleVar(3);

	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("Window")) {
			ImGui::MenuItem("ImGui Demo Window", nullptr, &showDemoWindow_);
			ImGui::Separator();
			ImGui::MenuItem("DockSpace Passthrough", nullptr, &enableDockSpacePassthrough_);
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	ImGuiID dockspaceId = ImGui::GetID("MainDockSpaceID");
	ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspaceFlags);
	ImGui::End();
#else
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("Window")) {
			ImGui::MenuItem("ImGui Demo Window", nullptr, &showDemoWindow_);
			ImGui::TextDisabled("Docking requires Dear ImGui docking branch.");
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
#endif

	if (showDemoWindow_) {
		ImGui::ShowDemoWindow(&showDemoWindow_);
	}
}
#endif
