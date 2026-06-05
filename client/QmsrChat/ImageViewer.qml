/**
 * @file ImageViewer.qml
 * @brief 全屏图片查看器（点击 ImageBubble 后弹出）
 * @details Phase 5 MVP — 支持工具栏缩放、滚轮缩放、双击重置、左右翻页、Esc/点外区关闭。
 *          PinchHandler/DragHandler（双指缩放 + 拖拽平移）v1 暂不实现。
 *          工具栏图标用 Unicode 字符（⊕⊖⛶↻⤓✕），后续 v2 替换为 Material Symbols。
 */
import QtQuick
import QtQuick.Controls

Rectangle {
    id: viewer
    color: "#111111"
    focus: true

    // 公共输入
    property var imageList: []        // [{imageId, imagePath, caption}, ...]
    property int currentIndex: 0
    property real scaleFactor: 1.0

    // 内部状态
    property int rotationAngle: 0

    // 键盘快捷键
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            viewer.closeRequested()
            event.accepted = true
        } else if (event.key === Qt.Key_Left) {
            viewer.prev()
            event.accepted = true
        } else if (event.key === Qt.Key_Right) {
            viewer.next()
            event.accepted = true
        } else if (event.key === Qt.Key_Plus || event.key === Qt.Key_Equal) {
            viewer.zoom(1.25)
            event.accepted = true
        } else if (event.key === Qt.Key_Minus) {
            viewer.zoom(0.8)
            event.accepted = true
        } else if (event.key === Qt.Key_0) {
            viewer.scaleFactor = 1.0
            viewer.rotationAngle = 0
            event.accepted = true
        }
    }

    function currentItem() {
        if (imageList.length === 0) return null
        if (currentIndex < 0 || currentIndex >= imageList.length) return null
        return imageList[currentIndex]
    }

    function next() {
        if (currentIndex < imageList.length - 1) {
            currentIndex++
            viewer.scaleFactor = 1.0
            viewer.rotationAngle = 0
        }
    }

    function prev() {
        if (currentIndex > 0) {
            currentIndex--
            viewer.scaleFactor = 1.0
            viewer.rotationAngle = 0
        }
    }

    function zoom(f) {
        viewer.scaleFactor = Math.max(0.25, Math.min(8.0, viewer.scaleFactor * f))
    }

    // 透明背景层（点击图片外区域关闭）
    MouseArea {
        anchors.fill: parent
        z: 0
        onClicked: viewer.closeRequested()
    }

    // 居中图片
    Image {
        id: img
        anchors.centerIn: parent
        z: 1
        source: {
            var item = viewer.currentItem()
            return item ? "file:///" + item.imagePath : ""
        }
        fillMode: Image.PreserveAspectFit
        transformOrigin: Item.Center
        rotation: viewer.rotationAngle
        scale: viewer.scaleFactor
        asynchronous: true
        cache: true
        smooth: true
        visible: status === Image.Ready || status === Image.Loading

        // 滚轮缩放
        WheelHandler {
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: function(event) {
                if (event.angleDelta.y > 0) {
                    viewer.zoom(1.25)
                } else if (event.angleDelta.y < 0) {
                    viewer.zoom(0.8)
                }
                event.accepted = true
            }
        }

        // 双击切换 1:1 / 2x（v1 简化：1.0 ↔ 2.0）
        MouseArea {
            anchors.fill: parent
            onDoubleClicked: {
                viewer.scaleFactor = (viewer.scaleFactor > 1.05) ? 1.0 : 2.0
            }
            onClicked: function(mouse) {
                // 单击图片不关闭（防止误触）
                mouse.accepted = true
            }
        }
    }

    // 加载中占位
    Rectangle {
        anchors.centerIn: parent
        z: 1
        width: 240
        height: 80
        color: "#222222"
        radius: 6
        visible: viewer.imageList.length > 0 && img.status === Image.Loading

        Text {
            anchors.centerIn: parent
            text: qsTr("加载中…")
            color: "#888888"
            font.pixelSize: 14
            font.family: "Microsoft YaHei"
        }
    }

    // 加载失败占位
    Rectangle {
        anchors.centerIn: parent
        z: 1
        width: 280
        height: 100
        color: "#222222"
        radius: 6
        visible: viewer.imageList.length > 0 && img.status === Image.Error

        Column {
            anchors.centerIn: parent
            spacing: 8
            Text {
                text: qsTr("图片加载失败")
                color: "#cccccc"
                font.pixelSize: 14
                font.family: "Microsoft YaHei"
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                text: qsTr("点击任意处关闭")
                color: "#888888"
                font.pixelSize: 11
                font.family: "Microsoft YaHei"
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }

    // 标题（左上）
    Text {
        id: titleText
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 24
        z: 2
        text: {
            var item = viewer.currentItem()
            return item && item.caption ? item.caption : ""
        }
        color: Qt.rgba(1.0, 1.0, 1.0, 0.75)
        font.pixelSize: 13
        font.family: "Microsoft YaHei"
        elide: Text.ElideRight
        width: Math.min(implicitWidth, parent.width - 200)
    }

    // 关闭按钮（右上）
    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 18
        z: 2
        width: 36
        height: 36
        radius: 18
        color: closeMouse.containsMouse ? Qt.rgba(60/255, 60/255, 60/255, 0.85) : Qt.rgba(40/255, 40/255, 40/255, 0.7)

        Text {
            anchors.centerIn: parent
            text: "✕"
            color: "#FFFFFF"
            font.pixelSize: 18
        }

        MouseArea {
            id: closeMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: viewer.closeRequested()
        }
    }

    // 翻页左箭头
    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: 18
        anchors.verticalCenter: parent.verticalCenter
        z: 2
        width: 44
        height: 44
        radius: 22
        color: viewer.currentIndex > 0
               ? (prevMouse.containsMouse ? Qt.rgba(60/255, 60/255, 60/255, 0.85) : Qt.rgba(40/255, 40/255, 40/255, 0.6))
               : Qt.rgba(40/255, 40/255, 40/255, 0.3)

        Text {
            anchors.centerIn: parent
            text: "‹"
            color: "#FFFFFF"
            font.pixelSize: 24
        }

        MouseArea {
            id: prevMouse
            anchors.fill: parent
            hoverEnabled: true
            enabled: viewer.currentIndex > 0
            cursorShape: viewer.currentIndex > 0 ? Qt.PointingHandCursor : Qt.ForbiddenCursor
            onClicked: viewer.prev()
        }
    }

    // 翻页右箭头
    Rectangle {
        anchors.right: parent.right
        anchors.rightMargin: 18
        anchors.verticalCenter: parent.verticalCenter
        z: 2
        width: 44
        height: 44
        radius: 22
        color: viewer.currentIndex < viewer.imageList.length - 1
               ? (nextMouse.containsMouse ? Qt.rgba(60/255, 60/255, 60/255, 0.85) : Qt.rgba(40/255, 40/255, 40/255, 0.6))
               : Qt.rgba(40/255, 40/255, 40/255, 0.3)

        Text {
            anchors.centerIn: parent
            text: "›"
            color: "#FFFFFF"
            font.pixelSize: 24
        }

        MouseArea {
            id: nextMouse
            anchors.fill: parent
            hoverEnabled: true
            enabled: viewer.currentIndex < viewer.imageList.length - 1
            cursorShape: viewer.currentIndex < viewer.imageList.length - 1 ? Qt.PointingHandCursor : Qt.ForbiddenCursor
            onClicked: viewer.next()
        }
    }

    // 提示（工具栏上方）
    Text {
        anchors.bottom: toolbar.top
        anchors.bottomMargin: 12
        anchors.horizontalCenter: parent.horizontalCenter
        z: 2
        text: qsTr("滚轮缩放 · 双击切换 · ← → 翻页 · Esc 关闭")
        color: Qt.rgba(1.0, 1.0, 1.0, 0.45)
        font.pixelSize: 11
        font.family: "Microsoft YaHei"
    }

    // 底部工具栏
    Rectangle {
        id: toolbar
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 18
        anchors.horizontalCenter: parent.horizontalCenter
        z: 2
        height: 52
        width: toolbarRow.width + 16
        radius: 26
        color: Qt.rgba(40/255, 40/255, 40/255, 0.85)

        Row {
            id: toolbarRow
            anchors.centerIn: parent
            spacing: 0

            Repeater {
                model: [
                    { icon: "⊕",  tip: qsTr("放大 (+)"),       action: function() { viewer.zoom(1.25) } },
                    { icon: "⊖",  tip: qsTr("缩小 (−)"),       action: function() { viewer.zoom(0.8) } },
                    { icon: "1:1", tip: qsTr("1:1 实际大小"),  action: function() { viewer.scaleFactor = 1.0; viewer.rotationAngle = 0 } },
                    { icon: "⛶",  tip: qsTr("适应窗口"),        action: function() { viewer.scaleFactor = 1.0; viewer.rotationAngle = 0 } },
                    { sep: true },
                    { icon: "↻",  tip: qsTr("旋转"),           action: function() { viewer.rotationAngle = (viewer.rotationAngle + 90) % 360 } },
                    { icon: "⤓",  tip: qsTr("另存为…"),         action: function() { viewer.saveRequested() } },
                    { sep: true },
                    { icon: "✕",  tip: qsTr("关闭"),           action: function() { viewer.closeRequested() } }
                ]
                delegate: Loader {
                    sourceComponent: modelData.sep ? sepComp : btnComp
                    Component {
                        id: sepComp
                        Rectangle {
                            width: 1
                            height: 24
                            color: Qt.rgba(1.0, 1.0, 1.0, 0.15)
                        }
                    }
                    Component {
                        id: btnComp
                        Rectangle {
                            id: btnRoot
                            width: 44
                            height: 44
                            radius: 22
                            color: btnMouse.containsMouse ? Qt.rgba(1.0, 1.0, 1.0, 0.12) : "transparent"

                            Text {
                                anchors.centerIn: parent
                                text: modelData.icon
                                color: "#FFFFFF"
                                font.pixelSize: modelData.icon === "1:1" ? 11 : 18
                                font.family: "Microsoft YaHei"
                            }

                            MouseArea {
                                id: btnMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: modelData.action()
                            }

                            ToolTip.visible: btnMouse.containsMouse
                            ToolTip.delay: 500
                            ToolTip.text: modelData.tip
                        }
                    }
                }
            }
        }
    }

    // 组件激活时强制获得焦点（接收键盘事件）
    Component.onCompleted: viewer.forceActiveFocus()

    signal closeRequested()
    signal saveRequested()
}
