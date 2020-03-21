#include "win32_window.h"

HWND Win32Window::hwnd = nullptr;

int Win32Window::Run(Renderer *pRenderer, HINSTANCE hInstance, int nCmdShow) {
	// Initialize the window class.
	WNDCLASSEX windowClass = {};
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = hInstance;
	windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	windowClass.lpszClassName = L"DXExample";

	RegisterClassEx(&windowClass);

	// Create the window and store a handle to it.
	RECT windowRect = {0, 0, static_cast<long>(pRenderer->GetWidth()), static_cast<long>(pRenderer->GetHeight())};
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, false);
	hwnd = CreateWindow(
		windowClass.lpszClassName,
		pRenderer->GetTitle(),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		nullptr, nullptr, hInstance, pRenderer
		);

	// Initialize the sample. OnInit is defined in each child-implementation of DXSample.
	pRenderer->OnInit();
	ShowWindow(hwnd, nCmdShow);

	// Main sample loop.
	MSG msg = {};
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, hwnd, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	pRenderer->OnDestroy();

	// Return this part of the WM_QUIT message to Windows.
	return static_cast<int>(msg.wParam);
}

LRESULT Win32Window::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	Renderer *pRender = reinterpret_cast<Renderer *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	switch (message) {
		case WM_CREATE:
			// Save the Renderer* passed in to CreateWindow.
			LPCREATESTRUCT pCreateStruct;
			pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
			SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
			return 0;

		case WM_PAINT:
			if (pRender) {
				pRender->OnUpdate();
				pRender->OnRender();
			}
			return 0;

		case WM_KEYDOWN:
			if (pRender) {
				pRender->OnKeyDown(static_cast<UINT8>(wParam));
			}
			return 0;

		case WM_KEYUP:
			if (pRender) {
				pRender->OnKeyUp(static_cast<UINT8>(wParam));
			}
			return 0;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
	}

	// Handle any messages the switch statement didn't.
	return DefWindowProc(hWnd, message, wParam, lParam);
}
