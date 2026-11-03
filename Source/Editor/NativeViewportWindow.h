#pragma once
#include <QElapsedTimer>
#include <QTimer>
#include <QWindow>
#include <memory>

class RHITestPeriod1;

// QML 只负责布局；本窗口的 HWND 交给自研 RHI 创建 DXGI swap chain。
// 第一版沿用示例的单线程 immediate RHI：GUI 线程拥有全部引擎资源。
// Qt Quick 的场景图线程使用独立的 D3D11 设备，不能访问这些 D3D12 资源。
class NativeViewportWindow final : public QWindow
{
    Q_OBJECT
    Q_PROPERTY(QWindow* renderWindow READ renderWindow CONSTANT)
    Q_PROPERTY(bool animating READ animating WRITE setAnimating NOTIFY settingsChanged)
    Q_PROPERTY(int textureIndex READ textureIndex NOTIFY settingsChanged)
    Q_PROPERTY(int fps READ fps NOTIFY statisticsChanged)
    Q_PROPERTY(qulonglong frameCount READ frameCount NOTIFY statisticsChanged)
    Q_PROPERTY(QString renderSize READ renderSize NOTIFY statisticsChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY statusChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY statusChanged)
public:
    explicit NativeViewportWindow(bool Validation = false);
    ~NativeViewportWindow() override;
    QWindow* renderWindow() { return this; }
    bool animating() const { return Animating; }
    int textureIndex() const { return TextureIndex; }
    int fps() const { return FramesPerSecond; }
    qulonglong frameCount() const { return FrameCount; }
    QString renderSize() const;
    QString errorMessage() const { return ErrorMessage; }
    bool ready() const { return Renderer != nullptr && ErrorMessage.isEmpty(); }

    void setAnimating(bool Enabled);
    Q_INVOKABLE void selectTexture(int Index);
    Q_INVOKABLE void openLogsFolder();
    // 必须在 QML 宿主销毁之前调用；先停 Tick、等待 GPU、释放示例，最后 RHIExit。
    void shutdown() noexcept;
    // 验证入口：帧外同步回读离屏与 backbuffer；不在正常编辑器循环使用。
    void validateFrame(const char* OutputPath);

Q_SIGNALS:
    void settingsChanged();
    void statisticsChanged();
    void statusChanged();
    void frameRendered();

protected:
    void exposeEvent(QExposeEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    bool event(QEvent*) override;
private:
    void tick();
    void fail(const QString& Message) noexcept;
    QSize nativePixelSize() const;

    std::unique_ptr<RHITestPeriod1> Renderer;
    QTimer FrameTimer;
    QElapsedTimer DeltaTimer, StatisticsTimer;
    QSize RenderPixels;
    QString ErrorMessage;
    qulonglong FrameCount = 0;
    int StatisticsFrames = 0;
    int FramesPerSecond = 0;
    int TextureIndex = 0;
    bool Animating = true;
    bool Validation = false;
    bool OwnsRHI = false;
    bool Closing = false;
    bool ResizePending = true;
};
