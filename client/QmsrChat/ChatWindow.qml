/**
 * @file ChatWindow.qml
 * @brief 聊天主窗口 — 独立的大窗口，包含 ChatView
 * @details 登录成功后由 main.cpp 创建，替代旧的 MainWindow.qml 中的 chatPage
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
    color: "#F8F9FE"
    title: "msrChat"

    Component.onCompleted: {
        chatController.initialize()
        var buffered = authController.TakeBufferedMessages()
        if (buffered && buffered.length > 0) {
            chatController.drainBufferedMessages(buffered)
        }
    }

    ChatView {
        anchors.fill: parent
    }
}
