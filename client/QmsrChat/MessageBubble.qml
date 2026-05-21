/**
 * @file MessageBubble.qml
 * @brief 消息气泡组件
 * @details 用于在聊天界面中显示单条消息气泡，支持左右对齐、状态显示和动画效果。
 */
import QtQuick

Rectangle {
    id: messageBubble
    property bool isSelf: false
    property string content: ""
    property string timestamp: ""
    property int status: 0

    property int maxBubbleWidth: Math.min(chatViewRoot.width * 0.7, 300)

    implicitWidth: bubbleContent.width + 24
    implicitHeight: bubbleContent.height + 16
    radius: 12
    color: isSelf ? "#2196F3" : "#FFFFFF"

    border.width: 1
    border.color: isSelf ? "#1976D2" : "#E0E0E0"

    Column {
        id: bubbleContent
        anchors.centerIn: parent
        spacing: 4

        Text {
            id: messageText
            text: content
            color: isSelf ? "#FFFFFF" : "#333333"
            font.pixelSize: 14
            font.family: "Microsoft YaHei"
            wrapMode: Text.WordWrap
            width: Math.min(implicitWidth, messageBubble.maxBubbleWidth - 16)
        }

        Row {
            spacing: 4
            layoutDirection: isSelf ? Qt.RightToLeft : Qt.LeftToRight

            Text {
                id: timeText
                text: timestamp
                color: isSelf ? "rgba(255,255,255,0.7)" : "rgba(0,0,0,0.5)"
                font.pixelSize: 10
                font.family: "Microsoft YaHei"
            }

            Text {
                id: statusIcon
                visible: isSelf
                font.pixelSize: 10
                color: isSelf ? "rgba(255,255,255,0.7)" : "rgba(0,0,0,0.5)"

                text: {
                    switch (status) {
                        case 0: return "发送中"
                        case 1: return "已送达"
                        case 2: return "离线"
                        case -1: return "失败"
                        default: return ""
                    }
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