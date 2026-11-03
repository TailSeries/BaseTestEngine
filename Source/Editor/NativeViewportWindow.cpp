#include "NativeViewportWindow.h"
#include <QDesktopServices>
#include <QDir>
#include <QKeyEvent>
#include <QPlatformSurfaceEvent>
#include <QUrl>
#include "RHITestPeriod1.h"
#include "TestPlatform.h"
#include "Logging/Log.h"
// 引擎的 verify 是语句宏，Qt 容器也有同名成员；在 Qt 桥接 TU 中隔离它。
#undef verify
#include <Windows.h>
#include <algorithm>

NativeViewportWindow::NativeViewportWindow(bool InValidation) : Validation(InValidation)
{
    setTitle(QStringLiteral("Engine Viewport"));
    // 不创建 Qt backing store，也不让 Qt 的渲染器绘制这个原生窗口。
    setSurfaceType(QSurface::RasterSurface);
    FrameTimer.setTimerType(Qt::PreciseTimer);
    FrameTimer.setInterval(16);
    connect(&FrameTimer, &QTimer::timeout, this, &NativeViewportWindow::tick);
}

NativeViewportWindow::~NativeViewportWindow() { shutdown(); }

QSize NativeViewportWindow::nativePixelSize() const
{
    // Qt 布局使用逻辑像素；DXGI 使用物理像素。直接查询 HWND，涵盖分数缩放及跨屏 DPI。
    RECT Client{};
    if (!handle() || !GetClientRect(reinterpret_cast<HWND>(winId()), &Client)) return {};
    return {Client.right - Client.left, Client.bottom - Client.top};
}

QString NativeViewportWindow::renderSize() const
{
    return QStringLiteral("%1 × %2").arg(RenderPixels.width()).arg(RenderPixels.height());
}

void NativeViewportWindow::exposeEvent(QExposeEvent*)
{
    if (!Closing && ErrorMessage.isEmpty() && isExposed())
    {
        // 暂停/最小化后不累计一大段动画时间。
        DeltaTimer.start();
        if (!StatisticsTimer.isValid()) StatisticsTimer.start();
        FrameTimer.start();
    }
    else FrameTimer.stop();
}

void NativeViewportWindow::resizeEvent(QResizeEvent*) { ResizePending = true; }

bool NativeViewportWindow::event(QEvent* Event)
{
    if (Event->type() == QEvent::DevicePixelRatioChange) ResizePending = true;
    if (Event->type() == QEvent::PlatformSurface)
    {
        auto* SurfaceEvent = static_cast<QPlatformSurfaceEvent*>(Event);
        if (SurfaceEvent->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
            shutdown(); // HWND 失效前先销毁 swap chain。
    }
    return QWindow::event(Event);
}

void NativeViewportWindow::keyPressEvent(QKeyEvent* Event)
{
    if (Event->key() == Qt::Key_Space && !Event->isAutoRepeat())
    {
        selectTexture(1 - TextureIndex);
        Event->accept();
        return;
    }
    QWindow::keyPressEvent(Event);
}

void NativeViewportWindow::tick()
{
    if (Closing || !isExposed() || !ErrorMessage.isEmpty()) return;
    try
    {
        const QSize Pixels = nativePixelSize();
        if (Pixels.isEmpty()) return; // 最小化/零尺寸不调用 ResizeBuffers。
        if (!Renderer)
        {
            RHIInit(CreateTestRHI(Validation));
            OwnsRHI = true;
            Renderer = std::make_unique<RHITestPeriod1>();
            Renderer->Initialize(reinterpret_cast<void*>(winId()), false, Pixels.width(), Pixels.height());
            Renderer->SetAnimating(Animating);
            RenderPixels = Pixels;
            ResizePending = false;
            CORE_LOG(LogCore, Log, "Editor D3D12 viewport initialized: {} x {}", Pixels.width(), Pixels.height());
            Q_EMIT statusChanged();
        }
        if (ResizePending || Pixels != RenderPixels)
        {
            Renderer->Resize(Pixels.width(), Pixels.height());
            RenderPixels = Pixels;
            ResizePending = false;
        }
        const float Delta = std::clamp(DeltaTimer.restart() / 1000.0f, 0.0f, 0.1f);
        Renderer->DrawTriangle(true, Delta);
        ++FrameCount;
        ++StatisticsFrames;
        if (StatisticsTimer.elapsed() >= 500)
        {
            FramesPerSecond = qRound(StatisticsFrames * 1000.0 / StatisticsTimer.restart());
            StatisticsFrames = 0;
            Q_EMIT statisticsChanged();
        }
        Q_EMIT frameRendered();
    }
    catch (const std::exception& Error) { fail(QString::fromUtf8(Error.what())); }
}

void NativeViewportWindow::setAnimating(bool Enabled)
{
    if (Animating == Enabled) return;
    Animating = Enabled;
    if (Renderer) Renderer->SetAnimating(Enabled);
    Q_EMIT settingsChanged();
}

void NativeViewportWindow::selectTexture(int Index)
{
    if (Index < 0 || Index > 1 || Index == TextureIndex || !ready()) return;
    Renderer->RequestTextureSwap();
    // 同帧多次切换以奇偶合并，下一次 Tick 才录制命令；QML 回调不直接执行渲染。
    TextureIndex = Index;
    Q_EMIT settingsChanged();
}

void NativeViewportWindow::openLogsFolder()
{
    QDir().mkpath(QStringLiteral("Logs"));
    QDesktopServices::openUrl(QUrl::fromLocalFile(QDir("Logs").absolutePath()));
}

void NativeViewportWindow::validateFrame(const char* OutputPath)
{
    if (!ready()) throw std::logic_error("Viewport is not ready");
    Renderer->DrawTriangle(false, 0.0f);
    Renderer->ValidateOutput(OutputPath);
    CheckDebugMessages();
}

void NativeViewportWindow::fail(const QString& Message) noexcept
{
    FrameTimer.stop();
    ErrorMessage = Message;
    CORE_LOG(LogCore, Error, "Editor viewport failed: {}", Message.toStdString());
    // 原生窗口隐藏后，QML 中央错误提示才能显示出来。
    hide();
    Q_EMIT statusChanged();
}

void NativeViewportWindow::shutdown() noexcept
{
    if (Closing) return;
    Closing = true;
    FrameTimer.stop();
    try
    {
        if (Renderer) Renderer->WaitForGPU();
        Renderer.reset();
        if (OwnsRHI)
        {
            if (Validation) CheckDebugMessages();
            RHIExit();
            OwnsRHI = false;
        }
    }
    catch (const std::exception& Error)
    {
        CORE_LOG(LogCore, Error, "Editor shutdown failed: {}", Error.what());
        ErrorMessage = QString::fromUtf8(Error.what());
    }
}
