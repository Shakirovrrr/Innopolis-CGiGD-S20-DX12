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
	ID3D12CommandList *commandLists[] = {command_list.Get()};
	command_queue->ExecuteCommandLists(_countof(commandLists), commandLists);
	ThrowIfFailed(swap_chain->Present(0, 0));
	WaitForPreviousFrame();
}

void Renderer::OnDestroy() {
	WaitForPreviousFrame();
	CloseHandle(fence_event);
}

void Renderer::LoadPipeline() {
	// Create debug layer
	UINT dxgiFactoryFlag = 0;
#ifdef DEBUG
	ComPtr<ID3D12Debug> dbgController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbgController)))) {
		dbgController->EnableDebugLayer();
		dxgiFactoryFlag |= DXGI_CREATE_FACTORY_DEBUG;
	}
#endif // DEBUG

	// Create device
	ComPtr<IDXGIFactory4> dxgiFactory;
	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlag, IID_PPV_ARGS(&dxgiFactory)));

	ComPtr<IDXGIAdapter1> hardwareAdapter;
	ThrowIfFailed(dxgiFactory->EnumAdapters1(0, &hardwareAdapter));
	ThrowIfFailed(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));

	// Create a direct command queue
	D3D12_COMMAND_QUEUE_DESC queueDescriptor = {};
	queueDescriptor.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDescriptor.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(device->CreateCommandQueue(&queueDescriptor, IID_PPV_ARGS(&command_queue)));

	// Create swap chain
	DXGI_SWAP_CHAIN_DESC1 swapChainDescriptor = {};
	swapChainDescriptor.BufferCount = frame_number;
	swapChainDescriptor.Width = GetWidth();
	swapChainDescriptor.Height = GetHeight();
	swapChainDescriptor.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDescriptor.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDescriptor.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDescriptor.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> tempSwapChain;
	ThrowIfFailed(dxgiFactory->CreateSwapChainForHwnd(
		command_queue.Get(),
		Win32Window::GetHwnd(),
		&swapChainDescriptor,
		nullptr, nullptr,
		&tempSwapChain
		));

	ThrowIfFailed(dxgiFactory->MakeWindowAssociation(Win32Window::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));
	ThrowIfFailed(tempSwapChain.As(&swap_chain));

	frame_index = swap_chain->GetCurrentBackBufferIndex();

	// Create descriptor heap for render target view
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDescriptor = {};
	rtvHeapDescriptor.NumDescriptors = frame_number;
	rtvHeapDescriptor.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDescriptor.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(device->CreateDescriptorHeap(&rtvHeapDescriptor, IID_PPV_ARGS(&rtv_heap)));
	rtv_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// Create render target view for each frame
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtv_heap->GetCPUDescriptorHandleForHeapStart());
	for (unsigned int i = 0; i < frame_number; i++) {
		ThrowIfFailed(swap_chain->GetBuffer(i, IID_PPV_ARGS(&render_targets[i])));
		device->CreateRenderTargetView(render_targets[i].Get(), nullptr, rtvHandle);
		rtvHandle.Offset(1, rtv_descriptor_size);
	}

	// Create command allocator
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator)));
}

void Renderer::LoadAssets() {
	// Create a root signature
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDescriptor = {};
	rootSignatureDescriptor.Init(0, nullptr, 0, nullptr,
								 D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3D10Blob> signature;
	ComPtr<ID3D10Blob> error;
	ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDescriptor,
											  D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
	ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(),
											  signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));

	// Create full PSO
	ComPtr<ID3D10Blob> vertexShader;
	ComPtr<ID3D10Blob> pixelShader;

	UINT compileFlags = 0;
