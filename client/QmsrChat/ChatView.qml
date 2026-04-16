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

/**
 * @brief 消息气泡组件
 */
Rectangle {
    id: messageBubble
    property bool isSelf: false
    property string content: ""
    property string timestamp: ""
    property int status: 0

    width: bubbleLayout.width + 24
    height: bubbleLayout.height + 16
    radius: 12
    color: isSelf ? "#2196F3" : "#FFFFFF"

    anchors {
        right: isSelf ? parent.right : undefined
        left: isSelf ? undefined : parent.left
        top: parent.top
        margins: 4
    }

    border.width: 1
    border.color: isSelf ? "#1976D2" : "#E0E0E0"

    ColumnLayout {
        id: bubbleLayout
        anchors.centerIn: parent
        spacing: 4

        Text {
            id: messageText
            text: content
            color: isSelf ? "#FFFFFF" : "#333333"
            font.pixelSize: 14
            font.family: "Microsoft YaHei"
            wrapMode: Text.WordWrap
            Layout.maximumWidth: bubbleMaxWidth
            Layout.preferredWidth: Math.min(bubbleMaxWidth, messageText.contentWidth + 1)
            Layout.margins: 8

            property int bubbleMaxWidth: parent.parent.width * 0.7
        }

        RowLayout {
            spacing: 4
            Layout.alignment: isSelf ? Qt.AlignRight : Qt.AlignLeft
            Layout.margins: 8

            Text {
                id: timeText
                text: timestamp
                color: isSelf ? "rgba(255,255,255,0.7)" : "rgba(0,0,0,0.5)"
                font.pixelSize: 10
                font.family: "Microsoft YaHei"
            }

            Image {
                id: statusIcon
                width: 14
                height: 14
                visible: isSelf && status !== undefined

                source: {
                    if (!isSelf) return ""
                    switch (status) {
                        case 0: return "qrc:/image/clock.png"
                        case 1: return "qrc:/image/check.png"
                        case -1: return "qrc:/image/error.png"
                        default: return ""
                    }
                }

                states: [
                    State {
                        name: "sending"
                        when: isSelf && status === 0
                        PropertyChanges { target: statusIcon; opacity: 0.6 }
                    },
                    State {
                        name: "sent"
                        when: isSelf && status === 1
                        PropertyChanges { target: statusIcon; opacity: 1.0 }
                    },
                    State {
                        name: "failed"
                        when: isSelf && status === -1
                        PropertyChanges { target: statusIcon; opacity: 1.0 }
                    }
                ]

                transitions: Transition {
                    NumberAnimation { properties: "opacity"; duration: 300 }
                }
            }
        }
    }

    Rectangle {
        id: tailIndicator
        width: 12
        height: 12
        rotation: 45
        color: parent.color
        border.width: 1
        border.color: parent.border.color

        anchors {
            verticalCenter: parent.verticalCenter
            horizontalCenter: isSelf ? parent.left : parent.right
            horizontalCenterOffset: isSelf ? 6 : -6
        }
    }

    SequentialAnimation {
        id: appearAnimation
        running: false

        PropertyAction {
            target: messageBubble
            property: "opacity"
            value: 0
        }

        NumberAnimation {
            target: messageBubble
            property: "opacity"
            from: 0
            to: 1
            duration: 200
            easing.type: Easing.OutQuad
        }
    }

    Component.onCompleted: {
        appearAnimation.running = true
    }
}
