#include "App.h"
#include "GraphicsError.h"

#include <d3d12.h>
#include "d3dx12.h"
#include <dxgi1_6.h>
#include <wrl.h>


namespace chil::app
{
	using namespace Microsoft::WRL;

	int Run(win::IWindow& window)
	{
		// Set up the graphics environment
		constexpr UINT width = 1280;
		constexpr UINT height = 720;
		constexpr UINT bufferCount = 2;

		// dxgi factory
		ComPtr<IDXGIFactory6> factory;
		CreateDXGIFactory2(
			DXGI_CREATE_FACTORY_DEBUG,
			IID_PPV_ARGS(&factory)
		) >> chk;

		// device
		ComPtr<ID3D12Device> device;
		D3D12CreateDevice(
			nullptr,
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&device)
		) >> chk;

		// command queue
		ComPtr<ID3D12CommandQueue> commandQueue;
		{
			const D3D12_COMMAND_QUEUE_DESC queueDesc = {
				.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
				.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
				.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
				.NodeMask = 0
			};
			device->CreateCommandQueue(
				&queueDesc,
				IID_PPV_ARGS(&commandQueue)
			) >> chk;
		}

		// swap chain
		ComPtr<IDXGISwapChain4> swapChain;
		{
			const DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {
				.Width = width,
				.Height = height,
				.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
				.Stereo = FALSE,
				.SampleDesc = {
					.Count = 1,
					.Quality = 0
				},
				.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
				.BufferCount = bufferCount,
				.Scaling = DXGI_SCALING_STRETCH,
				.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
				.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
				.Flags = 0,
			};
			ComPtr<IDXGISwapChain1> swapChain1;
			factory->CreateSwapChainForHwnd(
				commandQueue.Get(),
				window.GetHandle(),
				&swapChainDesc,
				nullptr,
				nullptr,
				&swapChain1
			) >> chk;
			swapChain1.As(&swapChain) >> chk;
		}

		// depth buffer
		ComPtr<ID3D12Resource> depthBuffer;
		{
			const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
			const D3D12_RESOURCE_DESC depthBufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				DXGI_FORMAT_D24_UNORM_S8_UINT,
				width,
				height,
				1, // array size
				1, // mip levels
				1, // sample count
				0, // sample quality
				D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
			const D3D12_CLEAR_VALUE clearValue = {
				.Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
				.DepthStencil = { 1.0f, 0 }
			};
			device->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_NONE,
				&depthBufferDesc,
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				&clearValue,
				IID_PPV_ARGS(&depthBuffer)
			) >> chk;
		}

		// render target view	
		
		// descriptor heap
		ComPtr<ID3D12DescriptorHeap> rtvHeap;
		{
			const D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {
				.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
				.NumDescriptors = bufferCount,
			};
			device->CreateDescriptorHeap(
				&rtvHeapDesc,
				IID_PPV_ARGS(&rtvHeap)
			) >> chk;
		}
		const auto rtvHeapSize = device->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// rtv descriptors and buffer references
		ComPtr<ID3D12Resource> backBuffers[bufferCount];
		{
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
				rtvHeap->GetCPUDescriptorHandleForHeapStart());

			for (int i = 0; i < bufferCount; ++i) 
			{
				swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i])) >> chk;
				device->CreateRenderTargetView(
					backBuffers[i].Get(),
					nullptr,
					rtvHandle
				);
				rtvHandle.Offset(rtvHeapSize);
			}
		}
		
		// command allocator
		ComPtr<ID3D12CommandAllocator> commandAllocator;
		device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&commandAllocator)
		) >> chk;

		// command list
		ComPtr<ID3D12GraphicsCommandList> commandList;
		device->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			commandAllocator.Get(),
			nullptr,
			IID_PPV_ARGS(&commandList)
		) >> chk;
		commandList->Close() >> chk;

		// fence
		ComPtr<ID3D12Fence> fence;
		UINT64 fenceValue = 0;
		device->CreateFence(
			fenceValue,
			D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(&fence)
		) >> chk;

		// fence signalling event
		HANDLE fenceEvent = CreateEvent(
			nullptr,
			FALSE,
			FALSE,
			nullptr
		);
		if (!fenceEvent) 
		{
			GetLastError() >> chk;
			throw std::runtime_error("Failed to create fence event");
		}

		// main render loop
		UINT currentBackBufferIndex;
		while (!window.IsClosing())
		{
			// advance back buffer
			currentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();

			// select currrent buffer to render to
			auto& backBuffer = backBuffers[currentBackBufferIndex];

			// reset command allocator and command list
			commandAllocator->Reset() >> chk;
			commandList->Reset(commandAllocator.Get(), nullptr) >> chk;

			// clear the Render Target
			{
				//Transition the back buffer to render target state
				const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
					backBuffer.Get(),
					D3D12_RESOURCE_STATE_PRESENT,
					D3D12_RESOURCE_STATE_RENDER_TARGET
				);
				commandList->ResourceBarrier(1, &barrier);

				// clear the buffer
				FLOAT clearColor[] = { 0.0f, 0.17f, 0.20f, 1.0f };
				const CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
					rtvHeap->GetCPUDescriptorHandleForHeapStart(),
					(INT)currentBackBufferIndex,
					rtvHeapSize
				);
				commandList->ClearRenderTargetView(
					rtvHandle,
					clearColor,
					0,
					nullptr
				);
			}
			// prepare buffer for presentation
			{
				// transition the back buffer to present state
				const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
					backBuffer.Get(),
					D3D12_RESOURCE_STATE_RENDER_TARGET,
					D3D12_RESOURCE_STATE_PRESENT
				);
				commandList->ResourceBarrier(1, &barrier);
			}
			// submit the command list
			{
				// close the command list
				commandList->Close() >> chk;

				// execute the command list
				ID3D12CommandList* const commandLists[] = { commandList.Get() };
				commandQueue->ExecuteCommandLists(
					(UINT)std::size(commandLists),
					commandLists
				);
			}

			// insert fence to mark command list completion
			commandQueue->Signal(
				fence.Get(),
				fenceValue++
			) >> chk;

			// present frame
			swapChain->Present(0, 0) >> chk;

			// wait for the command list to be free
			fence->SetEventOnCompletion(
				fenceValue - 1,
				fenceEvent
			) >> chk;
			if (::WaitForSingleObject(fenceEvent, INFINITE) == WAIT_FAILED)
			{
				GetLastError() >> chk;
			}

		}

		return 0;
	}

}