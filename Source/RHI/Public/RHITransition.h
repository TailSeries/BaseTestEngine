#pragma once
#include "RHIAccess.h"
#include "RHIDefinitions.h"
#include <limits>
#include <stdexcept>
#include "RHIPipeline.h"
#include <span>
#include "RHIModule.h"
#include <cstddef>

class FRHITexture;
class FRHIBuffer;
/*
* 示例：FRHITransitionInfo(
    Texture,
    ERHIAccess::RTV,
    ERHIAccess::SRVGraphicsPixel); 这张纹理此前作为颜色渲染目标使用，接下来供像素 Shader 读取，范围默认是整个资源。
*/


// 描述纹理转换所覆盖的 mip、数组层和深度/模板平面。
// 每个维度可以选择单个索引，也可以选择该维度的全部子资源。

//FRHISubresourceRange 描述转换范围：
/*
| `MipIndex` | 某一级 mip，或所有 mip |
| `ArraySlice` | 某一个纹理数组层，或所有数组层 | 纹理数组里的哪一张图
| `PlaneSlice` | 某个平面，例如深度平面或模板平面 | 这张图中的哪一类数据
| `kAllSubresources` | 用作各字段的“全部”标记 |

**如何理解这个所谓的平面：拿一张同时保存深度和模板的纹理来举例：
PlaneSlice = kDepthPlaneSlice;   // 只选择深度部分
PlaneSlice = kStencilPlaneSlice; // 只选择模板部分
PlaneSlice = kAllSubresources;   // 选择所有平面

*/

struct FRHISubresourceRange
{
    static constexpr uint16 kDepthPlaneSlice = 0;
    static constexpr uint16 kStencilPlaneSlice = 1;
    static constexpr uint16 kAllSubresources = (std::numeric_limits<uint16>::max)();

    uint16 MipIndex = kAllSubresources;
    uint16 ArraySlice = kAllSubresources;
    uint16 PlaneSlice = kAllSubresources;

    FRHISubresourceRange() = default;

    FRHISubresourceRange(uint32 InMipIndex, uint32 InArraySlice, uint32 InPlaneSlice)
    {
        // 只检查能否存入 uint16。索引是否超出具体纹理的范围，后续由处理转换的代码验证。
        if (InMipIndex > kAllSubresources ||
            InArraySlice > kAllSubresources ||
            InPlaneSlice > kAllSubresources)
        {
            throw std::out_of_range(
                "RHI subresource index exceeds uint16 range");
        }
        MipIndex = static_cast<uint16>(InMipIndex);
        ArraySlice = static_cast<uint16>(InArraySlice);
        PlaneSlice = static_cast<uint16>(InPlaneSlice);
    }

    bool IsAllMips() const
    {
        return MipIndex == kAllSubresources;
    }

    bool IsAllArraySlices() const
    {
        return ArraySlice == kAllSubresources;
    }

    bool IsAllPlaneSlices() const
    {
        return PlaneSlice == kAllSubresources;
    }

    bool IsWholeResource() const
    {
        return IsAllMips() &&
            IsAllArraySlices() &&
            IsAllPlaneSlices();
    }
};


// 单个资源的转换描述。
// 保留 UE 的“子资源范围 + 带类型的资源指针 + 前后访问 + 标志”结构。
struct FRHITransitionInfo : public FRHISubresourceRange
{
    /*
    * 目前有三个细节需要记住：
    - 指针是借用的。 构造转换描述不会增加引用计数，调用方仍需保证资源生命周期；GPU 使用期间的保护继续依赖我们的 Fence 与延迟释放机制。
    - 范围描述先保留完整骨架。 第一版后端可以先只支持整个资源，遇到局部范围明确报错，后续再接入逐子资源追踪，不能默默忽略范围。
    - AccessBefore/After 描述 GPU 命令顺序中的访问变化。 它们不表示 CPU 已经等到 GPU 执行完成。
    */

    // 当前支持 Texture 和 Buffer。
   // 按 Type 读取对应成员，不把不同类型的指针成员混着使用。
    union
    {
        FRHITexture* Texture = nullptr;
        FRHIBuffer* Buffer;
    };

    enum class EType : uint8
    {
        Unknown,
        Texture,
        Buffer,
    } Type = EType::Unknown;


    ERHIAccess AccessBefore = ERHIAccess::Unknown;
    ERHIAccess AccessAfter = ERHIAccess::Unknown;

    EResourceTransitionFlags Flags = EResourceTransitionFlags::None;

    // 默认构造仅提供空描述，不能直接作为有效转换提交。
    FRHITransitionInfo() = default;
    // 纹理转换：默认覆盖整个资源，也可指定子资源范围。
    FRHITransitionInfo(
        FRHITexture* InTexture,
        ERHIAccess InPreviousState,
        ERHIAccess InNewState,
        EResourceTransitionFlags InFlags = EResourceTransitionFlags::None,
        uint32 InMipIndex = kAllSubresources,
        uint32 InArraySlice = kAllSubresources,
        uint32 InPlaneSlice = kAllSubresources)
        : FRHISubresourceRange(
            InMipIndex,
            InArraySlice,
            InPlaneSlice)
        , Texture(InTexture)
        , Type(EType::Texture)
        , AccessBefore(InPreviousState)
        , AccessAfter(InNewState)
        , Flags(InFlags)
    {}

