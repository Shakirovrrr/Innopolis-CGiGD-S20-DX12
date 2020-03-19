#pragma once

#include "renderer.h"

class Renderer;

class Win32Window
{
public:
	static int Run(Renderer* pRenderer, HINSTANCE hInstance, int nCmdShow);
	static HWND GetHwnd() { return hwnd; }

protected:
	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
	static HWND hwnd;
};

