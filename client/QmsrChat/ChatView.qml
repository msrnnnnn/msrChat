/**
 * @file ChatView.qml
 * @brief 现代化聊天界面视图
 * @details 使用 QML ListView 实现气泡式聊天界面，支持左右对齐、状态显示和动画效果。
 */
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: chatViewRoot
    color: "#F5F5F5"

    property int currentUid: chatController ? chatController.currentUid : 0
    property int targetUid: chatController ? chatController.targetUid : 0
    property bool isConnected: chatController ? chatController.isConnected : false

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ListView {
            id: messageListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 10
            spacing: 8
            verticalLayoutDirection: ListView.BottomToTop
            reverseLayout: false
            clip: true

            model: chatModel

            delegate: MessageBubble {
                isSelf: model.isSelf
                content: model.content
                timestamp: model.displayTime
                status: model.status
                width: chatViewRoot.width - 20
            }

            ScrollBar.vertical: ScrollBar {
                width: 8
                anchors.right: parent.right
                anchors.rightMargin: 2
                policy: ScrollBar.AsNeeded
                background: Rectangle {
                    color: "#E0E0E0"
                    radius: 4
                }
                contentItem: Rectangle {
                    color: "#A0A0A0"
                    radius: 4
                }
            }

            Component.onCompleted: {
                positionViewAtEnd()
            }
        }

        Rectangle {
            id: inputArea
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: "#FFFFFF"
            border.width: 1
            border.color: "#E0E0E0"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                TextArea {
                    id: messageInput
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    placeholderText: qsTr("输入消息...")
                    wrapMode: TextArea.Wrap
                    font.pixelSize: 14
                    verticalAlignment: TextInput.AlignVCenter
                    padding: 8

                    background: Rectangle {
                        color: "#F8F8F8"
                        radius: 8
                        border.width: 1
                        border.color: "#E0E0E0"
                    }

                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Return && !(event.modifiers & Qt.ShiftModifier)) {
                            event.accepted = true
                            sendButton.clicked()
                        }
                    }
                }

                Button {
                    id: sendButton
                    Layout.preferredWidth: 80
                    Layout.fillHeight: true
                    text: qsTr("发送")
                    font.pixelSize: 14
                    font.bold: true
                    enabled: isConnected && messageInput.text.trim().length > 0

                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? "#FFFFFF" : "#A0A0A0"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font: parent.font
                    }

                    background: Rectangle {
                        color: parent.enabled ? "#2196F3" : "#E0E0E0"
                        radius: 8
                        border.width: 0
                    }

                    onClicked: {
                        if (messageInput.text.trim().length > 0) {
                            chatController.sendMessage(messageInput.text)
                            messageInput.text = ""
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: connectionIndicator
        width: 10
        height: 10
        radius: 5
        color: isConnected ? "#4CAF50" : "#F44336"
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 10
        visible: !isConnected

        SequentialAnimation on color {
            running: !isConnected
            loops: Animation.Infinite
            ColorAnimation { to: "#FFC107"; duration: 500 }
            ColorAnimation { to: "#F44336"; duration: 500 }
        }
    }

    Label {
        id: connectionLabel
        text: isConnected ? "" : qsTr("连接断开")
        color: "#F44336"
        font.pixelSize: 12
        anchors.top: connectionIndicator.bottom
        anchors.right: parent.right
        anchors.margins: 10
        visible: !isConnected
    }
}
