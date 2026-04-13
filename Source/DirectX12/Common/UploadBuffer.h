#pragma once
#include <d3d12.h>
#include <wrl/client.h>

#include "D3DUtil.h"
/*
 * 实现了一个 DirectX12 上传缓冲区的封装类，主要用于简化 CPU 到 GPU 的数据传输
 上传缓冲区（Upload Buffer） 是 DirectX12 中一种特殊的内存类型，位于 D3D12_HEAP_TYPE_UPLOAD 堆上，特点：
  - CPU 可写，GPU 可读                                                                                                                                                                                                                                    - 用于将数据从 CPU 传输到 GPU（如常量缓冲区、顶点数据等）
  - 创建后立即映射（Map），保持映射状态直到销毁
 */


template<typename T>
class UploadBuffer
{
public:
	UploadBuffer(ID3D12Device* device, UINT elementcount, bool isConstantBuffer)
		:MIsconstantBUffer(isConstantBuffer)
	{
		MElemantByteSize = sizeof(T);
		//常量缓冲区特殊处理：DirectX12 硬件要求常量缓冲区视图（CBV）的偏移和大小必须是 256 字节的倍数
		if (isConstantBuffer)
		{
			MElemantByteSize = D3DUtil::CalcConstantBufferByteSize(MElemantByteSize);
		}

		 CD3DX12_HEAP_PROPERTIES HeapProperties(D3D12_HEAP_TYPE_UPLOAD);
		 CD3DX12_RESOURCE_DESC ResourceSize = CD3DX12_RESOURCE_DESC::Buffer(MElemantByteSize * elementcount);
		ThrowIfFailed(device->CreateCommittedResource(
		&HeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&ResourceSize,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&MUploadBuffer)
		));

		/*
		 *  创建后立即映射，获取 CPU 可访问的指针 mMappedData
			- 注意：上传堆资源可以保持映射状态，不需要频繁 Map/Unmap
			- 析构时调用 Unmap 释放映射
		 */
		ThrowIfFailed(MUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&MMappedData)));
	}

	UploadBuffer(const UploadBuffer& rhs) = delete;
	UploadBuffer& operator=(const UploadBuffer& rhs) = delete;

	~UploadBuffer()
	{
		if (MUploadBuffer)
		{
			MUploadBuffer->Unmap(0, nullptr);
			MUploadBuffer = nullptr;
		}
	}

	ID3D12Resource* Resource() const
	{
		return MUploadBuffer.Get();
	}


	/*
	 * 将SourceData复制到第几个元素位置
	 * 注意这里是复制到map后的MMappedData，这是一个cpu可以访问的地址。
	 */
	void CopyData(int elementIndex, const T& SourceData)
	{
		memcpy(&MMappedData[elementIndex * MElemantByteSize], &SourceData, sizeof(T));
	}



private:
	Microsoft::WRL::ComPtr<ID3D12Resource> MUploadBuffer;
	BYTE* MMappedData = nullptr;

	UINT MElemantByteSize = 0;
	bool MIsconstantBUffer = false;
};
