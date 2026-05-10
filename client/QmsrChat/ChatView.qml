/**
 * @file ChatView.qml
 * @brief 现代化聊天界面视图
 * @details 使用 QML ListView 实现气泡式聊天界面，支持左右对齐、状态显示和动画效果。
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQml

Rectangle {
    id: chatViewRoot
    color: "#F5F5F5"

    property var chatModel: null
    property var chatController: null
    property var chatDialog: null

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
            spacing: 4
            verticalLayoutDirection: ListView.TopToBottom
            clip: true

            model: chatModel

            delegate: Item {
                width: chatViewRoot.width - 20
                height: bubbleLoader.item ? bubbleLoader.item.height + 8 : 0

                Loader {
                    id: bubbleLoader
                    anchors.left: isSelf ? undefined : parent.left
                    anchors.right: isSelf ? parent.right : undefined
                    anchors.top: parent.top
                    anchors.margins: 4
                    sourceComponent: MessageBubble {
                        isSelf: model.isSelf
                        content: model.content
                        timestamp: model.displayTime
                        status: model.status
                    }
                }
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

            onCountChanged: {
                Qt.callLater(function() {
                    positionViewAtEnd()
                })
            }
        }

        Rectangle {
            id: inputArea
            Layout.fillWidth: true
            Layout.preferredHeight: 130
            color: "#FFFFFF"
            border.width: 1
            border.color: "#E0E0E0"

            TextArea {
                id: messageInput
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: 5
                anchors.rightMargin: 8
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
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 10
                width: 70
                height: 36
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

            Button {
                id: fileButton
                anchors.right: sendButton.left
                anchors.bottom: parent.bottom
                anchors.margins: 10
                width: 36
                height: 36
                text: qsTr("📎")
                font.pixelSize: 16
                enabled: isConnected

                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? "#666666" : "#A0A0A0"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font: parent.font
                }

                background: Rectangle {
                    color: "#F0F0F0"
                    radius: 8
                    border.width: 1
                    border.color: "#E0E0E0"
                }

                onClicked: {
                    if (chatController) {
                        chatController.sendFile("")
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

    ListModel {
        id: fileProgressModel
    }

    Rectangle {
        id: fileProgressPanel
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 10
        width: 240
        height: fileProgressList.height + 10
        visible: fileProgressModel.count > 0
        color: "#FFFFFF"
        border.width: 1
        border.color: "#E0E0E0"
        radius: 8

        ListView {
            id: fileProgressList
            anchors.centerIn: parent
            width: parent.width - 10
            height: contentHeight
            model: fileProgressModel
            interactive: false

            delegate: Rectangle {
                width: parent.width
                height: 40
                color: "transparent"

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Text {
                        text: filename
                        font.pixelSize: 12
                        color: "#333333"
                    }
                    ProgressBar {
                        width: parent.width
                        from: 0
                        to: 100
                        value: progress
                    }
                    Text {
                        text: progress + "%"
                        font.pixelSize: 10
                        color: "#666666"
                    }
                }
            }
        }
    }

    Connections {
        target: chatController

        function onSigError(errorMsg) {
            console.error("[Chat业务异常]: " + errorMsg)
            // TODO: 替换为实际的 Toast 或 MessageDialog 组件调用
        }

        function onSigFileSendStarted(task_id, filename, total_size) {
            fileProgressModel.append({"task_id": task_id, "filename": filename, "progress": 0})
        }

        function onSigFileSendProgress(task_id, prog, sent, total) {
            for (var i = 0; i < fileProgressModel.count; i++) {
                if (fileProgressModel.get(i).task_id === task_id) {
                    fileProgressModel.setProperty(i, "progress", prog)
                    break
                }
            }
        }

        function onSigFileSendComplete(task_id, success, error) {
            for (var i = 0; i < fileProgressModel.count; i++) {
                if (fileProgressModel.get(i).task_id === task_id) {
                    fileProgressModel.remove(i)
                    break
                }
            }
        }
    }

    // 临时状态显示栏
    Text {
        text: "当前UID: " + (chatController ? chatController.currentUid : "未知")
              + " | 目标UID: " + (chatController ? chatController.targetUid : "未选择")
        color: "#666666"
        font.pixelSize: 12
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.margins: 5
    }
}
