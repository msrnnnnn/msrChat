import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: appWindow
    visible: true
    width: 800
    height: 600
    minimumWidth: 600
    minimumHeight: 500
    color: "#F8F9FE"
    title: "msrChat"

    StackView {
        id: pageStack
        anchors.fill: parent
        initialItem: loginPage
        replaceEnter: Transition { ParallelAnimation { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 300; easing.type: Easing.OutCubic } } }
        replaceExit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 200 } }
    }

    Component {
        id: loginPage
        LoginView {
            onSwitchToRegister: pageStack.replace(registerPage)
            onSwitchToReset: pageStack.replace(resetPage)
        }
    }

    Component {
        id: registerPage
        RegisterView { onSwitchToLogin: pageStack.replace(loginPage) }
    }

    Component {
        id: resetPage
        ResetView { onSwitchToLogin: pageStack.replace(loginPage) }
    }

    Component {
        id: chatPage
        Rectangle {
            color: "#F8F9FE"
            Component.onCompleted: {
                chatController.initialize()
                var buffered = authController.TakeBufferedMessages()
                if (buffered && buffered.length > 0) {
                    chatController.drainBufferedMessages(buffered)
                }
            }
            ChatView {
                anchors.fill: parent
                chatModel: _chatModel
                chatController: chatController
            }
        }
    }

    Connections {
        target: authController
        function onChatLoginSuccess() { pageStack.replace(chatPage) }
        function onTokenInvalid(message) { pageStack.replace(loginPage) }
    }
}
