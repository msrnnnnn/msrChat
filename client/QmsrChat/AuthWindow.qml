/**
 * @file AuthWindow.qml
 * @brief 认证窗口 — 无边框独立窗口，卡片即窗口
 * @details 使用 FramelessWindowHint 去除原生窗口边框，卡片填满窗口。
 *          所有页面预创建（非 Component 惰性实例），切换 0 延迟。
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: authWindow
    visible: true
    width: 376
    height: pageStack.currentItem && pageStack.currentItem.preferredHeight
            ? pageStack.currentItem.preferredHeight : 540
    flags: Qt.FramelessWindowHint | Qt.Window
    color: "transparent"
    title: "msrChat"

    Behavior on height {
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }

    // 预创建页面实例 — 避免首次切换时编译+创建的卡顿
    // 所有页面作为 StackView 子项，StackView 管理可见性
    StackView {
        id: pageStack
        anchors.fill: parent
        initialItem: loginView

        replaceEnter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 300; easing.type: Easing.OutCubic }
            }
        }
        replaceExit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 200 }
        }

        LoginView {
            id: loginView
            onSwitchToRegister: pageStack.replace(registerView)
            onSwitchToReset: pageStack.replace(resetView)
        }

        RegisterView {
            id: registerView
            onSwitchToLogin: pageStack.replace(loginView)
        }

        ResetView {
            id: resetView
            onSwitchToLogin: pageStack.replace(loginView)
        }
    }

    // 窗口控制按钮 — 最小化(左) + 关闭(右)，z 高于拖拽区域
    Row {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 8
        anchors.rightMargin: 8
        spacing: 4
        z: 101

        Button {
            id: minimizeBtn
            width: 32; height: 28
            flat: true
            contentItem: Text {
                text: "─"
                color: minimizeBtn.hovered ? "#1A1A2E" : "#9C9AAA"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: minimizeBtn.hovered ? "#EEF2FF" : "transparent"
                radius: 6
            }
            onClicked: authWindow.showMinimized()
        }

        Button {
            id: closeBtn
            width: 32; height: 28
            flat: true
            contentItem: Text {
                text: "✕"
                color: closeBtn.hovered ? "#EF4444" : "#9C9AAA"
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: closeBtn.hovered ? "#FEE2E2" : "transparent"
                radius: 6
            }
            onClicked: authWindow.close()
        }
    }

    // 顶部拖拽区域 — 无边框窗口需要手动实现拖动
    MouseArea {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 40
        z: 100
        property point pressPos

        onPressed: pressPos = Qt.point(mouseX, mouseY)
        onPositionChanged: {
            var delta = Qt.point(mouseX - pressPos.x, mouseY - pressPos.y)
            authWindow.x += delta.x
            authWindow.y += delta.y
        }
    }
}