#ifdef DEBUG
	compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif // DEBUG

	std::wstring shaderPath = GetBinPath(std::wstring(L"shaders.hlsl"));
	ThrowIfFailed(D3DCompileFromFile(shaderPath.c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &error));
	ThrowIfFailed(D3DCompileFromFile(shaderPath.c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &error));

	D3D12_INPUT_ELEMENT_DESC inputElementDescriptors[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDescriptor = {};
	psoDescriptor.InputLayout = {inputElementDescriptors, _countof(inputElementDescriptors)};
	psoDescriptor.pRootSignature = root_signature.Get();
	psoDescriptor.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
	psoDescriptor.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
	psoDescriptor.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDescriptor.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDescriptor.DepthStencilState.DepthEnable = false;
	psoDescriptor.DepthStencilState.StencilEnable = false;
	psoDescriptor.SampleMask = UINT_MAX;
	psoDescriptor.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDescriptor.NumRenderTargets = 1;
	psoDescriptor.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDescriptor.SampleDesc.Count = 1;

	ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDescriptor, IID_PPV_ARGS(&pipeline_state)));

	// Create command list
	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, command_allocator.Get(), pipeline_state.Get(), IID_PPV_ARGS(&command_list)));
	ThrowIfFailed(command_list->Close());

	// Create and upload vertex buffer
	ColorVertex triangleVertices[] = {
		{{0.0f, 0.25f * aspectRatio, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
		{{0.25f, -0.25f * aspectRatio, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
		{{-0.25f, -0.25f * aspectRatio, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}
	};

	const UINT vertexBufferSize = sizeof(triangleVertices);
	ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
												  D3D12_HEAP_FLAG_NONE,
												  &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
												  D3D12_RESOURCE_STATE_GENERIC_READ,
												  nullptr,
												  IID_PPV_ARGS(&vertex_buffer)
												  ));

	UINT8 *vertexDataBegin;
	CD3DX12_RANGE readRange(0, 0);
	ThrowIfFailed(vertex_buffer->Map(0, &readRange, reinterpret_cast<void **>(&vertexDataBegin)));
	memcpy(vertexDataBegin, triangleVertices, vertexBufferSize);
	vertex_buffer->Unmap(0, nullptr);

	vertex_buffer_view.BufferLocation = vertex_buffer->GetGPUVirtualAddress();
	vertex_buffer_view.StrideInBytes = sizeof(ColorVertex);
	vertex_buffer_view.SizeInBytes = vertexBufferSize;

	// Create synchronization objects
	ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
	fence_value = 1;
	fence_event = CreateEvent(nullptr, false, false, nullptr);
	if (fence_event == nullptr) {
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}
}

void Renderer::PopulateCommandList() {
	// Reset allocators and lists
	ThrowIfFailed(command_allocator->Reset());
	ThrowIfFailed(command_list->Reset(command_allocator.Get(), pipeline_state.Get()));

	// Set initial state
	command_list->SetGraphicsRootSignature(root_signature.Get());
	command_list->RSSetViewports(1, &view_port);
	command_list->RSSetScissorRects(1, &scissor_rect);

	// Resource barrier from present to RT
	command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		render_targets[frame_index].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET
		));

	// Record commands
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtv_heap->GetCPUDescriptorHandleForHeapStart(), frame_index, rtv_descriptor_size);
	command_list->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
	const float clearColor[] = {1.0f, 0.0f, 0.0f, 1.0f};
	command_list->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	command_list->IASetVertexBuffers(0, 1, &vertex_buffer_view);
	command_list->DrawInstanced(3, 1, 0, 0);

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
	const UINT64 prevFenceVal = fence_value;
	ThrowIfFailed(command_queue->Signal(fence.Get(), prevFenceVal));
	fence_value++;

	if (fence->GetCompletedValue() < prevFenceVal) {
		ThrowIfFailed(fence->SetEventOnCompletion(prevFenceVal, fence_event));
		WaitForSingleObject(fence_event, INFINITE);
	}

	frame_index = swap_chain->GetCurrentBackBufferIndex();
}

std::wstring Renderer::GetBinPath(std::wstring shader_file) const {
	WCHAR buffer[MAX_PATH];
	GetModuleFileName(nullptr, buffer, MAX_PATH);
	std::wstring modulePath = buffer;
	std::wstring::size_type pos = modulePath.find_last_of(L"\\/");

	return modulePath.substr(0, pos + 1) + shader_file;
}
