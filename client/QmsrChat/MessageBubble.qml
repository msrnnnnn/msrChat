/**
 * @file MessageBubble.qml
 * @brief 消息气泡组件
 * @details 用于在聊天界面中显示单条消息气泡，支持左右对齐、状态显示和动画效果。
 */
import QtQuick
import QtQuick.Layouts

// 文字消息气泡 — 支持左右对齐（己方蓝/对方白）、已撤回/已编辑状态、状态图标和入场动画
Item {
    id: messageBubble

    // 是否为当前用户发送的消息
    property bool isSelf: false
    // 消息文字内容
    property string content: ""
    // 消息时间戳（毫秒级 Unix epoch）
    property var timestamp: 0          // qint64 ms epoch via QML var (JS Number 53-bit OK for ms)
    // 格式化后的显示时间（HH:mm:ss）
    property string displayTime: ""    // HH:mm:ss for UI display only
    // 消息状态：0=发送中 1=已送达 2=离线 -1=失败
    property int status: 0
    // 是否已撤回
    property bool recalled: false
    // 是否已编辑
    property bool edited: false

    // 视图宽度和气泡最大宽度（70% 视图宽，上限 300px）
    property real viewWidth: 400
    property int maxBubbleWidth: Math.min(viewWidth * 0.7, 300)

    width: parent ? parent.width : 0
    // 根据撤回状态切换显示区域高度
    height: recalled ? recalledRect.height : bubbleRect.height

    // 已撤回状态显示 — 居中灰色胶囊 "消息已撤回"
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

    // 正常消息气泡 — 己方蓝色右对齐，对方白色左对齐
    Rectangle {
        id: bubbleRect
        width: Math.min(maxBubbleWidth, bubbleContent.width + 16)
        height: bubbleContent.height + 16
        // 左右对齐切换
        anchors.right: isSelf ? parent.right : undefined
        anchors.left: isSelf ? undefined : parent.left
        anchors.top: parent.top
        radius: 12
        // 颜色区分己方/对方
        color: isSelf ? "#2196F3" : "#FFFFFF"
        border.width: 1
        border.color: isSelf ? "#1976D2" : "#E0E0E0"
        opacity: recalled ? 0.5 : 1.0
        visible: !recalled

        // 气泡内容布局：消息文本 + 已编辑标签 + 时间/状态行
        ColumnLayout {
            id: bubbleContent
            x: 8
            y: 8
            width: Math.min(maxBubbleWidth - 16, implicitWidth)
            spacing: 4

            // 消息文字 — 自动换行，己方白色对方深灰
            Text {
                id: messageText
                text: content
                color: isSelf ? "#FFFFFF" : "#333333"
                font.pixelSize: 16
                font.family: "Microsoft YaHei"
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                Layout.fillWidth: true
            }

            // 已编辑标签 — 仅在 edited 且未撤回时显示
            Text {
                id: editedLabel
                visible: edited && !recalled
                text: "(已编辑)"
                color: isSelf ? Qt.rgba(1.0, 1.0, 1.0, 0.5) : Qt.rgba(0.0, 0.0, 0.0, 0.4)
                font.pixelSize: 10
                font.family: "Microsoft YaHei"
                Layout.alignment: isSelf ? Qt.AlignRight : Qt.AlignLeft
            }

            // 时间与状态行 — 己方右对齐（时间左，状态右），对方左对齐
            RowLayout {
                Layout.alignment: isSelf ? Qt.AlignRight : Qt.AlignLeft

                // 时间文字
                Text {
                    id: timeText
                    text: displayTime
                    color: isSelf ? Qt.rgba(1.0, 1.0, 1.0, 0.7) : Qt.rgba(0.0, 0.0, 0.0, 0.5)
                    font.pixelSize: 10
                    font.family: "Microsoft YaHei"
                }

                // 状态图标 — 仅己方消息显示
                Text {
                    id: statusIcon
                    visible: isSelf
                    font.pixelSize: 10
                    color: isSelf ? Qt.rgba(1.0, 1.0, 1.0, 0.7) : Qt.rgba(0.0, 0.0, 0.0, 0.5)

                    // 根据 status 值映射为中文状态文本
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

    // 气泡尾部三角指示器 — 旋转 45° 的正方形，颜色跟随气泡
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

    // 右键 MouseArea — 捕获右键点击并向上查找 chatViewRoot 以触发 showActionMenu
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

    // 入场渐显动画 — 组件完成初始化后执行
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

    // 组件完成初始化时播放入场动画
    Component.onCompleted: {
        appearAnimation.running = true
    }

    signal rightClicked(real x, real y, var timestamp)
}