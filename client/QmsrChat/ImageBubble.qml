/**
 * @file ImageBubble.qml
 * @brief 图片消息气泡（接收方左 / 发送方右，含可选 caption + 加载态 + 已编辑徽章 + 撤回占位）
 * @details Phase 5 — 颜色沿用 MessageBubble 蓝色 #2196F3（自方） / 白色（对方），与文字气泡保持视觉一致。
 *          不在 P5 加右键 MouseArea（由 P6 接管）。
 */
import QtQuick

// 图片消息气泡 — 含缩略图、加载/失败占位、caption、已编辑徽章、撤回占位
Item {
    id: imageBubble

    // 视图宽度和气泡/缩略图尺寸
    property real viewWidth: 400
    property int maxBubbleWidth: Math.min(viewWidth * 0.7, 300)
    property int thumbnailSize: 240

    // 是否为当前用户发送
    property bool isSelf: false
    // 图片本地文件路径（"error" 表示加载失败）
    property string imagePath: ""
    // 图片说明文字
    property string caption: ""
    // 图片原始尺寸（用于计算缩略图比例）
    property int imageWidth: 0
    property int imageHeight: 0
    // 加载状态标志
    property bool loaded: imagePath !== "" && imagePath !== "error"
    property bool failed: imagePath === "error"
    // 是否已编辑 / 已撤回
    property bool edited: false
    property bool recalled: false
    // 时间戳和显示时间
    property var timestamp: 0
    property string displayTime: ""

    width: parent ? parent.width : 0
    // 根据撤回状态切换高度来源
    height: recalled ? recalledRow.height + 8 : bubbleRect.height + 8

    // 撤回占位 — 居中灰色胶囊，区分己方/对方文字
    Item {
        id: recalledRow
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width
        height: recalled ? 30 : 0
        visible: recalled

        Rectangle {
            anchors.centerIn: parent
            height: 24
            width: Math.min(recalledText.implicitWidth + 24, imageBubble.maxBubbleWidth)
            radius: 4
            color: "#ebedf0"

            Text {
                id: recalledText
                anchors.centerIn: parent
                text: imageBubble.isSelf ? qsTr("你撤回了一条消息") : qsTr("对方撤回了一条消息")
                color: "#b0b3b8"
                font.pixelSize: 12
                font.family: "Microsoft YaHei"
            }
        }
    }

    // 图片气泡主体 — 己方蓝/对方白，撤回时隐藏
    Rectangle {
        id: bubbleRect
        anchors.top: recalledRow.bottom
        anchors.topMargin: recalled ? 8 : 0
        anchors.right: imageBubble.isSelf ? parent.right : undefined
        anchors.left: imageBubble.isSelf ? undefined : parent.left
        width: Math.min(imageBubble.maxBubbleWidth, imageColumn.implicitWidth + 16)
        height: imageColumn.height + 16
        // 圆角：三个角圆润，底角尖锐突出（同 MessageBubble）
        topLeftRadius: 12
        topRightRadius: 12
        bottomLeftRadius: imageBubble.isSelf ? 12 : 0
        bottomRightRadius: imageBubble.isSelf ? 0 : 12
        color: imageBubble.isSelf ? "#4F46E5" : "#FFFFFF"
        border.width: imageBubble.isSelf ? 0 : 1
        border.color: "#EAE9F2"
        visible: !imageBubble.recalled
        opacity: 0

        // 渐显动画
        Behavior on opacity {
            NumberAnimation { duration: 200; easing.type: Easing.OutQuad }
        }
    }

    // 图片列布局：缩略图 + caption + 时间/编辑标签
    Column {
        id: imageColumn
        anchors.top: bubbleRect.top
        anchors.topMargin: 8
        anchors.horizontalCenter: bubbleRect.horizontalCenter
        spacing: 4
        visible: !imageBubble.recalled

        // 缩略图容器 — 按原始比例缩放
        Item {
            id: thumbnailContainer
            width: {
                var w = imageBubble.imageWidth
                if (w <= 0) return imageBubble.thumbnailSize
                return Math.min(imageBubble.thumbnailSize, w)
            }
            height: imageBubble.imageWidth > 0 && imageBubble.imageHeight > 0
                    ? (width * imageBubble.imageHeight / imageBubble.imageWidth)
                    : (width * 0.66)

            // 加载中占位 — 灰色背景 + "加载中…"
            Rectangle {
                anchors.fill: parent
                color: "#EEF2FF"
                visible: !imageBubble.loaded && !imageBubble.failed
                radius: 6

                Text {
                    anchors.centerIn: parent
                    text: qsTr("加载中…")
                    color: "#9C9AAA"
                    font.pixelSize: 13
                    font.family: "Microsoft YaHei"
                }
            }

            // 加载失败占位 — 浅红背景 + 错误提示
            Rectangle {
                anchors.fill: parent
                color: "#FEF2F2"
                visible: imageBubble.failed
                radius: 6

                Text {
                    anchors.centerIn: parent
                    text: qsTr("图片加载失败")
                    color: "#EF4444"
                    font.pixelSize: 13
                    font.family: "Microsoft YaHei"
                }
            }

            // 实际缩略图 — 异步加载、裁剪填充、可点击
            Image {
                id: thumbnailImage
                anchors.fill: parent
                source: imageBubble.loaded ? "file:///" + imageBubble.imagePath : ""
                fillMode: Image.PreserveAspectCrop
                visible: imageBubble.loaded
                asynchronous: true
                cache: true
                smooth: true
                clip: true

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    enabled: imageBubble.loaded
                    onClicked: imageBubble.clicked()
                }
            }
        }

        // 图片说明文字 — 仅 caption 非空时显示
        Text {
            visible: imageBubble.caption !== ""
            text: imageBubble.caption
            color: imageBubble.isSelf ? "#FFFFFF" : "#1A1A2E"
            font.pixelSize: 14
            font.family: "Microsoft YaHei"
            wrapMode: Text.Wrap
            width: Math.min(imageBubble.thumbnailSize, imageBubble.imageWidth > 0 ? imageBubble.imageWidth : imageBubble.thumbnailSize)
        }

        // 时间 + 已编辑徽章 — 己方右对齐，对方左对齐
        Row {
            spacing: 4
            layoutDirection: imageBubble.isSelf ? Qt.RightToLeft : Qt.LeftToRight

            Text {
                text: imageBubble.displayTime
                color: imageBubble.isSelf ? Qt.rgba(1.0, 1.0, 1.0, 0.55) : "#9C9AAA"
                font.pixelSize: 11
                font.family: "Microsoft YaHei"
            }
            Text {
                visible: imageBubble.edited
                text: qsTr("已编辑")
                color: "#9C9AAA"
                font.pixelSize: 11
                font.family: "Microsoft YaHei"
                font.italic: true
            }
        }
    }

    // 组件完成时触发渐显动画（撤回时跳过）
    Component.onCompleted: {
        if (!imageBubble.recalled) {
            bubbleRect.opacity = 1
        }
    }

    // 右键 MouseArea（Phase 6）— 与左键 MouseArea（line 128）不冲突，acceptedButtons 不同
    MouseArea {
        anchors.fill: bubbleRect
        acceptedButtons: Qt.RightButton
        z: 1
        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                // Phase B — 用 mapToItem 把局部坐标映射到 chatViewRoot 坐标系
                var root = imageBubble
                while (root && root.objectName !== "chatViewRoot") {
                    root = root.parent
                }
                if (!root) {
                    console.warn("[ImageBubble] cannot find chatViewRoot")
                    return
                }
                var pt = bubbleRect.mapToItem(root, mouse.x, mouse.y)
                imageBubble.rightClicked(pt.x, pt.y, imageBubble.timestamp)
            }
        }
    }

    signal clicked()
    signal rightClicked(real x, real y, var timestamp)
}
