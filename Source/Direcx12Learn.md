# 《DirectX 12 3D 游戏开发实战》学习笔记

> 学习笔记按章节序号编排，持续补充中。
> 源码仓库：`F:\workspace\BaseTestEngineDocs\d3d12book`
> 作者：Frank Luna

---

## 📚 全书目录

| 章节 | 标题 | 状态 |
|------|------|------|
| 第 1 章 | 矢量代数 | ⬜ 待补充 |
| 第 2 章 | 矩阵代数 | ⬜ 待补充 |
| 第 3 章 | 变换 | ⬜ 待补充 |
| 第 4 章 | Direct3D 初始化 | ⬜ 待补充 |
| 第 5 章 | 渲染流水线 | ⬜ 待补充 |
| 第 6 章 | 在 Direct3D 中绘制（Part I） | ⬜ 待补充 |
| [第 7 章](#ch7) | 在 Direct3D 中绘制（Part II） | ✅ 已完成 |
| 第 8 章 | 光照 | ⬜ 待补充 |
| 第 9 章 | 纹理 | ⬜ 待补充 |
| 第 10 章 | 混合 | ⬜ 待补充 |
| 第 11 章 | 模板 | ⬜ 待补充 |
| 第 12 章 | 几何着色器 | ⬜ 待补充 |
| 第 13 章 | 计算着色器 | ⬜ 待补充 |
| 第 14 章 | 曲面细分 | ⬜ 待补充 |
| …… | 后续章节持续补充 | ⬜ |

> 图例：✅ 已完成 / 🟡 进行中 / ⬜ 待补充

---

<a id="ch7"></a>
# 第 7 章 — 在 Direct3D 中绘制（Part II）

> 源码位置：`F:\workspace\BaseTestEngineDocs\d3d12book\Chapter 7 Drawing in Direct3D Part II`
> 本章核心主题：**帧资源（Frame Resources）、常量缓冲区（Constant Buffers）、根签名与描述符表、渲染项（Render Items）组织、CPU/GPU 同步，以及动态顶点缓冲（水波模拟）**。

## 7.0 本章目录

1. [章节概览与两个示例项目](#71-章节概览与两个示例项目)
2. [公共基础框架（Common）](#72-公共基础框架common)
3. [核心概念一：帧资源 FrameResource](#73-核心概念一帧资源-frameresource)
4. [核心概念二：渲染项 RenderItem 与脏标记](#74-核心概念二渲染项-renderitem-与脏标记)
5. [核心概念三：常量缓冲区结构](#75-核心概念三常量缓冲区结构)
6. [核心概念四：根签名、描述符堆与 CBV](#76-核心概念四根签名描述符堆与-cbv)
7. [示例一：Shapes（多形状静态场景）](#77-示例一shapes多形状静态场景)
8. [示例二：LandAndWaves（地形 + 水波动态场景）](#78-示例二landandwaves地形--水波动态场景)
9. [着色器 color.hlsl](#79-着色器-colorhlsl)
10. [CPU/GPU 同步与帧循环机制](#710-cpugpu-同步与帧循环机制)
11. [Shapes vs LandAndWaves 对比](#711-shapes-vs-landandwaves-对比)
12. [关键知识点总结](#712-关键知识点总结)

---

## 7.1 章节概览与两个示例项目

第 7 章在第 6 章（绘制单个 Box）的基础上，扩展为**组织多个绘制对象、高效的常量缓冲区管理、以及动态几何体更新**。本章包含两个循序渐进的示例：

| 项目 | 路径 | 内容 |
|------|------|------|
| **Shapes** | `Shapes/` | 用描述符表 + 描述符堆管理 CBV；绘制盒子、网格、球体、圆柱等静态几何体，演示多渲染项组织 |
| **LandAndWaves** | `LandAndWaves/` | 用根描述符（root descriptor）简化 CBV 绑定；绘制程序化地形 + 实时水波模拟，演示动态顶点缓冲 |

两个示例共享同一套设计思想（FrameResource、RenderItem、PassCB/ObjectCB 分离），区别在于**常量缓冲区的绑定方式**（描述符表 vs 根描述符）以及**是否有动态几何体**。

### 文件结构

```
Chapter 7 Drawing in Direct3D Part II/
├── Shapes/
│   ├── ShapesApp.cpp          # 主程序（806 行）
│   ├── FrameResource.h/.cpp   # 帧资源（PassCB + ObjectCB）
│   └── Shaders/color.hlsl     # 顶点+像素着色器
└── LandAndWaves/
    ├── LandAndWavesApp.cpp    # 主程序（756 行）
    ├── FrameResource.h/.cpp   # 帧资源（PassCB + ObjectCB + WavesVB）
    ├── Waves.h/.cpp           # 水波模拟类（CPU 端物理计算）
    └── Shaders/color.hlsl     # 着色器（与 Shapes 相同）
```

---

## 7.2 公共基础框架（Common）

两个项目都通过相对路径 `../../Common/` 引用公共代码。这些是理解示例的前提：

### 7.2.1 `D3DApp` 基类（`d3dApp.h/.cpp`）

所有示例 App 的基类，封装了 Direct3D 应用的样板代码。`ShapesApp` / `LandAndWavesApp` 均继承自它。

关键成员（派生类直接使用）：
- **设备与交换链**：`md3dDevice`、`mSwapChain`、双缓冲 `mSwapChainBuffer[2]`、`mDepthStencilBuffer`
- **命令对象**：`mCommandQueue`、`mDirectCmdListAlloc`（初始化用分配器）、`mCommandList`
- **围栏同步**：`mFence`、`mCurrentFence`、`FlushCommandQueue()`
- **描述符堆大小**：`mRtvDescriptorSize`、`mDsvDescriptorSize`、`mCbvSrvUavDescriptorSize`
- **视口/裁剪**：`mScreenViewport`、`mScissorRect`
- **格式配置**：`mBackBufferFormat`（R8G8B8A8_UNORM）、`mDepthStencilFormat`（D24_UNORM_S8_UINT）
- **4xMSAA**：`m4xMsaaState`、`m4xMsaaQuality`

关键虚函数（派生类必须/可选重写）：
- 纯虚：`Update(gt)`、`Draw(gt)` —— 派生类必须实现
- 可选：`Initialize()`、`OnResize()`、`MsgProc()`、`OnMouseDown/Up/Move()`

便捷辅助：`CurrentBackBuffer()`、`CurrentBackBufferView()`、`DepthStencilView()`、`AspectRatio()`、`Run()`（主循环）。

### 7.2.2 `UploadBuffer<T>`（`UploadBuffer.h`）

**上传缓冲区模板** —— CPU 可写、GPU 可读的缓冲区，是常量缓冲区与动态顶点缓冲的基石。

```cpp
template<typename T>
class UploadBuffer {
    UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer);
    void CopyData(int elementIndex, const T& data);  // CPU 端写入第 i 个元素
    ID3D12Resource* Resource() const;                // 取底层 GPU 资源
};
```

要点：
- 创建于 `D3D12_HEAP_TYPE_UPLOAD` 堆，状态为 `GENERIC_READ`
- 构造时 `Map()` 持久映射，析构时 `Unmap()`
- **常量缓冲区特例**：若 `isConstantBuffer=true`，每个元素按 **256 字节对齐**（`d3dUtil::CalcConstantBufferByteSize`）。这是 D3D12 硬件要求：CBV 的偏移与大小都必须是 256 的倍数
- `CopyData` 仅是 `memcpy` 到映射内存，**不涉及 GPU 同步** —— 同步靠 FrameResource 机制保证

### 7.2.3 `GeometryGenerator`（`GeometryGenerator.h/.cpp`）

程序化生成常见几何体的静态工具类（命名空间 `DirectX12` 内）。返回 `MeshData`（含 `Vertices` 与 `Indices32`，并可取 `GetIndices16()`）。

可用方法：
- `CreateBox(w, h, d, numSubdivisions)` —— 可细分的盒子
- `CreateSphere(radius, sliceCount, stackCount)`
- `CreateGeosphere(radius, numSubdivisions)`
- `CreateCylinder(bottomRadius, topRadius, height, sliceCount, stackCount)`
- `CreateGrid(width, depth, m, n)` —— xz 平面网格
- `CreateQuad(...)` —— 屏幕对齐四边形

每个顶点含 `Position / Normal / TangentU / TexC`。三角形默认**外朝向**。

### 7.2.4 其他公共组件

- `MathHelper`：`Identity4x4()`、`Clamp`、`Pi`、`RandF`/`Rand`、矩阵辅助等
- `d3dUtil`：`ThrowIfFailed`、`CalcConstantBufferByteSize`、`CreateDefaultBuffer`（创建默认堆 GPU 缓冲，含上传中间资源）、`CompileShader`、`MeshGeometry`/`SubmeshGeometry` 结构
- `GameTimer`：`TotalTime()`、`DeltaTime()`
- `d3dx12.h`：`CD3DX12_*` 系列辅助结构

### 7.2.5 `MeshGeometry` / `SubmeshGeometry`（定义于 d3dUtil）

```cpp
struct MeshGeometry {
    std::string Name;
    UINT VertexByteStride, VertexBufferByteSize;
    DXGI_FORMAT IndexFormat;
    UINT IndexBufferByteSize;
    D3DBlob VertexBufferCPU, IndexBufferCPU;     // CPU 端副本
    ComPtr<ID3D12Resource> VertexBufferGPU, IndexBufferGPU, VertexBufferUploader;
    std::unordered_map<std::string, SubmeshGeometry> DrawArgs;  // 子网格
};
struct SubmeshGeometry {
    UINT IndexCount, StartIndexLocation, BaseVertexLocation;
};
```

**关键设计**：一个大顶点/索引缓冲可容纳多个子网格，通过 `SubmeshGeometry` 描述每个子网格的区域（索引数、起始索引、基准顶点）。一个 `DrawArgs` 映射表让多种几何共用同一缓冲 —— 这正是 Shapes 把 box/grid/sphere/cylinder 拼进一个 `shapeGeo` 的方式。

---

## 7.3 核心概念一：帧资源 FrameResource

### 7.3.1 为什么需要帧资源

D3D12 是**显式同步**的 API。CPU 提交命令后 GPU 异步执行，若 CPU 立刻复用 GPU 还在使用的命令分配器/常量缓冲区，会破坏数据完整性。**FrameResource 的核心思想：为每一帧（最多在途 N 帧）准备一套独立的资源，CPU 写第 k+N 帧时不会触碰 GPU 还在用的第 k 帧。**

```cpp
const int gNumFrameResources = 3;  // 三缓冲
```

### 7.3.2 FrameResource 结构

**Shapes 版本**（`Shapes/FrameResource.h`）：

```cpp
struct FrameResource {
    FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount);
    // 不可拷贝
    ComPtr<ID3D12CommandAllocator> CmdListAlloc;             // 每帧独立的命令分配器
    std::unique_ptr<UploadBuffer<PassConstants>> PassCB;     // 每帧独立的 pass 常量缓冲
    std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB; // 每帧独立的 object 常量缓冲
    UINT64 Fence = 0;  // 围栏值：标记该帧资源何时被 GPU 用完
};
```

**LandAndWaves 版本**多一个动态顶点缓冲（构造多一个 `waveVertCount` 参数）：

```cpp
std::unique_ptr<UploadBuffer<Vertex>> WavesVB;  // 水波顶点缓冲，每帧独立
// 构造：WavesVB = make_unique<UploadBuffer<Vertex>>(device, waveVertCount, false);  // 非常量缓冲
```

要点：
- `CmdListAlloc` 每帧独立 —— **命令分配器在 GPU 执行完其命令前不能 Reset**
- `PassCB` / `ObjectCB` 每帧独立 —— **常量缓冲在 GPU 还在读取时不能被 CPU 覆写**
- `WavesVB` 每帧独立 —— 动态顶点缓冲同理
- `Fence` 字段用于追踪该帧资源是否仍被 GPU 占用

主程序持有环形数组：

```cpp
std::vector<std::unique_ptr<FrameResource>> mFrameResources;
FrameResource* mCurrFrameResource = nullptr;
int mCurrFrameResourceIndex = 0;
```

---

## 7.4 核心概念二：渲染项 RenderItem 与脏标记

### 7.4.1 RenderItem 结构

两个示例的 `RenderItem` 几乎一致（定义在各自 App.cpp 内）：

```cpp
struct RenderItem {
    XMFLOAT4X4 World = Identity4x4();                 // 世界矩阵
    int NumFramesDirty = gNumFrameResources;          // 脏标记
    UINT ObjCBIndex = -1;                              // 在 ObjectCB 中的索引
    MeshGeometry* Geo = nullptr;                       // 几何体
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = TRIANGLELIST;
    UINT IndexCount = 0;                               // DrawIndexedInstanced 参数
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};
```

### 7.4.2 脏标记机制（NumFramesDirty）

**问题**：物体世界矩阵若改变，需更新到所有在途的 FrameResource 的 ObjectCB（因为每个 FrameResource 有自己的 ObjectCB 副本）。但每帧都全量重写所有物体开销大。

**解决**：用 `NumFramesDirty` 计数器。当修改物体数据时，设 `NumFramesDirty = gNumFrameResources`。每帧更新时：

```cpp
void UpdateObjectCBs(const GameTimer& gt) {
    auto currObjectCB = mCurrFrameResource->ObjectCB.get();
    for(auto& e : mAllRitems) {
        if(e->NumFramesDirty > 0) {                   // 只更新仍"脏"的物体
            XMMATRIX world = XMLoadFloat4x4(&e->World);
            ObjectConstants objConstants;
            XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));  // 行主序转列主序
            currObjectCB->CopyData(e->ObjCBIndex, objConstants);
            e->NumFramesDirty--;                       // 递减，三帧后所有副本都更新完毕
        }
    }
}
```

> 注意 `XMMatrixTranspose`：DirectXMath 用行主序矩阵，而 HLSL 的 `cbuffer` 期望列主序，所以上传前需转置。

### 7.4.3 渲染项组织

主程序维护两个集合：
- `std::vector<std::unique_ptr<RenderItem>> mAllRitems;` —— 拥有所有渲染项（统一生命周期）
- 按 PSO/渲染层分组：
  - Shapes：`std::vector<RenderItem*> mOpaqueRitems;`（一个不透明层）
  - LandAndWaves：`std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];`（用枚举 `RenderLayer` 索引，更通用，便于未来扩展透明层等）

```cpp
enum class RenderLayer : int { Opaque = 0, Count };
```

绘制时按层遍历，同层共用同一 PSO，减少状态切换。

---

## 7.5 核心概念三：常量缓冲区结构

### 7.5.1 ObjectConstants（逐物体）

```cpp
struct ObjectConstants {
    XMFLOAT4X4 World = Identity4x4();  // 仅一个世界矩阵
};
```

### 7.5.2 PassConstants（逐帧/pass，所有物体共享）

```cpp
struct PassConstants {
    XMFLOAT4X4 View, InvView, Proj, InvProj, ViewProj, InvViewProj;
    XMFLOAT3 EyePosW;  float cbPerObjectPad1;          // 注意 padding 对齐
    XMFLOAT2 RenderTargetSize, InvRenderTargetSize;
    float NearZ, FarZ, TotalTime, DeltaTime;
};
```

要点：
- 包含**完整的相机矩阵族**（正/逆 × View/Proj/ViewProj），供着色器各种空间变换
- `EyePosW` 后插了一个 `cbPerObjectPad1` —— **HLSL cbuffer 严苛的对齐规则**：`float3` 后跟普通 `float`，需 padding 使下一个成员（这里是 `float2`）从 8 字节边界开始
- `UpdateMainPassCB` 每帧重算 ViewProj、各逆矩阵（逆矩阵需先算行列式作为 `XMMatrixInverse` 参数），转置后写入当前帧的 PassCB

### 7.5.3 顶点结构

```cpp
struct Vertex {
    XMFLOAT3 Pos;
    XMFLOAT4 Color;  // 顶点颜色（本章无光照，直接输出颜色）
};
```

输入布局对应：
```cpp
{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  PER_VERTEX_DATA, 0 },
{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, PER_VERTEX_DATA, 0 },
```

---

## 7.6 核心概念四：根签名、描述符堆与 CBV

本章两个示例的**最大差异**在于常量缓冲如何绑定到着色器。

### 7.6.1 方式 A：描述符表 + 描述符堆（Shapes 用）

**根签名**（`Shapes::BuildRootSignature`）：

```cpp
CD3DX12_DESCRIPTOR_RANGE cbvTable0;  cbvTable0.Init(CBV, 1, 0);  // b0
CD3DX12_DESCRIPTOR_RANGE cbvTable1;  cbvTable1.Init(CBV, 1, 1);  // b1
CD3DX12_ROOT_PARAMETER slotRootParameter[2];
slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);  // 物体 CBV（表）
slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1);  // pass CBV（表）
```

**描述符堆**（`BuildDescriptorHeaps`）：
- 需要的描述符总数 = `(objCount + 1) * gNumFrameResources`
  - 每个物体 × 每个帧资源 一个 ObjectCBV
  - 每个帧资源一个 PassCBV（"+1"）
- `mPassCbvOffset = objCount * gNumFrameResources` —— pass CBV 放在堆尾部

**CBV 创建**（`BuildConstantBufferViews`）：
- 对每个帧资源、每个物体：取 ObjectCB 的 GPU 虚拟地址，按 256 对齐偏移到第 i 个物体，创建 CBV 到描述符堆对应槽
- pass CBV 同理

**绘制时绑定**（`DrawRenderItems`）：
```cpp
// 逐物体：计算 cbvIndex = frameIndex*objCount + objCBIndex
// Offset 句柄，SetGraphicsRootDescriptorTable(0, cbvHandle)
// pass CBV 在 Draw 开头绑一次：SetGraphicsRootDescriptorTable(1, passCbvHandle)
```

> 描述符表方式开销较大（每个物体一个描述符，且要维护堆），但更灵活，是后续纹理/SRV 的通用方式。

### 7.6.2 方式 B：根描述符（root descriptor）（LandAndWaves 用）

**根签名**（`LandAndWaves::BuildRootSignature`）：

```cpp
CD3DX12_ROOT_PARAMETER slotRootParameter[2];
slotRootParameter[0].InitAsConstantBufferView(0);  // 直接指向 b0 的虚拟地址
slotRootParameter[1].InitAsConstantBufferView(1);  // 直接指向 b1 的虚拟地址
```

**绘制时绑定**（`DrawRenderItems`）：
```cpp
D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress();
objCBAddress += ri->ObjCBIndex * objCBByteSize;   // 偏移到该物体
cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);
// pass CBV：SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress())
```

> 根描述符方式**无需描述符堆**、无需创建 CBV 描述符，代码大幅简化。代价：根描述符不带大小/格式信息（GPU 不能做越界检查），且占根签名的"代价"更高。对于纯常量缓冲、数量适中的场景，根描述符是更简洁的选择。

---

## 7.7 示例一：Shapes（多形状静态场景）

### 7.7.1 场景内容

地面网格上摆放：1 个放大盒子、1 个大网格地面、左右各 5 根圆柱 + 5 个球体（共 20 个柱+球）。几何体颜色固定：盒子暗绿、地面森林绿、球体深红、圆柱钢蓝。

按键：按住 `1` 键切换线框模式。

### 7.7.2 几何体构建（BuildShapeGeometry）

**关键技巧：合并几何体到单一大缓冲**

```cpp
GeometryGenerator geoGen;
auto box      = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 3);
auto grid     = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
auto sphere   = geoGen.CreateSphere(0.5f, 20, 20);
auto cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);
```

1. 计算各物体在大顶点/索引缓冲中的偏移
2. 创建 4 个 `SubmeshGeometry` 记录每个区域
3. 把所有顶点拼进一个 `std::vector<Vertex>`（填入位置+固定颜色）
4. 把所有索引拼进一个 `std::vector<uint16_t>`
5. 用 `D3DCreateBlob` 存 CPU 副本，用 `d3dUtil::CreateDefaultBuffer` 上传到 GPU 默认堆
6. `geo->DrawArgs["box"/"grid"/"sphere"/"cylinder"] = 子网格` —— 后续按名字取子网格绘制

好处：减少顶点/索引缓冲数量，减少 `IASetVertexBuffers` 调用。

### 7.7.3 渲染项构建（BuildRenderItems）

每个物体创建一个 `RenderItem`，分配 `ObjCBIndex`，绑定到 `shapeGeo` 的对应子网格：

```cpp
boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
```

循环 5 次生成左右圆柱与球体，世界矩阵用 `XMMatrixTranslation` 定位。最后全部归入 `mOpaqueRitems`。

### 7.7.4 PSO 构建（BuildPSOs）

创建两个 PSO：
- `opaque`：实体填充（注意：源码 `ShapesApp.cpp:671` 给 `opaque` 也设了 `FillMode = WIREFRAME`，与命名语义不符，疑似遗留 —— 实测时留意）
- `opaque_wireframe`：线框填充 `D3D12_FILL_MODE_WIREFRAME`

PSO 完整字段：input layout、root signature、VS/PS、光栅化/混合/深度模板状态、采样数、RTV/DSV 格式、拓扑类型。

### 7.7.5 主循环

```
Initialize():
  Reset 命令分配器 → Build* 系列 → Close → ExecuteCommandLists → FlushCommandQueue

Run()（基类）循环:
  CalculateFrameStats → 更新计时器 → Update(gt) → Draw(gt)

Update():
  OnKeyboardInput → UpdateCamera → 切换到下一帧资源 → 必要时等围栏
  → UpdateObjectCBs(脏标记) → UpdateMainPassCB

Draw():
  Reset 当前帧分配器 → Reset 命令列表(选 PSO) → 设视口/裁剪
  → 屏障(PRESENT→RENDER_TARGET) → 清颜色/深度 → OMSetRenderTargets
  → 设描述符堆 → 设根签名 → 绑 pass CBV → DrawRenderItems
  → 屏障(RENDER_TARGET→PRESENT) → Close → Execute → Present → Signal 围栏
```

### 7.7.6 相机（轨道相机）

球坐标（mTheta, mPhi, mRadius）转笛卡尔，`XMMatrixLookAtLH` 朝原点。鼠标：
- 左键拖：改 theta/phi（绕转）
- 右键拖：改 radius（缩放），clamp 在 [5, 150]

---

## 7.8 示例二：LandAndWaves（地形 + 水波动态场景）

### 7.8.1 场景内容

程序化生成的丘陵地形（160×160，50×50 网格，按高度函数起伏），顶点按高度着色（沙滩→草地→深绿→棕土→雪顶）；其上覆盖一片水波面（128×128 顶点网格），每 0.25 秒随机扰动产生波纹，CPU 端做波动方程数值积分，逐帧更新顶点缓冲。

按键：按住 `1` 切换线框。

### 7.8.2 常量缓冲绑定：根描述符方式

见 [7.6.2](#762-方式-b根描述符root-descriptorlandandwaves-用)。比 Shapes 简洁：无需 `BuildDescriptorHeaps` / `BuildConstantBufferViews`，根签名直接两个 root CBV。

### 7.8.3 地形几何（BuildLandGeometry）

```cpp
auto grid = geoGen.CreateGrid(160.0f, 160.0f, 50, 50);
for each vertex:
    vertices[i].Pos.y = GetHillsHeight(p.x, p.z);  // 高度函数
    vertices[i].Color = 按高度分段的颜色;
```

高度函数：
```cpp
float GetHillsHeight(float x, float z) {
    return 0.3f * (z * sinf(0.1f*x) + x * cosf(0.1f*z));
}
```

颜色分段：`y < -10` 沙黄、`< 5` 浅黄绿、`< 12` 深黄绿、`< 20` 棕、`>= 20` 雪白。

另有 `GetHillsNormal`（解析偏导数 `n = (-df/dx, 1, -df/dz)` 归一化），本章虽定义但着色器未用（无光照）。

### 7.8.4 水波几何（BuildWavesGeometryBuffers）

- **顶点缓冲初始为空**（`VertexBufferCPU = nullptr; VertexBufferGPU = nullptr;`），因为每帧由 `WavesVB` 动态填充
- 索引缓冲**静态**：按网格四边形拆成两个三角形，预生成所有索引
- 顶点格式 `R16_UINT`（断言顶点数 < 65535）

### 7.8.5 水波模拟类（Waves.h/.cpp）

CPU 端实现二维波动方程的有限差分数值积分。

构造参数：`(m行, n列, dx空间步长, dt时间步长, speed波速, damping阻尼)`。示例用 `Waves(128, 128, 1.0f, 0.03f, 4.0f, 0.2f)`。

预计算系数：
```cpp
d = damping*dt + 2;
e = speed^2 * dt^2 / dx^2;
mK1 = (damping*dt - 2) / d;
mK2 = (4 - 8*e) / d;
mK3 = (2*e) / d;
```

更新（`Update(dt)`）：
- 累积时间，达到 `mTimeStep` 才更新一次（固定时间步）
- 用 `concurrency::parallel_for` 并行遍历内部点，**就地更新**：
  ```
  prev[i,j].y = K1*prev[i,j].y + K2*curr[i,j].y + K3*(curr[四个邻居].y 之和)
  ```
- `std::swap(mPrevSolution, mCurrSolution)` —— 前一解变当前解
- 用有限差分计算法线与切线（供未来光照用）

扰动（`Disturb(i, j, magnitude)`）：给中心及四邻顶点叠加高度。

### 7.8.6 动态顶点缓冲更新（UpdateWaves）

```cpp
// 每 0.25s 随机扰动一个内部点
mWaves->Disturb(randI, randJ, randF(0.2, 0.5));
// 步进模拟
mWaves->Update(gt.DeltaTime());
// 把当前解写进"当前帧资源"的 WavesVB
auto currWavesVB = mCurrFrameResource->WavesVB.get();
for(i in 0..VertexCount) {
    v.Pos = mWaves->Position(i);
    v.Color = Blue;
    currWavesVB->CopyData(i, v);
}
// 关键：把渲染项的 VertexBufferGPU 指针指向当前帧的 WavesVB
mWavesRitem->Geo->VertexBufferGPU = currWavesVB->Resource();
```

> 这是"每帧一个动态上传顶点缓冲"的标准做法 —— 配合 FrameResource 三缓冲，CPU 写第 k+3 帧的 VB 时不会干扰 GPU 读第 k 帧的 VB。`VertexBufferView()` 会读取当前 `VertexBufferGPU`，所以每帧自然绑定到正确的帧缓冲。

### 7.8.7 渲染项

```cpp
auto wavesRitem = ...;  // waterGeo，ObjCBIndex=0
auto gridRitem  = ...;  // landGeo， ObjCBIndex=1
// 都进 Opaque 层
```

注意：`BuildRenderItems()` 在 `Initialize` 里被调用了**两次**（`LandAndWavesApp.cpp:186-187`，疑似源码笔误 —— 第二次会重复 push，导致 `mAllRitems`/`mRitemLayer` 含重复项、实际多绘一次。使用时留意）。

---

## 7.9 着色器 color.hlsl

两个项目的 `color.hlsl` **完全相同**。

```hlsl
cbuffer cbPerObject : register(b0) { float4x4 gWorld; };
cbuffer cbPass      : register(b1) { /* View/InvView/Proj/.../EyePosW/RTSize/NearZ/TotalTime... */ };

struct VertexIn  { float3 PosL : POSITION; float4 Color : COLOR; };
struct VertexOut { float4 PosH : SV_POSITION; float4 Color : COLOR; };

VertexOut VS(VertexIn vin) {
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);  // 局部→世界
    vout.PosH = mul(posW, gViewProj);                    // 世界→齐次裁剪
    vout.Color = vin.Color;
    return vout;
}

float4 PS(VertexOut pin) : SV_Target {
    return pin.Color;  // 直接输出顶点颜色，无光照
}
```

要点：
- 变换路径：**局部空间 →（gWorld）→ 世界空间 →（gViewProj）→ 齐次裁剪空间**
- cbuffer 布局必须与 C++ 端 `ObjectConstants`/`PassConstants` **逐字段逐 padding 对应**
- 本章无光照、无纹理，像素着色器仅回传颜色

---

## 7.10 CPU/GPU 同步与帧循环机制

### 7.10.1 围栏（Fence）机制

基类提供 `mFence` + `mCurrentFence` + `FlushCommandQueue()`。每帧 Draw 末尾：

```cpp
mCurrFrameResource->Fence = ++mCurrentFence;            // 给该帧资源打围栏值
mCommandQueue->Signal(mFence.Get(), mCurrentFence);    // GPU 完成到此才把完成值推到 mCurrentFence
```

### 7.10.2 帧资源等待

Update 开头，切换到下一帧资源前，检查该帧资源是否还被 GPU 占用：

```cpp
mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence) {
    // GPU 还没追上这个围栏值 → 阻塞等待
    HANDLE eventHandle = CreateEventEx(...);
    mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle);
    WaitForSingleObject(eventHandle, INFINITE);
    CloseHandle(eventHandle);
}
```

> 三缓冲的意义：CPU 最多领先 GPU 3 帧。若 GPU 跟得上，CPU 几乎不等；若 GPU 慢，CPU 在第 4 帧被迫等 GPU 释放第 1 帧的资源。这是显式同步的标准节奏控制。

### 7.10.3 完整一帧时序

```
[CPU] Update(切帧资源→等围栏→写CB→更新Waves)
   → Draw(Reset分配器→录命令→Close→Execute→Present→Signal)
       │ 命令进队列，GPU 异步执行
[GPU] ...执行命令... 完成后把 Fence CompletedValue 推进
[CPU 下一帧] Update 检查围栏，决定是否等待
```

---

## 7.11 Shapes vs LandAndWaves 对比

| 维度 | Shapes | LandAndWaves |
|------|--------|--------------|
| **CBV 绑定** | 描述符表 + 描述符堆 | 根描述符（root CBV） |
| **根签名** | 2 个 `InitAsDescriptorTable` | 2 个 `InitAsConstantBufferView` |
| **描述符堆** | 有（CBV 堆，含所有物体+pass） | 无 |
| **ObjectCB 寻址** | 描述符堆偏移到第 i 个物体 | 直接 GPU 虚拟地址偏移 |
| **PassCB 绑定** | `SetGraphicsRootDescriptorTable` | `SetGraphicsRootConstantBufferView` |
| **动态顶点缓冲** | 无 | 有（WavesVB，水波） |
| **FrameResource 额外成员** | — | `WavesVB`（UploadBuffer\<Vertex\>） |
| **几何体** | box/grid/sphere/cylinder 拼一个缓冲 | 地形网格（程序高度）+ 水波网格 |
| **渲染层** | 单 `mOpaqueRitems` | `RenderLayer` 枚举数组（可扩展） |
| **着色器目标** | `vs_5_1` / `ps_5_1` | `vs_5_0` / `ps_5_0` |
| **PrimitiveType 初值** | `D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST` | `D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST`（等价） |
| **物理模拟** | 无 | 波动方程有限差分（parallel_for 并行） |

### 选择建议

- **纯常量缓冲、物体数适中** → 根描述符（LandAndWaves 风格）代码最简
- **需要纹理/SRV、描述符数量大、需要堆管理** → 描述符表（Shapes 风格），也是后续章节的标准做法

---

## 7.12 关键知识点总结

### 7.12.1 设计模式

1. **FrameResource 环形缓冲**：N=3 缓冲解耦 CPU/GPU，每帧独立命令分配器 + 常量缓冲（+ 动态 VB）
2. **脏标记（NumFramesDirty）**：只把变化的数据传播到在途的各帧副本，避免全量重写
3. **PassCB / ObjectCB 分离**：相机等每帧不变量进 PassCB（绘一次绑一次），物体世界矩阵进 ObjectCB（逐物体绑）
4. **渲染项分层**：按 PSO/透明度分组，减少状态切换
5. **大缓冲 + 子网格**：多种几何拼进单一大顶点/索引缓冲，`SubmeshGeometry` 描述区域

### 7.12.2 常量缓冲规则

- 常量缓冲元素 **256 字节对齐**（`CalcConstantBufferByteSize`）
- HLSL `cbuffer` 成员有严苛对齐规则，`float3` 后通常需 padding
- DirectXMath 行主序 → HLSL 列主序，上传前 `XMMatrixTranspose`
- 上传缓冲 `Map` 持久映射，`CopyData` 仅 memcpy，同步靠 FrameResource

### 7.12.3 根签名两种 CBV 方式

- **描述符表**：灵活，支持堆，适合纹理/SRV；需维护描述符堆与 CBV 创建
- **根描述符**：直接传 GPU 虚拟地址，无需堆，代码简；占根签名代价高，无越界保护

### 7.12.4 动态顶点缓冲

- 用 `UploadBuffer<Vertex>`（`isConstantBuffer=false`，元素按 sizeof 对齐而非 256）
- 每帧资源持有一个，CPU 写当前帧、GPU 读在途帧互不干扰
- 每帧把渲染项的 `VertexBufferGPU` 指针指向当前帧缓冲

### 7.12.5 命令录制与提交流程

```
Reset(分配器) → Reset(命令列表, PSO) → 录制(屏障/清理/绑定/绘制/屏障) → Close
→ ExecuteCommandLists → Present → Signal(围栏)
```

### 7.12.6 CPU/GPU 同步

- 围栏值标记帧资源完成点
- 切换帧资源前检查 `GetCompletedValue < FrameResource.Fence` → 必要阻塞等待
- 三缓冲 = CPU 最多领先 GPU 3 帧

### 7.12.7 本章未涉及（后续章节）

- 光照与材质（本章像素直接输出颜色）
- 纹理贴图（本章顶点颜色）
- 法线/切线的实际使用（已生成但着色器未用）
- 实例化绘制（`DrawIndexedInstanced` 的 instanceCount=1）

---

## 7.13 源码文件清单与行数

| 文件 | 行数 | 作用 |
|------|------|------|
| `Shapes/ShapesApp.cpp` | 806 | Shapes 主程序 |
| `Shapes/FrameResource.h/.cpp` | 59/16 | 帧资源（PassCB+ObjectCB） |
| `Shapes/Shaders/color.hlsl` | 61 | 着色器 |
| `LandAndWaves/LandAndWavesApp.cpp` | 756 | LandAndWaves 主程序 |
| `LandAndWaves/FrameResource.h/.cpp` | 63/18 | 帧资源（+WavesVB） |
| `LandAndWaves/Waves.h/.cpp` | 62/172 | 水波模拟类 |
| `LandAndWaves/Shaders/color.hlsl` | 61 | 着色器（同 Shapes） |
| `Common/UploadBuffer.h` | 64 | 上传缓冲模板 |
| `Common/GeometryGenerator.h/.cpp` | 121/- | 程序化几何生成 |
| `Common/d3dApp.h/.cpp` | 128/- | D3D 应用基类 |

> 源码中观察到的小问题（使用/学习时留意，非阻塞）：
> - `ShapesApp.cpp:671` 给"实体" `opaque` PSO 也设了 `WIREFRAME`，与命名语义不符
> - `LandAndWavesApp.cpp:186-187` `BuildRenderItems()` 被调用两次，导致渲染项重复入列
