/**
 * @file ChatWindow.qml
 * @brief 聊天主窗口 — 无边框 + 自定义磨砂玻璃标题栏
 * @details 登录成功后由 main.cpp 创建，替代旧的 MainWindow.qml 中的 chatPage。
 *          使用 FramelessWindowHint 去除原生标题栏，QML 自绘半透明磨砂风格标题栏，
 *          包含拖拽、最小化/最大化/关闭按钮和边缘拖拽缩放。
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: chatWindow
    visible: true
    width: 920
    height: 660
    minimumWidth: 600
    minimumHeight: 500
    color: "transparent"
    title: "msrChat"
    flags: Qt.FramelessWindowHint | Qt.Window

    property bool isMaximizedState: false

    Component.onCompleted: {
        chatController.initialize()
        var buffered = authController.TakeBufferedMessages()
        if (buffered && buffered.length > 0) {
            chatController.drainBufferedMessages(buffered)
        }
    }

    // === 主布局：标题栏 + 聊天视图 ===
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── 自定义标题栏 — 半透明磨砂玻璃风格 ──
        Rectangle {
            id: titleBar
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: "#C8D8DDE8"

            // 底部分隔线
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: "#D8D7E0"
            }

            // 拖拽区域（标题栏左侧到控制按钮之间）
            MouseArea {
                anchors.left: parent.left
                anchors.right: windowControls.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                z: 1
                property point pressPos

                onPressed: pressPos = Qt.point(mouseX, mouseY)
                onPositionChanged: {
                    if (chatWindow.isMaximizedState) {
                        // 从最大化状态拖拽 → 还原窗口并居中到鼠标
                        var oldWidth = chatWindow.width
                        chatWindow.isMaximizedState = false
                        chatWindow.visibility = Window.Windowed
                        var ratio = mouseX / oldWidth
                        chatWindow.x = chatWindow.x + mouseX - chatWindow.width * ratio
                        chatWindow.y = chatWindow.y
                    }
                    var delta = Qt.point(mouseX - pressPos.x, mouseY - pressPos.y)
                    chatWindow.x += delta.x
                    chatWindow.y += delta.y
                }
                onDoubleClicked: maximizeBtn.clicked()
            }

            // 应用名称
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                text: "msrChat"
                color: "#4B4A5E"
                font.pixelSize: 13
                font.family: "Microsoft YaHei"
                font.weight: Font.DemiBold
            }

            // 窗口控制按钮 — 最小化 / 最大化 / 关闭
            Row {
                id: windowControls
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                spacing: 0

                Button {
                    id: minimizeBtn
                    width: 46; height: 36
                    flat: true
                    contentItem: Text {
                        text: "─"
                        color: minimizeBtn.hovered ? "#1A1A2E" : "#9C9AAA"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: minimizeBtn.hovered ? "#B8C4D0D8" : "transparent"
                    }
                    onClicked: chatWindow.showMinimized()
                }

                Button {
                    id: maximizeBtn
                    width: 46; height: 36
                    flat: true
                    contentItem: Text {
                        text: chatWindow.isMaximizedState ? "❐" : "☐"
                        color: maximizeBtn.hovered ? "#1A1A2E" : "#9C9AAA"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: maximizeBtn.hovered ? "#B8C4D0D8" : "transparent"
                    }
                    onClicked: {
                        if (chatWindow.isMaximizedState) {
                            chatWindow.isMaximizedState = false
                            chatWindow.visibility = Window.Windowed
                        } else {
                            chatWindow.isMaximizedState = true
                            chatWindow.visibility = Window.Maximized
                        }
                    }
                }

                Button {
                    id: closeBtn
                    width: 46; height: 36
                    flat: true
                    contentItem: Text {
                        text: "✕"
                        color: closeBtn.hovered ? "#FFFFFF" : "#9C9AAA"
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: closeBtn.hovered ? "#E81123" : "transparent"
                    }
                    onClicked: chatWindow.close()
                }
            }
        }

        // ── 聊天视图 ──
        ChatView {
            id: chatView
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }

    // === 边缘拖拽缩放（最大化时禁用）===
    property int _edgeSize: 6

    // 左边缘
    MouseArea {
        anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
        width: _edgeSize
        visible: !chatWindow.isMaximizedState
        cursorShape: Qt.SizeHorCursor; z: 100
        property real _smx; property real _sw; property real _sx
        onPressed: { _smx = mouseX; _sw = chatWindow.width; _sx = chatWindow.x }
        onPositionChanged: {
            var d = mouseX - _smx
            var nw = _sw - d
            if (nw >= chatWindow.minimumWidth) { chatWindow.x = _sx + d; chatWindow.width = nw }
        }
    }
    // 右边缘
    MouseArea {
        anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom
        width: _edgeSize
        visible: !chatWindow.isMaximizedState
        cursorShape: Qt.SizeHorCursor; z: 100
        property real _smx; property real _sw
        onPressed: { _smx = mouseX; _sw = chatWindow.width }
        onPositionChanged: chatWindow.width = Math.max(chatWindow.minimumWidth, _sw + (mouseX - _smx))
    }
    // 上边缘
    MouseArea {
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        height: _edgeSize
        visible: !chatWindow.isMaximizedState
        cursorShape: Qt.SizeVerCursor; z: 100
        property real _smy; property real _sh; property real _sy
        onPressed: { _smy = mouseY; _sh = chatWindow.height; _sy = chatWindow.y }
        onPositionChanged: {
            var d = mouseY - _smy
            var nh = _sh - d
            if (nh >= chatWindow.minimumHeight) { chatWindow.y = _sy + d; chatWindow.height = nh }
        }
    }
    // 下边缘
    MouseArea {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: _edgeSize
        visible: !chatWindow.isMaximizedState
        cursorShape: Qt.SizeVerCursor; z: 100
        property real _smy; property real _sh
        onPressed: { _smy = mouseY; _sh = chatWindow.height }
        onPositionChanged: chatWindow.height = Math.max(chatWindow.minimumHeight, _sh + (mouseY - _smy))
    }

    // 左上角
    MouseArea {
        anchors.left: parent.left; anchors.top: parent.top
        width: _edgeSize; height: _edgeSize
        visible: !chatWindow.isMaximizedState
        cursorShape: Qt.SizeFDiagCursor; z: 101
        property real _smx; property real _smy; property real _sw; property real _sh; property real _sx; property real _sy
        onPressed: { _smx = mouseX; _smy = mouseY; _sw = chatWindow.width; _sh = chatWindow.height; _sx = chatWindow.x; _sy = chatWindow.y }
        onPositionChanged: {
            var dx = mouseX - _smx; var dy = mouseY - _smy
            var nw = _sw - dx; var nh = _sh - dy
            if (nw >= chatWindow.minimumWidth) { chatWindow.x = _sx + dx; chatWindow.width = nw }
            if (nh >= chatWindow.minimumHeight) { chatWindow.y = _sy + dy; chatWindow.height = nh }
        }
    }
    // 右上角
    MouseArea {
        anchors.right: parent.right; anchors.top: parent.top
        width: _edgeSize; height: _edgeSize
        visible: !chatWindow.isMaximizedState
        cursorShape: Qt.SizeBDiagCursor; z: 101
        property real _smx; property real _smy; property real _sw; property real _sh; property real _sy
        onPressed: { _smx = mouseX; _smy = mouseY; _sw = chatWindow.width; _sh = chatWindow.height; _sy = chatWindow.y }
        onPositionChanged: {
            var dx = mouseX - _smx; var dy = mouseY - _smy
            chatWindow.width = Math.max(chatWindow.minimumWidth, _sw + dx)
            var nh = _sh - dy
            if (nh >= chatWindow.minimumHeight) { chatWindow.y = _sy + dy; chatWindow.height = nh }
        }
    }
    // 左下角
    MouseArea {
        anchors.left: parent.left; anchors.bottom: parent.bottom
        width: _edgeSize; height: _edgeSize
        visible: !chatWindow.isMaximizedState
        cursorShape: Qt.SizeBDiagCursor; z: 101
        property real _smx; property real _smy; property real _sw; property real _sh; property real _sx
        onPressed: { _smx = mouseX; _smy = mouseY; _sw = chatWindow.width; _sh = chatWindow.height; _sx = chatWindow.x }
        onPositionChanged: {
            var dx = mouseX - _smx; var dy = mouseY - _smy
            var nw = _sw - dx
            if (nw >= chatWindow.minimumWidth) { chatWindow.x = _sx + dx; chatWindow.width = nw }
            chatWindow.height = Math.max(chatWindow.minimumHeight, _sh + dy)
        }
    }
    // 右下角
    MouseArea {
        anchors.right: parent.right; anchors.bottom: parent.bottom
        width: _edgeSize; height: _edgeSize
        visible: !chatWindow.isMaximizedState
        cursorShape: Qt.SizeFDiagCursor; z: 101
        property real _smx; property real _smy; property real _sw; property real _sh
        onPressed: { _smx = mouseX; _smy = mouseY; _sw = chatWindow.width; _sh = chatWindow.height }
        onPositionChanged: {
            chatWindow.width = Math.max(chatWindow.minimumWidth, _sw + (mouseX - _smx))
            chatWindow.height = Math.max(chatWindow.minimumHeight, _sh + (mouseY - _smy))
        }
    }
}
