#include "D3D12Shader.h"
#include <cstring>
#include <cassert>

ComPtr<ID3DBlob> CompileShader(const char* source, const char* EntryPoint, const char* Target)
{
	UINT Flags = 0;
#if defined(_DEBUG)
	Flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;   // Debug 下带调试信息、不优化
#endif

	ComPtr<ID3DBlob> Code;
	ComPtr<ID3DBlob> Error;
	HRESULT hr = D3DCompile(source,  // 源码 + 长度
		strlen(source), 
		nullptr, //源文件名（调试用，可 nullptr）
		nullptr,// 宏定义 / include 处理器（都不用）
		nullptr,
		EntryPoint, // 入口函数 / 目标 profile（vs_5_0 等）
		Target,
		Flags,
		0,
		&Code,
		&Error);
	if (FAILED(hr))
	{
		if (Error)
		{
			OutputDebugStringA((const char*)Error->GetBufferPointer());  // 编译错误打到调试输出
		}
		assert(false && "Shader compile failed");
		return nullptr;
	}
	return Code;
}

/*
 * --
说明
- D3DCompile 参数:源码指针+长度、入口函数名(VSMain/PSMain)、目标 profile(vs_5_0/ps_5_0)、输出字节码 blob + 错误 blob。
- 错误处理:编译失败时 Errors blob 里是人类可读的错误信息(哪行哪列语法错),打到调试输出——写 shader 时全靠它定位。
- vs_5_0 / ps_5_0:Shader Model 5.0,D3D12 通用兼容。想用 SM6.0+(wave ops 等)得换 DXC 编译器,D3DCompile 只到 5.x,学习够用。
- 返回 ComPtr<ID3DBlob>:字节码存在 blob 里,后面 PSO 的 VS/PS 字段会指向 blob->GetBufferPointer() + GetBufferSize()。
- UE 对照:UE 从不运行时编译(太慢),它离线把所有 shader 变体编好打包。我们暂时单个三角形运行时编一次无所谓,属"学习阶段简化"。
---
 */
