#pragma once

#include "dx12_labs.h"
#include "win32_window.h"


class Renderer {
public:
	Renderer(UINT width, UINT height) : width(width), height(height), title(L"DX12 renderer"), frame_index(0), rtv_descriptor_size(0) {
		view_port = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
		scissor_rect = CD3DX12_RECT(0, 0, static_cast<LONG>(width), static_cast<LONG>(height));
		vertex_buffer_view = {};
		fence_value = 0;
		fence_event = nullptr;
		aspectRatio = static_cast<float>(width) / static_cast<float>(height);
		colorVertices.clear();

		deltaRotation = 0.0f;
		deltaForward = 0.0f;
		deltaZ = 0.0f;
		deltaA = 0.0f;
		angle = 0.0f;

		worldViewProj = XMMatrixIdentity();
		world = XMMatrixTranslation(0, 0, 0) * XMMatrixScaling(1, 1, 1);
		view = XMMatrixIdentity();
		eyePos = XMVECTOR({0, 1, -5});
		projection = XMMatrixPerspectiveFovLH(60.0f / 180.0f * XM_PI, aspectRatio, 0.001f, 100.0f);
		upDir = {0.0f, 1.0f, 0.0f};
		lookAt = eyePos + XMVECTOR({sinf(angle), 0.0f, cosf(angle)});
	};
	virtual ~Renderer() {};

	virtual void OnInit();
	virtual void OnUpdate();
	virtual void OnRender();
	virtual void OnDestroy();

	virtual void OnKeyDown(UINT8 key);
	virtual void OnKeyUp(UINT8 key);

	UINT GetWidth() const { return width; }
	UINT GetHeight() const { return height; }
	const WCHAR *GetTitle() const { return title.c_str(); }

protected:
	UINT width;
	UINT height;
	std::wstring title;

	XMVECTOR eyePos;
	float deltaRotation, deltaForward, deltaZ, deltaA;

	static const UINT frame_number = 2;

	// Pipeline objects.
	ComPtr<ID3D12Device> device;
	ComPtr<ID3D12CommandQueue> command_queue;
	ComPtr<IDXGISwapChain3> swap_chain;
	ComPtr<ID3D12DescriptorHeap> rtv_heap;
	UINT rtv_descriptor_size;
	ComPtr<ID3D12Resource> render_targets[frame_number];
	ComPtr<ID3D12CommandAllocator> command_allocator;
	ComPtr<ID3D12PipelineState> pipeline_state;
	ComPtr<ID3D12GraphicsCommandList> command_list;

	ComPtr<ID3D12RootSignature> root_signature;
	CD3DX12_VIEWPORT view_port;
	CD3DX12_RECT scissor_rect;

	// Resources
	ComPtr<ID3D12Resource> vertex_buffer;
	D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view;
	std::vector<ColorVertex> colorVertices;

	XMMATRIX worldViewProj;
	XMMATRIX projection, view, world;
	XMVECTOR upDir, lookAt;

	ComPtr<ID3D12Resource> constantBuffer;
	ComPtr<ID3D12DescriptorHeap> cbvHeap;
	UINT8 *cbvDataBegin;

	// Synchronization objects.
	UINT frame_index;
	HANDLE fence_event;
	ComPtr<ID3D12Fence> fence;
	UINT64 fence_value;

	float aspectRatio;
	float angle;

	void LoadPipeline();
	void LoadAssets();
	void PopulateCommandList();
	void WaitForPreviousFrame();
	std::wstring GetBinPath(std::wstring shader_file) const;
};