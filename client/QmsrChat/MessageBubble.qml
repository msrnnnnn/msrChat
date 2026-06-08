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

    // 视图宽度和气泡最大宽度（50% 视图宽，短文本自动适配）
    property real viewWidth: 400
    property int maxBubbleWidth: Math.floor(viewWidth * 0.5)

    width: parent ? parent.width : 0
    // 根据撤回状态切换显示区域高度（气泡 + 底部元信息）
    height: recalled ? recalledRect.height : bubbleRect.height + 18

    // 已撤回状态显示 — 居中灰色胶囊
    Rectangle {
        id: recalledRect
        visible: recalled
        anchors.horizontalCenter: parent.horizontalCenter
        y: 0
        width: Math.min(maxBubbleWidth, recalledText.width + 40)
        height: 32
        radius: 999
        color: "#F8F9FE"
        border.width: 1
        border.color: "#EAE9F2"

        Text {
            id: recalledText
            anchors.centerIn: parent
            text: isSelf ? "你撤回了一条消息" : "对方撤回了一条消息"
            color: "#9C9AAA"
            font.pixelSize: 12
            font.family: "Microsoft YaHei"
            font.italic: false
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
        // 圆角：三个角圆润，底角尖锐突出（己方右下，对方左下）
        topLeftRadius: 12
        topRightRadius: 12
        bottomLeftRadius: isSelf ? 12 : 0
        bottomRightRadius: isSelf ? 0 : 12
        // 颜色区分己方/对方
        color: isSelf ? "#4F46E5" : "#FFFFFF"
        border.width: 1
        border.color: isSelf ? "#3730A3" : "#EAE9F2"
        opacity: recalled ? 0.5 : 1.0
        visible: !recalled

        // 气泡内容布局：消息文本 + 已编辑标签
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
                color: isSelf ? "#FFFFFF" : "#1A1A2E"
                font.pixelSize: 14
                font.family: "Microsoft YaHei"
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                Layout.fillWidth: true
            }

            // 已编辑标签 — 仅在 edited 且未撤回时显示
            Text {
                id: editedLabel
                visible: edited && !recalled
                text: "(已编辑)"
                color: isSelf ? Qt.rgba(1.0, 1.0, 1.0, 0.60) : "#9C9AAA"
                font.pixelSize: 10
                font.family: "Microsoft YaHei"
                font.italic: true
                Layout.alignment: isSelf ? Qt.AlignRight : Qt.AlignLeft
            }
        }
    }

    // 时间与状态行 — 放在气泡下方，己方右对齐，对方左对齐
    RowLayout {
        id: metaRow
        anchors.top: bubbleRect.bottom
        anchors.topMargin: 2
        anchors.right: isSelf ? parent.right : undefined
        anchors.left: isSelf ? undefined : bubbleRect.left
        spacing: 4
        visible: !recalled

        Text {
            id: timeText
            text: displayTime
            color: "#9C9AAA"
            font.pixelSize: 10
            font.family: "Microsoft YaHei"
        }

        Text {
            id: statusIcon
            visible: isSelf
            font.pixelSize: 10
            font.family: "Microsoft YaHei"
            color: {
                switch (status) {
                    case -1: return "#EF4444"
                    case 2:  return "#F59E0B"
                    default: return "#9C9AAA"
                }
            }
            opacity: status === 0 ? 0.5 : 1.0
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

        PropertyAction {
            target: bubbleRect
            property: "transformOrigin"
            value: Item.Center
        }

        ParallelAnimation {
            NumberAnimation {
                target: bubbleRect
                property: "opacity"
                from: 0
                to: 1
                duration: 350
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: bubbleRect
                property: "y"
                from: bubbleRect.y + 10
                to: bubbleRect.y
                duration: 350
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: bubbleRect
                property: "scale"
                from: 0.96
                to: 1.0
                duration: 350
                easing.type: Easing.OutCubic
            }
        }
    }

    // 组件完成初始化时播放入场动画
    Component.onCompleted: {
        appearAnimation.running = true
    }

    signal rightClicked(real x, real y, var timestamp)
}