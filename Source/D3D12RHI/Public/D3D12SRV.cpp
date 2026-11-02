#include "D3D12DynamicRHI.h"
#include "D3D12Device.h"
#include "D3D12View.h"

#include <memory>
#include <stdexcept>
#include <utility>
TRefCountPtr<FRHIShaderResourceView> FD3D12DynamicRHI::RHICreateShaderResourceView(TRefCountPtr<FRHIViewableResource> Resource, const FRHIViewDesc& ViewDesc)
{
	if (!GetDevice())
	{
		throw std::logic_error("RHI device has not been initialized");
	}

	TRefCountPtr<FD3D12ShaderResourceView_RHI> View =
		std::make_shared<FD3D12ShaderResourceView_RHI>(
			GetDevice(),
			GetDevice()->GetResourceDescriptorHeap(),
			std::move(Resource),
			ViewDesc);

	View->CreateView();

	return View;
}
