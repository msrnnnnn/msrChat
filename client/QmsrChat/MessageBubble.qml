/**
 * @file MessageBubble.qml
 * @brief 消息气泡组件
 * @details 用于在聊天界面中显示单条消息气泡，支持左右对齐、状态显示和动画效果。
 */
import QtQuick
import QtQuick.Layouts

Item {
    id: messageBubble

    property bool isSelf: false
    property string content: ""
    property var timestamp: 0          // qint64 ms epoch via QML var (JS Number 53-bit OK for ms)
    property string displayTime: ""    // HH:mm:ss for UI display only
    property int status: 0
    property bool recalled: false
    property bool edited: false

    property real viewWidth: 400
    property int maxBubbleWidth: Math.min(viewWidth * 0.7, 300)

    width: parent ? parent.width : 0
    height: recalled ? recalledRect.height : bubbleRect.height

    // 已撤回状态显示
    Rectangle {
        id: recalledRect
        visible: recalled
        x: isSelf ? parent.width - width : 0
        y: 0
        width: Math.min(maxBubbleWidth, recalledText.width + 24)
        height: 30
        radius: 12
        color: "#F0F0F0"
        border.width: 1
        border.color: "#E0E0E0"

        Text {
            id: recalledText
            anchors.centerIn: parent
            text: "消息已撤回"
            color: "#999999"
            font.pixelSize: 13
            font.family: "Microsoft YaHei"
            font.italic: true
        }
    }

    Rectangle {
        id: bubbleRect
        width: Math.min(maxBubbleWidth, bubbleContent.width + 16)
        height: bubbleContent.height + 16
        anchors.right: isSelf ? parent.right : undefined
        anchors.left: isSelf ? undefined : parent.left
        anchors.top: parent.top
        radius: 12
        color: isSelf ? "#2196F3" : "#FFFFFF"
        border.width: 1
        border.color: isSelf ? "#1976D2" : "#E0E0E0"
        opacity: recalled ? 0.5 : 1.0
        visible: !recalled

        ColumnLayout {
            id: bubbleContent
            x: 8
            y: 8
            width: Math.min(maxBubbleWidth - 16, implicitWidth)
            spacing: 4

            Text {
                id: messageText
                text: content
                color: isSelf ? "#FFFFFF" : "#333333"
                font.pixelSize: 16
                font.family: "Microsoft YaHei"
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                Layout.fillWidth: true
            }

            Text {
                id: editedLabel
                visible: edited && !recalled
                text: "(已编辑)"
                color: isSelf ? Qt.rgba(1.0, 1.0, 1.0, 0.5) : Qt.rgba(0.0, 0.0, 0.0, 0.4)
                font.pixelSize: 10
                font.family: "Microsoft YaHei"
                Layout.alignment: isSelf ? Qt.AlignRight : Qt.AlignLeft
            }

            RowLayout {
                spacing: 4
                Layout.alignment: isSelf ? Qt.AlignRight : Qt.AlignLeft

                Text {
                    id: timeText
                    text: displayTime
                    color: isSelf ? Qt.rgba(1.0, 1.0, 1.0, 0.7) : Qt.rgba(0.0, 0.0, 0.0, 0.5)
                    font.pixelSize: 10
                    font.family: "Microsoft YaHei"
                }

                Text {
                    id: statusIcon
                    visible: isSelf
                    font.pixelSize: 10
                    color: isSelf ? Qt.rgba(1.0, 1.0, 1.0, 0.7) : Qt.rgba(0.0, 0.0, 0.0, 0.5)

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
    }

    Rectangle {
        id: tailIndicator
        width: 12
        height: 12
        rotation: 45
        color: bubbleRect.color

        anchors.bottom: bubbleRect.bottom
        anchors.bottomMargin: 6
        anchors.horizontalCenter: isSelf ? bubbleRect.right : bubbleRect.left
        anchors.horizontalCenterOffset: isSelf ? 6 : -6
        z: -1
        visible: !recalled
    }

    // 右键 MouseArea（Phase 6）— 召唤 MessageActionMenu
    MouseArea {
        anchors.fill: recalled ? recalledRect : bubbleRect
        acceptedButtons: Qt.RightButton
        z: 1
        enabled: !recalled  // 已撤回消息不允许右键菜单
        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                // Phase B — 用 mapToItem 把局部坐标映射到 chatViewRoot 坐标系
                var root = messageBubble
                while (root && root.objectName !== "chatViewRoot") {
                    root = root.parent
                }
                if (!root) {
                    console.warn("[MessageBubble] cannot find chatViewRoot")
                    return
                }
                var pt = bubbleRect.mapToItem(root, mouse.x, mouse.y)
                messageBubble.rightClicked(pt.x, pt.y, messageBubble.timestamp)
            }
        }
    }

    SequentialAnimation {
        id: appearAnimation
        running: false

        PropertyAction {
            target: bubbleRect
            property: "opacity"
            value: 0
        }

        NumberAnimation {
            target: bubbleRect
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

    signal rightClicked(real x, real y, var timestamp)
}