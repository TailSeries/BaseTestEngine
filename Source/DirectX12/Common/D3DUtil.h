#pragma once
#include <windows.h>
#include <wrl.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <string>
#include <unordered_map>

#include "d3dx12.h"
#include "../../Core/Base/GenericPlatform.h"

#pragma region DXDebug
inline void d3dSetDebugName(IDXGIObject* obj, const char* name)
{
    if (obj)
    {
        obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
    }
}
inline void d3dSetDebugName(ID3D12Device* obj, const char* name)
{
    if (obj)
    {
        obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
    }
}
inline void d3dSetDebugName(ID3D12DeviceChild* obj, const char* name)
{
    if (obj)
    {
        obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
    }
}

inline std::string AnsiTostring(const std::string& str)
{
    return str;
    //CHAR buffer[512];
    //MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    //return std::string(buffer);
}

class DxException
{
public:
    DxException() = default;
    DxException(HRESULT hr, const std::string& functionName, const std::string& filename, int lineNumber);

    std::string ToString()const;

    HRESULT ErrorCode = S_OK;
    std::string FunctionName;
    std::string Filename;
    int LineNumber = -1;
};


#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::string wfn = AnsiTostring(__FILE__);                       \
    if(FAILED(hr__)) { throw DxException(hr__, #x, wfn, __LINE__); } \
};
#endif



#ifndef ReleaseCom
#define ReleaseCom(x) { if(x){ x->Release(); x = 0; } }
#endif

#pragma endregion DXDebug

class D3DUtil
{
public:
    static std::wstring ToWString(const std::string& str);

	/**
     * 创建的时候就将初始化数据initdata放进默认堆中，然后返回默认堆，最后一个参数返回上传堆
     * @param device 
     * @param cmdList 
     * @param initData 
     * @param byteSize 
     * @param uploadBuffer 
     * @return 
     * 关键理解点

  1. “立即”复制到上传堆：是的，UpdateSubresources 内部会通过 Map/memcpy 将 initData 复制到上传堆（因为上传堆支持 CPU 写入）。
  2. “延迟”复制到默认堆：复制到默认堆的命令只是被记录到 cmdList 中，需要调用 ExecuteCommandLists 后 GPU 才会执行。
  3. 为什么这样设计：
    - 批量提交命令，减少 CPU-GPU 同步开销。
    - 允许在单个命令列表中记录多个资源上传命令，一次性提交。
    - 符合 DirectX12 的显式控制设计哲学。
  4. 上传堆的生命周期：
    - 在 CreateDefaultBuffer 中，上传堆通过 uploadBuffer 参数返回。
    - 在 MeshGeometry 中保存为 VertexBufferUploader/IndexBufferUploader。
    - 复制完成后不能立即释放，因为 GPU 可能还在读取（需要同步）。
    - 通常在确定 GPU 使用完成后才释放（如帧结束后）。

  所以，你的理解是对的：数据“立即”进入了上传堆（CPU 可访问），但进入默认堆（GPU 专用）是延迟的，需要命令列表执行后才会发生。
     */
    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const void* initData, uint64 byteSize, Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);


    static UINT CalcConstantBufferByteSize(UINT byteSize)
    {
        return (byteSize + 255) & ~255;
    }

    static  Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(const std::string& filename, const D3D_SHADER_MACRO* defines, const std::string& entrypoint, const std::string& target);

};



/*
 * 将网格数据进行封装
 * SubMeshGeometry 索引数量，起始索引位置，起始顶点位置
 * MeshGeometry：持有gpu资源
 */

struct SubMeshGeometry
{
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;//Index Buffer 中的偏移位置
    INT BaseVertexLocation = 0;//vertex Buffer中的偏移位置
    DirectX::BoundingBox Bounds; // 用于后续的视椎剔除
};

struct MeshGeometry
{
    std::string name;

    /*
     * CPU侧的顶点和索引数据
     * ID3DBlob dx提供的一个包装了对内存块的引用的工具接口，内部以malloc和free管理内存
     * dx不少跟cpu侧数据打交道的接口用的就是ID3DBlob
     */
    Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

    /*
     * GPU 侧的顶点和索引数据（default）
     */
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;
	/*
     * GPU 侧的顶点和索引数据（uploader）
     */
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

    //缓冲区的关键信息
    UINT VertexByteStride = 0;//每个顶点的数据大小
    UINT VertexBufferByteSize = 0;//顶点缓冲总字节数
    DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;//16位索引，总索引数65535
    UINT IndexBufferByteSize = 0;//索引缓冲总字节数


    //渲染时通过名字查找子网格然后DrawIndexInstanced
    std::unordered_map<std::string, SubMeshGeometry> DrawArgs;

    //获取VertexBuferView
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const;
    //获取IndexBufferView
    D3D12_INDEX_BUFFER_VIEW IndexBufferView() const;

    //在上传数据完成并执行 CopyBufferRegion() 或 UpdateSubresources() 后，
    //上传器就可以释放，以节省内存。(其实只是释放我们这一侧的Com指针罢了
    void DisposeUploaders();

};


