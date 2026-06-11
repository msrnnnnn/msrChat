/**
 * @file FileProgressPanel.qml
 * @brief 文件传输进度面板 — 右上角浮层，渐变进度条 + 入场/淡出动画
 * @details Phase 5B 附加提取自 ChatView.qml。
 *          包含数据模型、超时检测、动画和进度 delegate。
 */
import QtQuick
import QtQuick.Controls

Item {
    id: panelRoot

    anchors.top: parent.top
    anchors.right: parent.right
    anchors.topMargin: 16
    anchors.rightMargin: 16
    width: 240
    height: Math.max(60, fileProgressList.contentHeight)
    visible: false
    opacity: 0
    z: 20

    property bool _timerActive: fadeDelayTimer.running

    // ── 数据模型 ──
    ListModel {
        id: fileProgressModel
    }

    // ── 超时检测（10 秒 progress 仍为 0 → 标记等待） ──
    Timer {
        id: fileTransferTimeout
        interval: 10000
        repeat: false
        onTriggered: {
            for (var i = 0; i < fileProgressModel.count; i++) {
                var item = fileProgressModel.get(i)
                if (item.progress === 0 && item.error === "") {
                    console.log("[FileProgressPanel] File transfer timeout for task " + item.task_id)
                    fileProgressModel.setProperty(i, "error", "waiting")
                }
            }
        }
    }

    // ── 入场动画 ──
    NumberAnimation {
        id: entryAnim
        target: panelRoot
        property: "opacity"
        from: 0; to: 1
        duration: 300
        easing.type: Easing.OutCubic
    }

    // ── 淡出动画 ──
    NumberAnimation {
        id: fadeAnim
        target: panelRoot
        property: "opacity"
        to: 0
        duration: 500
        easing.type: Easing.OutCubic
        onStopped: {
            console.log("[DIAG] fadeAnim stopped, fading complete")
            fileProgressModel.clear()
            panelRoot.visible = false
        }
    }

    Timer {
        id: fadeDelayTimer
        interval: 3000
        onTriggered: {
            console.log("[ProgressPanel] Starting fade-out animation")
            fadeAnim.start()
        }
    }

    function showPanel() {
        fadeDelayTimer.stop()
        fadeAnim.stop()
        visible = true
        opacity = 0
        entryAnim.start()
    }

    function checkAllComplete() {
        console.log("[DIAG] checkAllComplete called, count=" + fileProgressModel.count)
        if (fileProgressModel.count === 0) return
        for (var i = 0; i < fileProgressModel.count; i++) {
            var item = fileProgressModel.get(i)
            console.log("[DIAG]   item[" + i + "] progress=" + item.progress + " error=" + item.error)
            var p = item.progress
            if (p >= 0 && p < 100) return
        }
        console.log("[ProgressPanel] All transfers done, scheduling fade in 3s")
        if (!fadeDelayTimer.running && !fadeAnim.running) {
            fadeDelayTimer.start()
        }
    }

    // ── 公共 API（供 ChatView Connections 调用） ──
    function onSendStarted(task_id, filename, total_size) {
        console.log("[FileProgressPanel] FileSendStarted: task=" + task_id + " file=" + filename + " size=" + total_size)
        fileProgressModel.append({"task_id": task_id, "filename": filename, "progress": 0, "error": ""})
        showPanel()
        fileTransferTimeout.restart()
    }

    function onSendProgress(task_id, prog, sent, total) {
        console.log("[FileProgressPanel] FileSendProgress: task=" + task_id + " prog=" + prog + "%")
        for (var i = 0; i < fileProgressModel.count; i++) {
            if (fileProgressModel.get(i).task_id === task_id) {
                fileProgressModel.setProperty(i, "progress", prog)
                if (prog === 100) checkAllComplete()
                break
            }
        }
    }

    function onSendComplete(task_id, success, error) {
        console.log("[FileProgressPanel] FileSendComplete: task=" + task_id + " success=" + success)
        for (var i = 0; i < fileProgressModel.count; i++) {
            if (fileProgressModel.get(i).task_id === task_id) {
                if (success) {
                    fileProgressModel.setProperty(i, "filename", "已发送: " + fileProgressModel.get(i).filename)
                    fileProgressModel.setProperty(i, "progress", 100)
                    fileProgressModel.setProperty(i, "error", "")
                } else {
                    fileProgressModel.setProperty(i, "filename", "发送失败")
                    fileProgressModel.setProperty(i, "progress", -1)
                    fileProgressModel.setProperty(i, "error", error)
                }
                break
            }
        }
        checkAllComplete()
    }

    function onRecvStarted(task_id, filename, total_size) {
        console.log("[FileProgressPanel] FileRecvStarted: task=" + task_id + " file=" + filename)
        fileProgressModel.append({"task_id": task_id, "filename": filename, "progress": 0, "error": ""})
        showPanel()
    }

    function onRecvProgress(task_id, prog, received, total) {
        console.log("[FileProgressPanel] FileRecvProgress: task=" + task_id + " prog=" + prog + "%")
        for (var i = 0; i < fileProgressModel.count; i++) {
            if (fileProgressModel.get(i).task_id === task_id) {
                fileProgressModel.setProperty(i, "progress", prog)
                if (prog === 100) checkAllComplete()
                break
            }
        }
    }

    function onRecvComplete(task_id, filepath, success, error) {
        showPanel()
        if (success) {
            console.log("[FileProgressPanel] FileRecvComplete: " + filepath)
            fileProgressModel.append({"task_id": task_id, "filename": "已保存: " + filepath, "progress": 100, "error": ""})
        } else {
            console.error("[FileProgressPanel] FileRecvFailed: " + error)
            fileProgressModel.append({"task_id": task_id, "filename": "接收失败", "progress": -1, "error": error})
        }
        checkAllComplete()
    }

    // ── 主体卡片 ──
    Rectangle {
        anchors.fill: parent
        color: "#FFFFFF"
        border.width: 1
        border.color: "#EAE9F2"
        radius: 12
        clip: true

        ListView {
            id: fileProgressList
            anchors.fill: parent
            model: fileProgressModel
            interactive: false

            delegate: Item {
                width: fileProgressList.width
                height: fpCol.height + 16

                Column {
                    id: fpCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    anchors.topMargin: 8
                    spacing: 4

                    // 文件名行 + 关闭按钮
                    Row {
                        width: parent.width
                        spacing: 8

                        Text {
                            text: filename
                            font.pixelSize: 12
                            font.family: "Microsoft YaHei"
                            color: "#1A1A2E"
                            width: parent.width - 26
                            elide: Text.ElideMiddle
                        }

                        Rectangle {
                            id: fpCloseBtn
                            width: 18; height: 18; radius: 3
                            color: fpCloseMouse.containsMouse ? "#F8F9FE" : "transparent"

                            Text {
                                anchors.centerIn: parent
                                text: "×"
                                font.pixelSize: 14
                                color: "#9C9AAA"
                            }

                            MouseArea {
                                id: fpCloseMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    for (var i = 0; i < fileProgressModel.count; i++) {
                                        if (fileProgressModel.get(i).task_id === model.task_id) {
                                            fileProgressModel.remove(i)
                                            break
                                        }
                                    }
                                    if (fileProgressModel.count === 0) {
                                        fadeAnim.start()
                                    }
                                }
                            }
                        }
                    }

                    // 自定义 4px 渐变进度条
                    Rectangle {
                        width: parent.width
                        height: 4
                        radius: 2
                        color: "#F8F9FE"

                        Rectangle {
                            width: model.progress >= 0
                                ? Math.max(0, parent.width * Math.min(model.progress, 100) / 100)
                                : parent.width * 0.23
                            height: 4
                            radius: 2
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop {
                                    position: 0.0
                                    color: model.progress >= 0 ? "#4F46E5" : "#FCA5A5"
                                }
                                GradientStop {
                                    position: 1.0
                                    color: model.progress >= 0 ? "#818CF8" : "#EF4444"
                                }
                            }
                        }
                    }

                    // 百分比 / 状态文字 + 诊断圆点
                    Row {
                        spacing: 4

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 6; height: 6; radius: 3
                            color: panelRoot._timerActive ? "#F59E0B"
                                 : (model.progress >= 100 || model.progress < 0 ? "#10B981" : "#D1D5DB")
                        }

                        Text {
                            text: {
                                if (model.progress >= 100) return qsTr("已完成")
                                if (model.progress < 0) {
                                    return (model.error && model.error !== "") ? model.error : qsTr("失败")
                                }
                                if (model.progress === 0 && model.error === "waiting") return qsTr("等待对方响应...")
                                return model.progress + "%"
                            }
                            font.pixelSize: 10
                            font.family: "Microsoft YaHei"
                            color: model.progress < 0 ? "#EF4444" : "#9C9AAA"
                        }
                    }
                }

                // item 底部分隔线（最后一个除外）
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: "#EAE9F2"
                    visible: index < fileProgressModel.count - 1
                }
            }
        }
    }
}
