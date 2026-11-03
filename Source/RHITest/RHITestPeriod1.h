#pragma once
#include <cstdint>
#include <memory>

// 同一个两 Pass 示例供 Win32 RHITest 与 Qt 编辑器调用；不依赖 Qt。
// 所有方法在创建它的线程上调用；RHI 由宿主先启动、最后关闭。
class RHITestPeriod1
{
public:
    RHITestPeriod1();
    ~RHITestPeriod1();
    RHITestPeriod1(const RHITestPeriod1&) = delete;
    RHITestPeriod1& operator=(const RHITestPeriod1&) = delete;

    // Window 为宿主拥有的 HWND，必须比本对象活得更久；尺寸单位为物理像素。
    void Initialize(void* Window, bool Validate = false,
                    std::uint32_t Width = 1920, std::uint32_t Height = 1080);
    void DrawTriangle(bool Present = true, float DeltaSeconds = 1.0f / 60.0f);
    // 帧外同步重建 backbuffer / SceneColor / Depth，调整窗口时可能等待 GPU。
    void Resize(std::uint32_t Width, std::uint32_t Height);
    void RequestTextureSwap();
    void SetAnimating(bool Enabled);
    void WaitForGPU();
    void Validate();
    // 等待并回读当前帧，验证离屏与合成结果；仅供宿主集成验证使用。
    void ValidateOutput(const char* OutputPath);
private:
    struct FImplementation;
    std::unique_ptr<FImplementation> Implementation;
};
