#pragma once
#include "GenericPlatform.h"
#include "EnumClassFlags.h"

// UE 对照：Runtime/RHI/Public/RHIAccess.h。
// 保留当前阶段需要的访问类型，名称和位值与 UE 对齐。
// 声明枚举不代表后端已经支持对应的转换，后续逐项接入。

//之所以使用位标志，是因为有些访问方式可以组合。例如一张纹理同时供顶点 Shader 和像素 Shader 读取：ERHIAccess::SRVGraphicsPixel | ERHIAccess::SRVGraphicsNonPixel
//不过，位标志可以组合，不代表任意组合都合法；后面需要做合法性检查。
enum class ERHIAccess :uint32
{
	// 之前的访问状态未知。后端必须按其状态追踪与同步规则处理，不能直接跳过屏障。
    // UE 的注释强调未知状态可能需要保守的缓存同步；它不等于 D3D12 的 COMMON 状态。这个状态提示我们应该flush all gpu caches
	Unknown = 0,

    // 只读访问。

    // 交换链图像交给呈现系统使用；例如 backbuffer 完成绘制后转换到此访问状态。
    // 状态转换本身不会调用 Present，也不意味着 CPU 已等到 GPU 完成。
    Present = 1u << 1,

    // Buffer 被输入装配阶段作为顶点缓冲或索引缓冲读取。
    // 描述的是顶点/索引输入用途，不是 Shader 通过 SRV 读取 Buffer。
    VertexOrIndexBuffer = 1u << 3,

    // 计算着色器（Compute Shader）通过 SRV 只读访问纹理或 Buffer。
    // Compute 指着色器阶段，不代表必须在独立的异步计算队列上执行。
    SRVCompute = 1u << 4,

    // 像素着色器（Pixel Shader）通过 SRV 只读访问资源，例如采样棋盘格纹理。
    SRVGraphicsPixel = 1u << 5,

    // 图形管线中除像素着色器之外的 Shader 阶段通过 SRV 只读访问资源。
    // 例如顶点着色器读取纹理或结构化 Buffer；不包含 Compute Shader。
    SRVGraphicsNonPixel = 1u << 6,

    // 资源作为复制命令的源，被 GPU 读取；例如将纹理内容复制到回读 Buffer。
    CopySrc = 1u << 7,

    // 深度/模板附件以只读方式参与测试，不写入所指定的深度/模板子资源。
    // 这不等于 Shader 的 SRV 读取；若还需 Shader 读取，要声明对应 SRV 访问并满足组合规则。
    DSVRead = 1u << 9,

    // 包含写入的访问。

    // Compute Shader 通过 UAV 访问资源，允许读取、写入或原子操作（取决于资源和 Shader 声明）。
    // UAV 不表示每条命令一定发生写入；连续 UAV 访问也可能需要额外的 UAV 屏障。
    UAVCompute = 1u << 10,

    // 图形 Shader 通过 UAV 访问资源，例如像素着色器写 RWStructuredBuffer。
    // 具体哪些 Shader 阶段与资源类型可用，仍受后端和硬件能力限制。
    UAVGraphics = 1u << 11,

    // 纹理作为颜色渲染目标使用，例如接收像素输出、混合结果或执行颜色清除。
    // 这是访问用途；RTV 描述符的创建与绑定仍由对应的 View 和 RenderPass 接口负责。
    RTV = 1u << 12,

    // 资源作为复制命令的目标，被 GPU 写入；例如接收上传 Buffer 复制来的纹理数据。
    CopyDest = 1u << 13,

    // 深度/模板附件允许写入，例如深度测试通过后更新深度，或执行深度/模板清除。
    // 最终是否写入仍取决于绘制状态与具体命令；访问状态本身不会开启深度写入。
    DSVWrite = 1u << 15,

    // Unknown 的别名，数值同为 0，表示没有设置任何明确的访问位。
    // 不是另一种独立的 GPU 状态，也不能据此判断资源已经不再被 GPU 使用。
    None = Unknown,

    // 图形管线所有 SRV 读取阶段的组合：像素阶段 + 非像素阶段，不包含 Compute。
    SRVGraphics = SRVGraphicsPixel | SRVGraphicsNonPixel,

    // 所有 SRV 访问位的集合，常用于检查某个访问值是否包含任一种 SRV 读取。
    // 它是组合掩码，不是额外分配的独立状态位。
    SRVMask = SRVCompute | SRVGraphics,

    // 所有 UAV 访问位的集合，常用于检查某个访问值是否包含图形或计算 UAV 访问。
    // 此掩码本身不提供图形/计算队列之间的同步。
    UAVMask = UAVCompute | UAVGraphics,
};

ENUM_CLASS_FLAGS(ERHIAccess)
