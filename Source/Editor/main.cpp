#include "NativeViewportWindow.h"
#include "Logging/Log.h"
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QPointer>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>
#include <atomic>
#include <fstream>

namespace
{
std::atomic<int> QtWarnings{0};
void QtLogHandler(QtMsgType Type, const QMessageLogContext& Context, const QString& Message)
{
    using CoreLog::ELogVerbosity;
    const auto Level = Type == QtFatalMsg ? ELogVerbosity::Fatal : Type == QtCriticalMsg ? ELogVerbosity::Error
        : Type == QtWarningMsg ? ELogVerbosity::Warning : Type == QtDebugMsg ? ELogVerbosity::Verbose : ELogVerbosity::Log;
    if (Type == QtWarningMsg || Type == QtCriticalMsg) ++QtWarnings;
    if (CoreLog::IsEnabled(LogCore, Level))
        CoreLog::WriteText(LogCore, Level, {Context.file ? Context.file : "Qt", Context.line,
                           Context.function ? Context.function : "Qt"}, Message.toUtf8().toStdString());
}
}

int main(int Argc, char** Argv)
{
    // Qt Quick 只画编辑器 UI；自研 D3D12 继续拥有中央窗口的设备/交换链。
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication App(Argc, Argv);
    QCoreApplication::setApplicationName(QStringLiteral("BaseTestEditor"));
    QCoreApplication::setOrganizationName(QStringLiteral("BaseTestEngine"));
    QCommandLineParser Parser;
    Parser.addHelpOption();
    Parser.addOption({"smoke-test", "Run the viewport integration checks and exit."});
    Parser.addOption({"smoke-output", "Directory for the integration report and GPU readback.", "directory", "editor-smoke"});
    Parser.process(App);
    const bool Smoke = Parser.isSet("smoke-test");
    const QString OutputDirectory = QDir(Parser.value("smoke-output")).absolutePath();
    // 从任意工作目录启动都能找到部署的插件；日志统一位于 exe 同级 Logs。
    QDir::setCurrent(QCoreApplication::applicationDirPath());
    CoreLog::FLogOptions LogOptions;
    LogOptions.FilePath = "Logs/Editor.log";
    LogOptions.Console = false;
    if (!CoreLog::Initialize(LogOptions)) return 1;
    qInstallMessageHandler(QtLogHandler);
    CORE_LOG(LogCore, Log, "BaseTestEditor starting with Qt {}", qVersion());

    int Result = 0;
    QPointer<NativeViewportWindow> Viewport = new NativeViewportWindow(Smoke);
    {
        QQmlApplicationEngine Engine;
        QQmlEngine::setObjectOwnership(Viewport, QQmlEngine::CppOwnership);
        Engine.rootContext()->setContextProperty(QStringLiteral("editor"), Viewport.data());
        Engine.load(QUrl(QStringLiteral("qrc:/editor/qml/Main.qml")));
        if (Engine.rootObjects().isEmpty()) Result = 1;
        else
        {
            auto* Window = qobject_cast<QQuickWindow*>(Engine.rootObjects().first());
            // 关闭顶层窗口前由宿主主动释放 GPU 资源，不能留到 HWND 消失之后。
            QObject::connect(&App, &QCoreApplication::aboutToQuit, Viewport, &NativeViewportWindow::shutdown);

            if (Smoke)
            {
                // 验证脚本隐藏启动进程；第一次 ShowWindow 可能遵循 STARTUPINFO 的 SW_HIDE。
                // 再显式 hide/show，让真实 HWND 暴露后才由帧事件推进验证。
                Window->hide();
                Window->show();
                QDir().mkpath(OutputDirectory);
                auto Finish = [&, Window](int Code, const std::string& Message)
                {
                    std::ofstream Report((OutputDirectory + "/editor-smoke.txt").toStdString());
                    Report << (Code == 0 ? "PASS: " : "FAIL: ") << Message << '\n';
                    if (Viewport) Viewport->shutdown();
                    QCoreApplication::exit(Code);
                };
                // 不使用固定启动延迟：由实际成功呈现的帧推进验证。
                auto Stage = std::make_shared<int>(0);
                auto OriginalSize = std::make_shared<QString>();
                auto ReducedSize = std::make_shared<QString>();
                QObject::connect(Viewport, &NativeViewportWindow::frameRendered, &App, [&, Stage, OriginalSize, ReducedSize, Finish, Window]
                {
                    try
                    {
                        const auto Count = Viewport->frameCount();
                        if (*Stage == 0 && Count >= 4)
                        {
                            *OriginalSize = Viewport->renderSize();
                            Viewport->setAnimating(false);
                            Window->resize(1200, 780);
                            *Stage = 1;
                        }
                        else if (*Stage == 1 && Count >= 10)
                        {
                            *ReducedSize = Viewport->renderSize();
                            if (*OriginalSize == *ReducedSize) throw std::runtime_error("Native viewport did not resize");
                            if (Viewport->animating()) throw std::runtime_error("Pause state did not apply");
                            Viewport->selectTexture(1);
                            // 验证显隐面板导致的 HWND 重新布局与交换链调整。
                            Window->setProperty("showInspector", false);
                            Window->setProperty("showAssets", false);
                            *Stage = 2;
                        }
                        else if (*Stage == 2 && Count >= 16)
                        {
                            if (Viewport->renderSize() == *ReducedSize) throw std::runtime_error("Panel visibility did not resize the viewport");
                            Window->setProperty("showInspector", true);
                            Window->setProperty("showAssets", true);
                            Window->resize(1440, 920);
                            Viewport->setAnimating(true);
                            *Stage = 3;
                        }
                        else if (*Stage == 3 && Count >= 24)
                        {
                            Viewport->validateFrame((OutputDirectory + "/viewport.ppm").toStdString().c_str());
                            // 顶层截图用于验证 QML 外壳（原生子窗口可能需桌面截图才能合成）。
                            Window->grabWindow().save(OutputDirectory + "/qml-shell.png");
                            // 桌面可捕获时保存包含 HWND 子窗口的合成截图；失败不替代 GPU 回读验证。
                            if (Window->screen()) Window->screen()->grabWindow(Window->winId()).save(OutputDirectory + "/editor-window.png");
                            if (QtWarnings.load()) throw std::runtime_error("Qt/QML emitted warnings; inspect Logs/Editor.log");
                            *Stage = 4;
                            Finish(0, "QML load, native D3D12 frames, resize, panel layout, pause/resume, texture swap, GPU image and clean debug queue");
                        }
                    }
                    catch (const std::exception& Error) { *Stage = 4; Finish(1, Error.what()); }
                });
                QTimer::singleShot(30000, &App, [Finish] { Finish(1, "Editor integration test timed out"); });
            }
            Result = App.exec();
            if (Viewport)
            {
                Viewport->shutdown();
                if (!Viewport->errorMessage().isEmpty()) Result = 1;
            }
        }
    } // 先释放 QML 宿主；若 WindowContainer 销毁了子窗口，QPointer 自动清空。
    if (Viewport) delete Viewport.data();
    qInstallMessageHandler(nullptr);
    CoreLog::Shutdown();
    return Result;
}
