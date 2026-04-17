/**
 * @file MessageBubble.qml
 * @brief 消息气泡组件
 * @details 用于在聊天界面中显示单条消息气泡，支持左右对齐、状态显示和动画效果。
 */
import QtQuick 2.15
import QtQuick.Layouts 1.15

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
