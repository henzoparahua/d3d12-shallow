#include "App.h"
#include <d3d12.h>
#include <wrl.h>
#include "GraphicsError.h"

namespace chil::app
{
	int Run(win::IWindow& window)
	{
		using namespace Microsoft::WRL;

		ComPtr<ID3D12Device> device;
		D3D12CreateDevice(
			nullptr,
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&device)
		) >> chk;

		return 0;
	}

}