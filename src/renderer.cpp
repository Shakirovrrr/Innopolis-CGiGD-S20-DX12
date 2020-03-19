#include "renderer.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

void Renderer::OnInit()
{
	LoadPipeline();
	LoadAssets();
}

void Renderer::OnUpdate()
{
}

void Renderer::OnRender()
{
}

void Renderer::OnDestroy()
{
}

void Renderer::LoadPipeline()
{
	// Create debug layer


	// Create device


	// Create a direct command queue


	// Create swap chain


	// Create descriptor heap for render target view

	// Create render target view for each frame


	// Create command allocator
}

void Renderer::LoadAssets()
{
	// Create a root signature


	// Create full PSO

	// Create command list

	// Create and upload vertex buffer

	// Create synchronization objects

}

void Renderer::PopulateCommandList()
{
	// Reset allocators and lists

	// Set initial state

	// Resource barrier from present to RT

	// Record commands

	// Resource barrier from RT to present

	// Close command list
}

void Renderer::WaitForPreviousFrame()
{
	// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
	// Signal and increment the fence value.

}

std::wstring Renderer::GetBinPath(std::wstring shader_file) const
{
}
