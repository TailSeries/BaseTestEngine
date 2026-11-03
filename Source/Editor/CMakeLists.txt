if(NOT WIN32)
    message(FATAL_ERROR "The editor viewport currently requires Windows / D3D12.")
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(BaseTestEditor WIN32
    main.cpp
    NativeViewportWindow.h
    NativeViewportWindow.cpp
)
target_link_libraries(BaseTestEditor PRIVATE RHITestSupport Qt6::Quick Qt6::QuickControls2)
# Qt 的 slots / signals 宏不能泄漏到引擎头文件；QObject 使用 Q_SIGNALS / Q_SLOTS。
target_compile_definitions(BaseTestEditor PRIVATE QT_NO_KEYWORDS)
qt_add_resources(BaseTestEditor EditorQml PREFIX "/editor" FILES
    qml/Main.qml
    qml/Theme.qml
)

# 和 Core / RHI DLL 放在同一目录；同时部署 QML imports、Qt DLL 和平台插件。
# 构建后自动部署；不要求运行时配置系统 PATH。
get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
find_program(QT_WINDEPLOYQT windeployqt HINTS "${QT_BIN_DIR}" REQUIRED NO_DEFAULT_PATH)
add_custom_command(TARGET BaseTestEditor POST_BUILD
    COMMAND "${QT_WINDEPLOYQT}" "$<TARGET_FILE:BaseTestEditor>"
            --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml" --no-translations --no-compiler-runtime
            "$<IF:$<CONFIG:Debug>,--debug,--release>"
    COMMENT "Deploying Qt runtime and QML modules next to BaseTestEditor"
    VERBATIM
)