    // Buffer 转换：当前按整个 Buffer 处理，不指定字节范围。
    FRHITransitionInfo(
        FRHIBuffer* InRHIBuffer,
        ERHIAccess InPreviousState,
        ERHIAccess InNewState,
        EResourceTransitionFlags InFlags = EResourceTransitionFlags::None)
        : Buffer(InRHIBuffer)
        , Type(EType::Buffer)
        , AccessBefore(InPreviousState)
        , AccessAfter(InNewState)
        , Flags(InFlags)
    {}
};
// 创建一次 Transition 所需的参数。
// UE 使用 TArrayView；当前项目使用 std::span 表达借用的连续数组。
struct FRHITransitionCreateInfo
{
    FRHITransitionCreateInfo() = default;

    FRHITransitionCreateInfo(
        ERHIPipeline InSrcPipelines,
        ERHIPipeline InDstPipelines,
        ERHITransitionCreateFlags InFlags =
        ERHITransitionCreateFlags::None,
        std::span<const FRHITransitionInfo> InTransitionInfos = {})
        : SrcPipelines(InSrcPipelines)
        , DstPipelines(InDstPipelines)
        , Flags(InFlags)
        , TransitionInfos(InTransitionInfos)
    {}

    // 转换前使用资源的逻辑管线。
    ERHIPipeline SrcPipelines = ERHIPipeline::None;

    // 转换后使用资源的逻辑管线。
    ERHIPipeline DstPipelines = ERHIPipeline::None;

    // 作用于整次转换的创建选项。
    ERHITransitionCreateFlags Flags =
        ERHITransitionCreateFlags::None;

    // 借用资源转换描述数组，不拥有数组，也不延长资源生命周期。
    std::span<const FRHITransitionInfo> TransitionInfos;

    //UE 还包含 transient 资源的别名转换描述 AliasingInfos。我们目前没有资源内存复用系统，暂时省略这一部分
};

class FDynamicRHI;
struct FRHITransition;
// UE 同名创建入口：RHI 分配公共头与尾部存储，后端构造私有数据。
RHIMODULE FRHITransition* RHICreateTransition(const FRHITransitionCreateInfo& CreateInfo);
// 教学版显式释放入口；对应 UE Bypass 的手动释放，另有多管线标记完成后 Cleanup 的路径。
// 这里只释放 CPU 转换描述，不代表其引用的 GPU 资源已完成使用。
RHIMODULE void RHIReleaseTransition(FRHITransition* Transition) noexcept;
extern RHIMODULE uint64 GRHITransitionPrivateData_SizeInBytes;
extern RHIMODULE uint64 GRHITransitionPrivateData_AlignInBytes;

// 不透明对象：RHI 不包含 D3D12 类型，也不把后端数据做成它的派生类。
// 一次分配：[FRHITransition][对齐填充][后端私有数据]。
struct RHIMODULE FRHITransition
{
    FRHITransition(const FRHITransition&) = delete;
    FRHITransition& operator=(const FRHITransition&) = delete;
    FRHITransition(FRHITransition&&) = delete;
    FRHITransition& operator=(FRHITransition&&) = delete;

    template<typename T> T* GetPrivateData()
    {
        if (sizeof(T) != PrivateSize || alignof(T) != PrivateAlignment)
            throw std::logic_error("Transition private data layout mismatch");
        return reinterpret_cast<T*>(reinterpret_cast<std::byte*>(this) + PrivateOffset);
    }
    template<typename T> const T* GetPrivateData() const
    {
        return const_cast<FRHITransition*>(this)->GetPrivateData<T>();
    }

private:
    FRHITransition() = default;
    ~FRHITransition() = default;
    friend RHIMODULE FRHITransition* RHICreateTransition(const FRHITransitionCreateInfo&);
    friend RHIMODULE void RHIReleaseTransition(FRHITransition*) noexcept;
    FDynamicRHI* Owner = nullptr; // 转换对象必须在 RHIExit 前释放。
    size_t PrivateOffset = 0;
    size_t PrivateSize = 0;
    size_t PrivateAlignment = 0;
    size_t AllocationAlignment = 0;
};

/*
* 示意：
* FRHITransitionInfo Infos[] =
{
    FRHITransitionInfo(
        OffscreenTexture,
        ERHIAccess::RTV,
        ERHIAccess::SRVGraphicsPixel),

    FRHITransitionInfo(
        BackBuffer,
        ERHIAccess::Present,
        ERHIAccess::RTV),
};

FRHITransitionCreateInfo CreateInfo(
    ERHIPipeline::Graphics,
    ERHIPipeline::Graphics,
    ERHITransitionCreateFlags::None,
    std::span<const FRHITransitionInfo>(Infos));
    它描述一次批量转换：离屏纹理准备被采样，backbuffer 准备接收接下来的绘制，CreateInfo 仍然只是参数。
*/
