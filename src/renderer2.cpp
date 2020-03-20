#include "renderer.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

void Renderer::OnInit() {
	LoadPipeline();
	LoadAssets();
}

void Renderer::OnUpdate() {}

void Renderer::OnRender() {
	PopulateCommandList();
	ID3D12CommandList *command_lists[] = {command_list.Get()};

	command_queue->ExecuteCommandLists(_countof(command_lists), command_lists);

	ThrowIfFailed(swap_chain->Present(0, 0));

	WaitForPreviousFrame();
}

void Renderer::OnDestroy() {
	WaitForPreviousFrame();
	CloseHandle(fence_event);
}

void Renderer::LoadPipeline() {
	UINT dxgifactory_flag = 0;

	// Create debug layer
#ifdef _DEBUG
	ComPtr<ID3D12Debug> debg_controller;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debg_controller)))) {
		debg_controller->EnableDebugLayer();
	}

	dxgifactory_flag |= DXGI_CREATE_FACTORY_DEBUG;
#endif

	// Create device
	ComPtr<IDXGIFactory4> dxgifactory;
	ThrowIfFailed(CreateDXGIFactory2(dxgifactory_flag, IID_PPV_ARGS(&dxgifactory)));

	ComPtr<IDXGIAdapter1> hardware_adapter;
	ThrowIfFailed(dxgifactory->EnumAdapters1(0, &hardware_adapter));
	ThrowIfFailed(D3D12CreateDevice(hardware_adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));

	// Create a direct command queue
	D3D12_COMMAND_QUEUE_DESC queue_desc = {};
	queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue)));

	// Create swap chain
	DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
	swap_chain_desc.BufferCount = frame_number;
	swap_chain_desc.Width = GetWidth();
	swap_chain_desc.Height = GetHeight();
	swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swap_chain_desc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> temp_swap_chain;
	ThrowIfFailed(dxgifactory->CreateSwapChainForHwnd(
		command_queue.Get(),
		Win32Window::GetHwnd(),
		&swap_chain_desc,
		nullptr,
		nullptr,
		&temp_swap_chain
		));
	ThrowIfFailed(dxgifactory->MakeWindowAssociation(Win32Window::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));
	ThrowIfFailed(temp_swap_chain.As(&swap_chain));

	frame_index = swap_chain->GetCurrentBackBufferIndex();

	// Create descriptor heap for render target view
	D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
	rtv_heap_desc.NumDescriptors = frame_number;
	rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&rtv_heap)));
	rtv_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// Create render target view for each frame
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(rtv_heap->GetCPUDescriptorHandleForHeapStart());
	for (INT i = 0; i < frame_number; i++) {
		ThrowIfFailed(swap_chain->GetBuffer(i, IID_PPV_ARGS(&render_targets[i])));
		device->CreateRenderTargetView(render_targets[i].Get(), nullptr, rtv_handle);
		rtv_handle.Offset(1, rtv_descriptor_size);
	}

	// Create command allocator
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator)));
}

void Renderer::LoadAssets() {
	// Create a root signature


	// Create full PSO

	// Create command list
	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocator.Get(),
											nullptr, IID_PPV_ARGS(&command_list)));
	ThrowIfFailed(command_list->Close());

	// Create and upload vertex buffer

	// Create synchronization objects
	ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
	fence_value = 1;
	fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fence_event == nullptr) {
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}
}

void Renderer::PopulateCommandList() {
	// Reset allocators and lists
	ThrowIfFailed(command_allocator->Reset());
	ThrowIfFailed(command_list->Reset(command_allocator.Get(), pipeline_state.Get()));

	// Set initial state

	// Resource barrier from present to RT
	command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		render_targets[frame_index].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET
		));

	// Record commands
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(rtv_heap->GetCPUDescriptorHandleForHeapStart(),
											 frame_index, rtv_descriptor_size);
	const float clear_color[3] = {0.f, 0.f, 0.f};
	command_list->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);

	// Resource barrier from RT to present
	command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		render_targets[frame_index].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT
		));

	// Close command list
	ThrowIfFailed(command_list->Close());
}

void Renderer::WaitForPreviousFrame() {
	// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
	// Signal and increment the fence value.
	const UINT64 prev_fence_vale = fence_value;
	ThrowIfFailed(command_queue->Signal(fence.Get(), prev_fence_vale));
	fence_value++;

	if (fence->GetCompletedValue() < prev_fence_vale) {
		ThrowIfFailed(fence->SetEventOnCompletion(prev_fence_vale, fence_event));
		WaitForSingleObject(fence_event, INFINITE);
	}

	frame_index = swap_chain->GetCurrentBackBufferIndex();
}

std::wstring Renderer::GetBinPath(std::wstring shader_file) const {
	return std::wstring(L"suutb");
}
