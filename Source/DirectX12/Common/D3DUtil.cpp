#include "D3DUtil.h"
#include <comdef.h>

#include "Common/LogAssert.h"
#pragma region DXDebug
DxException::DxException(HRESULT hr, const std::string& functionName, const std::string& filename, int lineNumber) :
    ErrorCode(hr),
    FunctionName(functionName),
    Filename(filename),
    LineNumber(lineNumber)
{
    ErrorStringMsg(ToString().c_str());
}


std::string DxException::ToString()const
{
    // Get the string description of the error code.
    _com_error err(ErrorCode);
    std::string msg = err.ErrorMessage();

    return FunctionName + " failed in " + Filename + "; line " + std::to_string(LineNumber) +  "; error: " + msg;
}
#pragma endregion DXDebug

Microsoft::WRL::ComPtr<ID3D12Resource> D3DUtil::CreateDefaultBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const void* initData, uint64 byteSize, Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
{
    Microsoft::WRL::ComPtr<ID3D12Resource> DefaultBuffer;
    CD3DX12_HEAP_PROPERTIES DefaultHeapProperty(D3D12_HEAP_TYPE_DEFAULT);
    // 默认堆一般用来存顶点的buffer，这里我们不指定对的额外用途，用D3D12_RESOURCE_FLAG_NONE就行
    CD3DX12_RESOURCE_DESC DefaultBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize, D3D12_RESOURCE_FLAG_NONE);
    ThrowIfFailed(device->CreateCommittedResource(&DefaultHeapProperty, D3D12_HEAP_FLAG_NONE, &DefaultBufferDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&DefaultBuffer)));


    //微软限制，上传堆上的资源必须创建为 D3D12_RESOURCE_STATE_GENERIC_READ 的初始状态
    CD3DX12_HEAP_PROPERTIES UploadHeapProperty(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC UploadBUfferDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize, D3D12_RESOURCE_FLAG_NONE);
    ThrowIfFailed(device->CreateCommittedResource(&UploadHeapProperty, D3D12_HEAP_FLAG_NONE, &UploadBUfferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer)));


    //将数据从cpu侧拷贝到上传堆，然后拷贝到默认堆
    D3D12_SUBRESOURCE_DATA SubResourceData = {};
    SubResourceData.pData = initData;
    SubResourceData.RowPitch = byteSize; // 数据总长度
    SubResourceData.SlicePitch = SubResourceData.RowPitch; // 表示一个纹理行的内存跨度（以字节为单位）。它指的是在一幅纹理中，从一行的起始位置到下一行的起始位置之间的字节数。换句话说，它是纹理每行的“宽度”，但它不仅仅是像素的数量，而是考虑了对齐、格式和其他因素后的字节总数。
    CD3DX12_RESOURCE_BARRIER DefaultBufferCommonToCopyDestBaarrier = CD3DX12_RESOURCE_BARRIER::Transition(DefaultBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    cmdList->ResourceBarrier(1, &DefaultBufferCommonToCopyDestBaarrier);
    UpdateSubresources<1>(cmdList, DefaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &SubResourceData);
    CD3DX12_RESOURCE_BARRIER DefaultBufferCopyDestBaarrierToCommon = CD3DX12_RESOURCE_BARRIER::Transition(DefaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
    cmdList->ResourceBarrier(1, &DefaultBufferCopyDestBaarrierToCommon);
	return DefaultBuffer;
}

D3D12_VERTEX_BUFFER_VIEW MeshGeometry::VertexBufferView() const
{
    //顶点缓冲区视图不需要描述符堆
    /*BufferLocation GPU 虚拟地址，指向实际的 vertex buffer 数据（显存中的地址）。通常由调用 ID3D12Resource::GetGPUVirtualAddress() 获得
     * StrideInBytes 每个顶点的字节大小（即一个顶点结构体的大小）。
     * SizeInBytes 顶点缓冲区的 总字节大小。GPU 读取时不会超过这个范围。
     */
    D3D12_VERTEX_BUFFER_VIEW vbv;
    vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
    vbv.StrideInBytes = VertexByteStride;
    vbv.SizeInBytes = VertexBufferByteSize;
    return vbv;
}

D3D12_INDEX_BUFFER_VIEW MeshGeometry::IndexBufferView() const
{
    //顶点缓冲区也不需要描述符堆
     /*BufferLocation GPU 虚拟地址，指向实际的 vertex buffer 数据（显存中的地址）。通常由调用 ID3D12Resource::GetGPUVirtualAddress() 获得
     * Format 数据格式，必须是以下两种之一：DXGI_FORMAT_R16_UINT DXGI_FORMAT_R32_UINT
     * SizeInBytes 顶点缓冲区的 总字节大小。GPU 读取时不会超过这个范围。
     * 为什么 索引缓冲区视图不需要 StrideInBytes 这个成员？Format 指定的就是索引缓冲区的数据大小。
     */
    D3D12_INDEX_BUFFER_VIEW Ibv;
    Ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
    Ibv.Format = IndexFormat;
    Ibv.SizeInBytes = IndexBufferByteSize;
    return Ibv;
}

void MeshGeometry::DisposeUploaders()
{
    VertexBufferUploader = nullptr;
    IndexBufferUploader = nullptr;
}
