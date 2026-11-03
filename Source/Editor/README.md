# Qt Quick 编辑器

编辑器使用 Qt 6.8+ / QML，中央视口由现有 D3D12RHI + Renderer 绘制。
已验证 SDK 为 Qt 6.8.3 MSVC 2022 x64；项目使用 MSVC x64 工具链。
Qt SDK 属于构建依赖，不提交到仓库；只编译原有引擎目标时不需要 Qt。

## 构建和运行

在仓库根目录执行：

```powershell
# 首次安装 SDK（约数百 MB 下载，解压占用更多空间），保存到 out/Qt：
powershell -ExecutionPolicy Bypass -File Source/Editor/SetupQt.ps1

# 编译并运行：
powershell -ExecutionPolicy Bypass -File Source/Editor/BuildEditor.ps1 -Configuration Debug -Run

# 独立集成验证（需要可用桌面会话、D3D12 显卡及 Windows Graphics Tools 调试层）：
powershell -ExecutionPolicy Bypass -File Source/Editor/BuildEditor.ps1 -Configuration Debug -Validate
```

已有 Qt 安装时传 `-QtRoot C:/Qt/6.8.3/msvc2022_64`。需要 Qt Quick、Quick Controls、
Shader Tools；不要使用 MinGW SDK 链接当前的 MSVC 引擎 DLL。

也可以直接使用 CMake：

```powershell
cmake -S Source -B out/build/qt-editor -A x64 -DBASETEST_BUILD_EDITOR=ON -DCMAKE_PREFIX_PATH=F:/workspace/BaseTestEngine/out/Qt/6.8.3/msvc2022_64
cmake --build out/build/qt-editor --config Debug --target BaseTestEditor RHITest -j 8
& ./out/bin/Debug/BaseTestEditor.exe
```

构建后自动运行 `windeployqt`，部署 Qt DLL、平台插件和 QML imports 到 exe 同级目录。
不需要把 Qt/bin 加进系统 PATH。默认 `BASETEST_BUILD_EDITOR=OFF`，Qt 只链接到 Editor。
日志固定写入 exe 同级的 `Logs/Editor.log`，QML/Qt 警告也会进入 Core 的日志系统。

## 结构与职责

- `qml/Main.qml`：顶部工具栏、可拖动分隔条、场景列表、中央视口、属性面板和内置纹理列表。
- `qml/Theme.qml`：颜色主题，方便后续扩展美术风格。
- `NativeViewportWindow`：Qt QWindow / HWND 与引擎之间的桥接、Tick、物理像素尺寸、错误处理及关闭。
- `RHITestSupport`：原 `RHITestPeriod1` 抽出的可复用示例模块；Win32 RHITest 和编辑器使用同一份绘制代码。
- `RHIResizeViewport`：帧外等待 GPU，调整交换链；示例随后重建深度与 SceneColor，PSO/网格保留。

Qt Quick 通过 D3D11 绘制 UI；中央子 HWND 的 DXGI swap chain 仍由自研 D3D12 管理。
没有将 QML 当作 3D 渲染器，也没有让两套后端共享设备或描述符堆。
本阶段沿用 RHI 的单线程契约，在 GUI 线程 Tick 中录制并 Present；Qt Quick 可能有独立的场景图线程。
重建尺寸和 Present 可能等待 GPU，因此不是完整的 UE 式渲染线程架构。
后续要迁到渲染线程，应使用线程拥有的命令队列传递尺寸/输入/关闭请求，不可从两条线程操作即时命令列表。

窗口最小化/未暴露时停止绘制，忽略零尺寸。Qt 使用逻辑像素，交换链尺寸通过 HWND 客户区查询物理像素。
退出顺序为：停止定时器 → 等待 GPU → 释放示例资源及交换链 → RHIExit → 销毁 Qt 窗口。

## 现有操作及扩展方向

- Pause / Resume 暂停与恢复旋转。
- Swap texture、点击纹理卡片或视口中的空格键切换两张预上传纹理。
- Inspector / Assets 控制面板显隐；分隔条可以拖动。
- Open logs 打开日志目录。
- 场景列表和几何信息目前对应固定的示例立方体；模型/贴图导入尚未实现。

Qt Quick / QML 适合自定义主题、动画和统一视觉。若以后需要任意浮动停靠面板，
需要另外实现或集成 docking 方案，SplitView 只是可调布局。

当前使用 Qt 6.8 引入的 `WindowContainer`。嵌入的原生窗口位于普通 QML 内容之上，
因此不支持在视口上直接叠普通 QML gizmo、半透明按钮、圆角裁剪或变换。
工具栏、错误提示和状态条已按这个限制布置。需要视口内叠加时，先选择独立原生浮层，
或改成离屏渲染 + Qt Quick 纹理合成；后者还需要处理设备/适配器匹配、资源共享和 Fence 同步。

## 验证

`--smoke-test --smoke-output <目录>` 在实际成功绘制后推进检查：
窗口缩放、面板显隐、暂停/恢复、纹理切换、GPU 回读、离屏/合成像素一致及 D3D12 调试消息。
输出 `editor-smoke.txt`、`viewport.ppm`、`qml-shell.png`；Qt 顶层截图不一定包含原生子窗口，
桌面可捕获时另外输出 `editor-window.png`。该验证不替代多显示器 DPI 与长时间交互测试。

原有回归仍可执行 `Source/RHITest/RunStageCValidation.ps1`。

参考：
- https://doc.qt.io/qt-6/qml-qtquick-windowcontainer.html
- https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html
- https://aqtinstall.readthedocs.io/en/latest/cli.html

Qt 开源 SDK 动态链接使用；发布程序时保留所用 Qt 模块的许可声明，并按其许可条款部署。
