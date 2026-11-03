import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 原生视口与 UI 分区布局，避免让普通 QML 控件覆盖 HWND。
ApplicationWindow {
    id: root
    objectName: "editorWindow"
    width: 1440; height: 920
    minimumWidth: 1100; minimumHeight: 720
    visible: true
    title: "BaseTest Editor — Render Study"
    color: theme.base
    font.family: "Segoe UI"
    font.pixelSize: 13
    property bool showInspector: true
    property bool showAssets: true
    Theme { id: theme }

    component EditorButton: Button {
        id: control
        implicitHeight: 30
        implicitWidth: Math.max(76, implicitContentWidth + 24)
        hoverEnabled: true
        padding: 8
        contentItem: Text {
            text: control.text; font: control.font
            color: !control.enabled ? "#52606e" : control.checked ? theme.accent : theme.text
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 5
            color: control.checked ? "#203c36" : control.down ? "#36424d" : control.hovered ? "#303943" : theme.raised
            border.color: control.checked ? "#477c6c" : "#35404a"
        }
    }
    component Caption: Label {
        color: theme.muted; font.pixelSize: 10; font.letterSpacing: 1.5; font.weight: Font.DemiBold
    }
    component Divider: Rectangle {
        color: theme.line; implicitHeight: 1; Layout.fillWidth: true
    }

    header: Rectangle {
        height: 62; color: theme.panel
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: theme.line }
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 20; anchors.rightMargin: 20; spacing: 12
            Rectangle {
                width: 32; height: 32; radius: 8; color: theme.accent
                Label { anchors.centerIn: parent; text: "B"; color: "#152b25"; font.pixelSize: 21; font.bold: true }
            }
            ColumnLayout {
                spacing: 1
                Label { text: "BASETEST"; color: theme.text; font.pixelSize: 14; font.weight: Font.DemiBold; font.letterSpacing: 2 }
                Label { text: "ENGINE EDITOR"; color: theme.muted; font.pixelSize: 9; font.letterSpacing: 1.5 }
            }
            Rectangle { Layout.leftMargin: 12; Layout.rightMargin: 8; width: 1; height: 25; color: theme.line }
            Label { text: "Render Study"; color: theme.text }
            Label { text: "/"; color: "#52606e" }
            Label { text: "Demo Scene"; color: theme.muted }
            Item { Layout.fillWidth: true }
            EditorButton { text: "Inspector"; checked: root.showInspector; onClicked: root.showInspector = !root.showInspector }
            EditorButton { text: "Assets"; checked: root.showAssets; onClicked: root.showAssets = !root.showAssets }
            EditorButton { text: "Open logs"; onClicked: editor.openLogsFolder() }
        }
    }

    ColumnLayout {
        anchors.fill: parent; spacing: 0
        SplitView {
            objectName: "workspaceSplit"
            Layout.fillWidth: true; Layout.fillHeight: true; orientation: Qt.Horizontal
            handle: Rectangle {
                implicitWidth: 5
                color: SplitHandle.pressed ? theme.accent : SplitHandle.hovered ? "#4b5c69" : theme.base
            }
            Rectangle {
                SplitView.preferredWidth: 218; SplitView.minimumWidth: 170; SplitView.maximumWidth: 340
                color: theme.panel
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 16; spacing: 16
                    RowLayout {
                        Layout.fillWidth: true
                        Caption { text: "WORLD OUTLINER" }
                        Item { Layout.fillWidth: true }
                        Label { text: "01"; color: theme.muted; font.pixelSize: 11 }
                    }
                    Divider { }
                    Label { text: "⌄   Demo Scene"; color: theme.muted; font.pixelSize: 12 }
                    Rectangle {
                        Layout.fillWidth: true; height: 38; radius: 5; color: "#243b36"; border.color: "#3e6458"
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 10; spacing: 8
                            Label { text: "◇"; color: theme.accent; font.pixelSize: 19 }
                            Label { text: "Checker Cube"; color: theme.text; font.pixelSize: 12 }
                            Item { Layout.fillWidth: true }
                            Rectangle { width: 5; height: 5; radius: 3; color: theme.accent }
                        }
                    }
                    Item { Layout.fillHeight: true }
                    Divider { }
                    Caption { text: "WORKSPACE" }
                    Label { text: "Render Study"; color: theme.text; font.pixelSize: 12 }
                    Label { Layout.fillWidth: true; text: "A small scene for exploring\nyour renderer."; color: theme.muted; font.pixelSize: 11; lineHeight: 1.5 }
                }
            }

            SplitView {
                orientation: Qt.Vertical; SplitView.fillWidth: true; SplitView.minimumWidth: 440
                handle: Rectangle {
                    implicitHeight: 5
                    color: SplitHandle.pressed ? theme.accent : SplitHandle.hovered ? "#4b5c69" : theme.base
                }
                Rectangle {
                    SplitView.fillHeight: true; SplitView.minimumHeight: 280; color: theme.base
                    ColumnLayout {
                        anchors.fill: parent; spacing: 0
                        Rectangle {
                            Layout.fillWidth: true; height: 46; color: theme.panel
                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 12; spacing: 10
                                Caption { text: "SCENE VIEW"; color: theme.text }
                                Rectangle {
                                    width: 45; height: 20; radius: 4; color: "#27333b"
                                    Label { anchors.centerIn: parent; text: "DX12"; color: "#8eb4ce"; font.pixelSize: 10; font.bold: true }
                                }
                                Item { Layout.fillWidth: true }
                                EditorButton {
                                    text: editor.animating ? "Pause" : "Resume"; implicitWidth: 74; enabled: editor.ready
                                    onClicked: editor.animating = !editor.animating
                                }
                                EditorButton { text: "Swap texture"; enabled: editor.ready; onClicked: editor.selectTexture(1 - editor.textureIndex) }
                            }
                        }
                        Item {
                            Layout.fillWidth: true; Layout.fillHeight: true
                            Column {
                                anchors.centerIn: parent; width: parent.width - 64; spacing: 12
                                Label {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: editor.errorMessage.length ? "Viewport unavailable" : "Preparing your scene…"
                                    color: theme.text; font.pixelSize: 18
                                }
                                Label { width: parent.width; text: editor.errorMessage; color: "#e39f9f"; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter }
                            }
                            WindowContainer {
                                objectName: "engineViewportContainer"
                                anchors.fill: parent
                                visible: editor.errorMessage.length === 0
                                window: editor.renderWindow
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true; height: 30; color: "#1a2027"
                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 14
                                Rectangle { width: 5; height: 5; radius: 3; color: editor.ready ? theme.accent : theme.muted }
                                Label { text: editor.ready ? (editor.animating ? "LIVE" : "PAUSED") : "STARTING"; color: theme.muted; font.pixelSize: 10; font.letterSpacing: 1 }
                                Item { Layout.fillWidth: true }
                                Label { text: editor.renderSize + " px"; color: theme.muted; font.pixelSize: 10 }
                                Label { Layout.leftMargin: 14; text: editor.fps + " FPS"; color: theme.text; font.pixelSize: 10 }
                            }
                        }
                    }
                }
                Rectangle {
                    visible: root.showAssets
                    SplitView.preferredHeight: 208; SplitView.minimumHeight: 170; color: theme.panel
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 16; spacing: 12
                        RowLayout {
                            Layout.fillWidth: true
                            Caption { text: "ASSET LIBRARY"; color: theme.text }
                            Label { Layout.leftMargin: 12; text: "Built-in / Textures"; color: theme.muted; font.pixelSize: 11 }
                            Item { Layout.fillWidth: true }
                            Label { text: "02 assets"; color: theme.muted; font.pixelSize: 10 }
                        }
                        RowLayout {
                            spacing: 12
                            Repeater {
                                model: 2
                                delegate: Rectangle {
                                    id: assetCard
                                    required property int index
                                    width: 136; height: 125; radius: 6; color: theme.raised
                                    border.color: editor.textureIndex === index ? "#5b9c86" : theme.line
                                    Column {
                                        anchors.fill: parent; anchors.margins: 9; spacing: 7
                                        Rectangle {
                                            width: parent.width; height: 66; radius: 3; clip: true; color: "#151a20"
                                            Grid {
                                                anchors.centerIn: parent; columns: 8; spacing: 0
                                                Repeater {
                                                    model: 32
                                                    delegate: Rectangle {
                                                        required property int index
                                                        width: 15; height: 17
                                                        color: ((Math.floor(index / 8) + index % 8) % 2) ? (assetCard.index === 0 ? "#e1e3df" : "#ffff00") : (assetCard.index === 0 ? "#4b5055" : "#ff00ff")
                                                    }
                                                }
                                            }
                                        }
                                        Label { text: assetCard.index === 0 ? "Checker / Neutral" : "Checker / Color"; color: theme.text; font.pixelSize: 11 }
                                        Label { text: "TEXTURE   ·   256 × 256"; color: theme.muted; font.pixelSize: 9 }
                                    }
                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: editor.selectTexture(assetCard.index) }
                                }
                            }
                            Item { Layout.fillWidth: true }
                        }
                    }
                }
            }

            Rectangle {
                visible: root.showInspector
                SplitView.preferredWidth: 264; SplitView.minimumWidth: 230; SplitView.maximumWidth: 360
                color: theme.panel
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 18; spacing: 16
                    Caption { text: "INSPECTOR" }
                    Divider { }
                    RowLayout {
                        Rectangle {
                            width: 36; height: 36; radius: 7; color: "#283d38"
                            Label { anchors.centerIn: parent; text: "◇"; color: theme.accent; font.pixelSize: 24 }
                        }
                        ColumnLayout {
                            spacing: 3
                            Label { text: "Checker Cube"; color: theme.text; font.weight: Font.DemiBold }
                            Label { text: "Demo mesh"; color: theme.muted; font.pixelSize: 11 }
                        }
                    }
                    Divider { }
                    Caption { text: "GEOMETRY" }
                    Repeater {
                        model: [{label: "Vertices", value: "24"}, {label: "Triangles", value: "12"}, {label: "Material", value: "Checker"}]
                        delegate: RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            Label { text: modelData.label; color: theme.muted; font.pixelSize: 12 }
                            Item { Layout.fillWidth: true }
                            Label { text: modelData.value; color: theme.text; font.pixelSize: 12 }
                        }
                    }
                    Divider { }
                    Caption { text: "PREVIEW" }
                    EditorButton {
                        Layout.fillWidth: true
                        text: editor.animating ? "Rotation enabled" : "Rotation paused"
                        checked: editor.animating; enabled: editor.ready
                        onClicked: editor.animating = !editor.animating
                    }
                    Label { Layout.fillWidth: true; text: "Select a texture in the library, or press Space in the viewport to swap."; color: theme.muted; wrapMode: Text.Wrap; font.pixelSize: 11; lineHeight: 1.4 }
                    Item { Layout.fillHeight: true }
                    Divider { }
                    Caption { text: "COMING NEXT" }
                    Label { Layout.fillWidth: true; text: "Model and texture import"; color: theme.text; font.pixelSize: 12 }
                    Label { Layout.fillWidth: true; text: "The asset workspace is ready for your import pipeline."; color: theme.muted; wrapMode: Text.Wrap; font.pixelSize: 11; lineHeight: 1.4 }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true; height: 30; color: "#20272f"
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 18; spacing: 8
                Label { text: "BASETEST"; color: theme.accent; font.pixelSize: 9; font.letterSpacing: 1.5; font.bold: true }
                Label { text: "Render Study"; color: theme.muted; font.pixelSize: 10 }
                Item { Layout.fillWidth: true }
                Label { text: editor.ready ? "Renderer connected" : "Waiting for renderer"; color: theme.muted; font.pixelSize: 10 }
                Rectangle { Layout.leftMargin: 8; Layout.rightMargin: 8; width: 1; height: 12; color: theme.line }
                Label { text: "D3D12  /  Qt Quick"; color: theme.muted; font.pixelSize: 10 }
            }
        }
    }
}
