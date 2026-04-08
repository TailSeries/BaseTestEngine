#include "BoxApp.h"
#ifdef  CHAPTER_6


//顶点信息中定义一个位置 + 颜色
struct Vertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT4 Color;
};

// 存放世界矩阵
struct ObjectConstants
{
    DirectX::XMFLOAT4 GWorldViewProj;
};


#endif
void BoxApp::FillInputLayout()
{

	//我们的顶点只有一个 struct Vertex 	D3D12_INPUT_ELEMENT_DESC
	/*struct D3D12_INPUT_ELEMENT_DESC
    {
    LPCSTR SemanticName; // 语义名称
    UINT SemanticIndex; // 同语义的不同索引下标
    DXGI_FORMAT Format; // 该成员的数据格式
    UINT InputSlot; //输入槽 0 ~ 15
    UINT AlignedByteOffset; //在buffer结构中的偏移位置
    D3D12_INPUT_CLASSIFICATION InputSlotClass; // PER_VERTEX_DATA PER_INSTANCE_DATA是instance技术用的
    UINT InstanceDataStepRate;// 目前为0, 要用instance这里写1
    }
	 * 
	 */
    InputLayout.push_back({"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, Pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
    InputLayout.push_back({"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, Color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
}
