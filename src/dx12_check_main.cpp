
#include "dx12_labs.h"


bool IsAdapterSupportsDX12(IDXGIAdapter1* pAdapter)
{
	// There is no mistake with MinimumFeatureLevel param
	return SUCCEEDED(D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr));
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
{
	try
	{
		ComPtr<IDXGIFactory4> factory;
		DX::ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));

		ComPtr<IDXGIAdapter1> adapter;
		UINT adapter_index = 0;

		while (DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapter_index, &adapter))
		{
			DXGI_ADAPTER_DESC1 adapter_descriptor;
			adapter->GetDesc1(&adapter_descriptor);

			std::wcout << "Adapter #" << adapter_index + 1 << " " << adapter_descriptor.Description;

			if (IsAdapterSupportsDX12(adapter.Get()))
			{
				std::wcout << " supports DX12 ";
			}
			else
			{
				std::wcout << " doesn't support DX12 ";
			}

			adapter_index++;
		}
	}
	catch (com_exception e)
	{
		std::wcout << L"Exception:";
		std::wcout << e.get_wstring();
	}


	return 0;
}