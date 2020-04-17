#include "pch.h"
#include "renderer.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

void Renderer::OnInit() {
	LoadPipeline();
	LoadAssets();
}

void Renderer::OnUpdate() {
	angle += deltaRotation;
	eyePos += XMVECTOR({sinf(angle), 0.0f, cosf(angle)}) * deltaForward;

	lookAt = eyePos + XMVECTOR({sinf(angle), 0.0f, cosf(angle)});
	view = XMMatrixLookAtLH(eyePos, lookAt, upDir);

	worldViewProj = projection * view * world;
	worldViewProj = XMMatrixTranspose(
		XMMatrixTranspose(projection) *
		XMMatrixTranspose(view) *
		XMMatrixTranspose(world)
	);

	memcpy(cbvDataBegin, &worldViewProj, sizeof(worldViewProj));
}

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

void Renderer::OnKeyDown(UINT8 key) {
	switch (key) {
		case 0x41 - 'a' + 'd':
			deltaRotation = 0.001f;
			break;
		case 0x41 - 'a' + 'a':
			deltaRotation = -0.001f;
			break;
		case 0x41 - 'a' + 'w':
			deltaForward = 0.001f;
			break;
		case 0x41 - 'a' + 's':
			deltaForward = -0.001f;
			break;
		default:
			break;
	}
}

void Renderer::OnKeyUp(UINT8 key) {
	switch (key) {
		case 0x41 - 'a' + 'd':
			deltaRotation = 0.0f;
			break;
		case 0x41 - 'a' + 'a':
			deltaRotation = 0.0f;
			break;
		case 0x41 - 'a' + 'w':
			deltaForward = 0.0f;
			break;
		case 0x41 - 'a' + 's':
			deltaForward = 0.0f;
			break;
		default:
			break;
	}
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

	// Create constant buffer
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDescriptor = {};
	cbvHeapDescriptor.NumDescriptors = 1;
	cbvHeapDescriptor.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDescriptor.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(device->CreateDescriptorHeap(&cbvHeapDescriptor, IID_PPV_ARGS(&cbvHeap)));

	// Create render target view for each frame
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtv_heap->GetCPUDescriptorHandleForHeapStart());
	for (unsigned int i = 0; i < frame_number; i++) {
		ThrowIfFailed(swap_chain->GetBuffer(i, IID_PPV_ARGS(&render_targets[i])));
		device->CreateRenderTargetView(render_targets[i].Get(), nullptr, rtvHandle);
		std::wstring rtName = L"Render target #";
		rtName += std::to_wstring(i);
		OutputDebugString(rtName.c_str());
		render_targets[i]->SetName(L"Render target");
		rtvHandle.Offset(1, rtv_descriptor_size);
	}

	// Create command allocator
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator)));
}

void Renderer::LoadAssets() {
	// Create a root signature

	D3D12_FEATURE_DATA_ROOT_SIGNATURE rsFeatureData = {};
	rsFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rsFeatureData, sizeof(rsFeatureData)))) {
		rsFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
	CD3DX12_ROOT_PARAMETER1 rootParams[1];

	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	rootParams[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);

	D3D12_ROOT_SIGNATURE_FLAGS rsFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		| D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
		| D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
		| D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
		| D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescriptor;
	rootSignatureDescriptor.Init_1_1(_countof(rootParams), rootParams, 0, nullptr, rsFlags);

	/*CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDescriptor = {};
	rootSignatureDescriptor.Init(0, nullptr, 0, nullptr,
								 D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);*/

	ComPtr<ID3D10Blob> signature;
	ComPtr<ID3D10Blob> error;
	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDescriptor, rsFeatureData.HighestVersion, &signature, &error));
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
	psoDescriptor.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	psoDescriptor.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
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
	std::wstring objDir = GetBinPath(std::wstring());
	std::string objPath(objDir.begin(), objDir.end());
	std::string inputfile = objPath + "CornellBox-Original.obj";
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string warn;
	std::string err;

	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, inputfile.c_str());

	if (!warn.empty()) {
		std::wstring wwarn(warn.begin(), warn.end());
		wwarn = L"Tinyobjloader warning: " + wwarn + L'\n';
		OutputDebugString(wwarn.c_str());
	}

	if (!err.empty()) {
		std::wstring werr(err.begin(), err.end());
		werr = L"Tinyobjloader error: " + werr + L'\n';
		OutputDebugString(werr.c_str());
	}

	if (!ret) {
		ThrowIfFailed(-1);
	}

	// Loop over shapes
	for (size_t s = 0; s < shapes.size(); s++) {
		// Loop over faces(polygon)
		size_t index_offset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
			int fv = shapes[s].mesh.num_face_vertices[f];

			// per-face material
			int material_ids = shapes[s].mesh.material_ids[f];

			// Loop over vertices in the face.
			for (size_t v = 0; v < fv; v++) {
				// access to vertex
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
				tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
				tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
				tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
				materials[material_ids].diffuse;
				ColorVertex colorVertex = {
					{vx, vy, vz},
					{
					materials[material_ids].diffuse[0],
					materials[material_ids].diffuse[1],
					materials[material_ids].diffuse[2],
					1.0f
					}
				};
				colorVertices.push_back(colorVertex);
			}

			index_offset += fv;
		}
	}

	ColorVertex triangleVertices[] = {
		{{0.0f, 0.25f * aspectRatio, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
		{{0.25f, -0.25f * aspectRatio, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
		{{-0.25f, -0.25f * aspectRatio, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}
	};

	const UINT vertexBufferSize = sizeof(ColorVertex) * colorVertices.size();
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertex_buffer)
	));

	UINT8 *vertexDataBegin;
	CD3DX12_RANGE readRange(0, 0);
	ThrowIfFailed(vertex_buffer->Map(0, &readRange, reinterpret_cast<void **>(&vertexDataBegin)));
	memcpy(vertexDataBegin, colorVertices.data(), vertexBufferSize);
	vertex_buffer->Unmap(0, nullptr);

	vertex_buffer_view.BufferLocation = vertex_buffer->GetGPUVirtualAddress();
	vertex_buffer_view.StrideInBytes = sizeof(ColorVertex);
	vertex_buffer_view.SizeInBytes = vertexBufferSize;

	// Init constant buffer
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constantBuffer)
	));

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDescriptor = {};
	cbvDescriptor.BufferLocation = constantBuffer->GetGPUVirtualAddress();
	cbvDescriptor.SizeInBytes = (sizeof(worldViewProj) + 255) & ~255;
	device->CreateConstantBufferView(&cbvDescriptor, cbvHeap->GetCPUDescriptorHandleForHeapStart());

	ThrowIfFailed(constantBuffer->Map(0, &readRange, reinterpret_cast<void **>(&cbvDataBegin)));
	memcpy(cbvDataBegin, &worldViewProj, sizeof(worldViewProj));

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
	ID3D12DescriptorHeap *heaps[] = {cbvHeap.Get()};
	command_list->SetDescriptorHeaps(_countof(heaps), heaps);
	command_list->SetGraphicsRootDescriptorTable(0, cbvHeap->GetGPUDescriptorHandleForHeapStart());
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
	const float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
	command_list->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	command_list->IASetVertexBuffers(0, 1, &vertex_buffer_view);
	command_list->DrawInstanced(colorVertices.size(), 1, 0, 0);

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
