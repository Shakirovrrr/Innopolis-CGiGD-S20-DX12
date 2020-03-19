#include "win32_window.h"

HWND Win32Window::hwnd = nullptr;

int Win32Window::Run(Renderer* pRenderer, HINSTANCE hInstance, int nCmdShow)
{
	// Initialize the window class.

	// Create the window and store a handle to it.

	// Initialize the sample. OnInit is defined in each child-implementation of DXSample.

	// Main sample loop.

	// Return this part of the WM_QUIT message to Windows.
}

LRESULT Win32Window::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	Renderer* pRender = reinterpret_cast<Renderer*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	switch (message)
	{
	case WM_CREATE:
	{
		// Save the Renderer* passed in to CreateWindow.
		LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
	}
	return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	// Handle any messages the switch statement didn't.
	return DefWindowProc(hWnd, message, wParam, lParam);
}
