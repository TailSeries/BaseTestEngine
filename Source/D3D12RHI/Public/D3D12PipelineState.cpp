#include "D3D12PipelineState.h"
#include "D3D12Device.h"

FD3D12PipelineState::FD3D12PipelineState(FD3D12Device* InDevice, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& Desc)
{
	ID3D12Device* D3DDevice = InDevice->GetDevice();
	VERIFY_D3D12(D3DDevice->CreateGraphicsPipelineState(&Desc, IID_PPV_ARGS(&PSO)));
}

FD3D12PipelineState::~FD3D12PipelineState() = default;

/*
 * ---
 *三个要点
 *
 *- PSO 烘死状态:一旦创建,里面的状态不可变。换光栅/混合状态 = 换一个 PSO。这是 D3D12 相比 D3D11 的核心变化——把零散的 state 调用合成一个对象,消除运行时状态校验开销。
 *- RTVFormats 必须和渲染目标真实格式一致:我们 swap chain 是 R8G8B8A8_UNORM,这里也得填它,否则绘制时格式不匹配报错。
 *- 深度先关:三角形不需要深度测试,DepthEnable=FALSE + DSVFormat=UNKNOWN。等做 3D 场景(A5)才建深度缓冲(DSV)、打开深度。
 *- UE 对照:UE 的 PSO 有缓存(FD3D12PipelineStateCache)+ 异步编译(PSO 创建慢,UE 后台预编译避免卡顿)。我们一个 PSO 直接同步建,学习够用。
 *
 *---
 */

