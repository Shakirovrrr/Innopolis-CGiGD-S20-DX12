#pragma once

#ifndef UNICODE
#define UNICODE
#endif

#include <Windows.h>

#include <wrl.h>

#include <initguid.h>

#include <dxgi1_4.h>
#include "d3dx12.h"

#include <D3Dcompiler.h>
#include <DirectXMath.h>

#include <iostream>

#include <exception>

using namespace Microsoft::WRL;

namespace DX
{
	class com_exception : public std::exception
	{
	public:
		com_exception(HRESULT hr) : result(hr)
		{
			what_str = std::wstring(L"Failure with HRESULT of " +
				std::to_wstring(static_cast<unsigned int>(result)) +
				L"\n");
		}

		const LPCWSTR get_wstring() const
		{
			return what_str.c_str();
		}


	private:
		HRESULT result;
		std::wstring what_str;
	};

	inline void ThrowIfFailed(HRESULT hr)
	{
		if (FAILED(hr))
		{
			throw com_exception(hr);
		}
	}
}

using namespace DX;
using namespace DirectX;

struct ColorVertex
{
	XMFLOAT3 position;
	XMFLOAT4 color;
};
