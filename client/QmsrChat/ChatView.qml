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
            cacheBuffer: 2000

            model: chatModel

            delegate: Loader {
                width: messageListView.width - 12
                sourceComponent: model.messageType === 1 ? imageBubbleComponent : textBubbleComponent

                Component {
                    id: textBubbleComponent
                    MessageBubble {
                        viewWidth: chatViewRoot.width
                        isSelf: model.isSelf
                        content: model.content
                        timestamp: model.timestamp           // qint64 ms
                        displayTime: model.displayTime       // UI string
                        status: model.status
                        onRightClicked: function(localX, localY, ts) {
                            chatViewRoot.showActionMenu(localX, localY, ts,
                                /*isImage*/ false, model.isSelf, model.content)
                        }
                    }
                }
                Component {
                    id: imageBubbleComponent
                    ImageBubble {
                        viewWidth: chatViewRoot.width
                        isSelf: model.isSelf
                        imagePath: model.imagePath
                        caption: model.content
                        imageWidth: model.imageWidth
                        imageHeight: model.imageHeight
                        edited: model.edited
                        recalled: model.recalled
                        timestamp: model.timestamp           // qint64 ms
                        displayTime: model.displayTime       // UI string
                        onClicked: chatController.openImageViewer(model.imageId)
                        onRightClicked: function(localX, localY, ts) {
                            chatViewRoot.showActionMenu(localX, localY, ts,
                                /*isImage*/ true, model.isSelf, model.content)
                        }
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
                    fileDialog.open()
                }
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: "选择文件"
        onAccepted: {
            console.log("[ChatView] FileDialog onAccepted, selectedFile:", selectedFile.toString())
            if (chatController) {
                console.log("[ChatView] calling chatController.sendFile")
                chatController.sendFile(selectedFile.toString())
            }
        }
    }

    Rectangle {
        id: errorBanner
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: inputArea.top
        height: 0
        color: "#FF5252"
        visible: height > 0
        clip: true
        Behavior on height { NumberAnimation { duration: 300 } }

        Text {
            id: errorBannerText
            anchors.centerIn: parent
            color: "#FFFFFF"
            font.pixelSize: 13
        }

        Timer {
            id: errorBannerTimer
            interval: 4000
            onTriggered: errorBanner.height = 0
        }

        function show(msg) {
            errorBannerText.text = msg
            errorBanner.height = 32
            errorBannerTimer.restart()
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
                width: fileProgressList.width
                height: 40
                color: "transparent"

                // 关闭按钮
                Rectangle {
                    id: closeBtn
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    width: 16
                    height: 16
                    color: "transparent"
                    Text {
                        text: "×"
                        font.pixelSize: 14
                        font.bold: true
                        anchors.centerIn: parent
                        color: "#999999"
                    }
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: closeBtn.color = "#E0E0E0"
                        onExited: closeBtn.color = "transparent"
                        onClicked: {
                            for (var i = 0; i < fileProgressModel.count; i++) {
                                if (fileProgressModel.get(i).task_id === model.task_id) {
                                    fileProgressModel.remove(i)
                                    break
                                }
                            }
                        }
                    }
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: closeBtn.left
                    anchors.margins: 4
                    spacing: 2
                    Text {
                        text: filename
                        font.pixelSize: 12
                        color: "#333333"
                        elide: Text.ElideMiddle
                    }
                    ProgressBar {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        from: 0
                        to: 100
                        value: progress
                        visible: progress >= 0
                    }
                    Text {
                        text: progress >= 0 ? (progress + "%") : error
                        font.pixelSize: 10
                        color: progress < 0 ? "#FF5252" : "#666666"
                        visible: progress >= 0 || error !== ""
                    }
                }
            }
        }
    }

    Connections {
        target: chatController

        function onSigError(errorMsg) {
            console.error("[Chat]: " + errorMsg)
            errorBanner.show(errorMsg)
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
            console.log("[Chat] FileSendComplete, task_id:" + task_id + " success:" + success)
            // 找到并更新为完成状态，不立即移除，让用户看清
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
        }

        function onSigFileRecvProgress(task_id, prog, received, total) {
            // 接收进度暂不显示在发送进度面板，可由独立 UI 处理
            console.log("[FileRecv] task=" + task_id + " progress=" + prog + "%")
        }

        function onSigFileRecvComplete(task_id, filepath, success, error) {
            if (success) {
                console.log("[FileRecv] Complete: " + filepath)
                fileProgressModel.append({"task_id": task_id, "filename": "已保存: " + filepath, "progress": 100, "error": ""})
            } else {
                console.error("[FileRecv] Failed: " + error)
                fileProgressModel.append({"task_id": task_id, "filename": "接收失败", "progress": -1, "error": error})
            }
        }

        // Phase 6 — "回复"菜单项触发：在输入框插入"回复 XXX: "前缀并 focus
        function onSigSetReplyContext(prefix) {
            messageInput.text = prefix + messageInput.text
            messageInput.cursorPosition = prefix.length
            messageInput.forceActiveFocus()
        }
    }

    Connections {
        target: chatModel

        function onScrollToBottomRequested() {
            messageListView.positionViewAtEnd()
        }

        function onScrollToTopRequested() {
            messageListView.positionViewAtIndex(0, ListView.Beginning)
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

    // ImageViewer modal（Phase 5）— 接收 sigShowImageViewer 信号弹出
    Loader {
        id: imageViewerLoader
        anchors.fill: parent
        active: false
        z: 999
        sourceComponent: ImageViewer {
            imageList: imageViewerLoader.imageList
            currentIndex: imageViewerLoader.currentIndex
            onCloseRequested: imageViewerLoader.active = false
        }
        property var imageList: []
        property int currentIndex: 0
    }

    Connections {
        target: chatController
        function onSigShowImageViewer(list, idx) {
            imageViewerLoader.imageList = list
            imageViewerLoader.currentIndex = idx
            imageViewerLoader.active = true
        }
    }

    // === Phase 6 — 右键消息气泡弹操作菜单 ===

    // 菜单外区透明 MouseArea — 仅 active 时显示，z 999（低于 menuLoader z:1000，避免盖住菜单）
    MouseArea {
        anchors.fill: parent
        z: 999
        visible: actionMenuLoader.active
        onClicked: actionMenuLoader.active = false
    }

    Loader {
        id: actionMenuLoader
        active: false
        z: 1000
        x: menuX
        y: menuY
        sourceComponent: MessageActionMenu {
            isImage: actionMenuLoader.menuIsImage
            isOwn: actionMenuLoader.menuIsOwn
            messageTimestamp: actionMenuLoader.menuTimestamp
            hasCaption: actionMenuLoader.menuHasCaption
            onReplyRequested: { chatController.actionReply(actionMenuLoader.menuTimestamp); actionMenuLoader.active = false }
            onCopyTextRequested: { chatController.actionCopyText(actionMenuLoader.menuTimestamp); actionMenuLoader.active = false }
            onRecallRequested: { chatController.actionRecall(actionMenuLoader.menuTimestamp); actionMenuLoader.active = false }
            onEditRequested: { actionMenuLoader.active = false /* v1 stub */ }
            onSaveAsRequested: { chatController.actionSaveAs(actionMenuLoader.menuTimestamp); actionMenuLoader.active = false }
            onDeleteRequested: { chatController.actionDelete(actionMenuLoader.menuTimestamp); actionMenuLoader.active = false }
        }
        property real menuX: 0
        property real menuY: 0
        property var menuTimestamp: 0
        property bool menuIsImage: false
        property bool menuIsOwn: false
        property bool menuHasCaption: true
    }

    // 在 chatViewRoot 上暴露 showActionMenu 函数（bubble 的 onRightClicked 调用）
    function showActionMenu(localX, localY, ts, isImage, isOwn, content) {
        // 防御性:接收 string 也兼容(老代码万一遗漏)
        var realTs = (typeof ts === "string") ? Number(ts) : ts
        actionMenuLoader.menuX = Math.max(0, messageListView.contentX + localX)
        actionMenuLoader.menuY = Math.max(0, messageListView.contentY + localY)
        actionMenuLoader.menuTimestamp = realTs
        actionMenuLoader.menuIsImage = isImage
        actionMenuLoader.menuIsOwn = isOwn
        actionMenuLoader.menuHasCaption = (content && content.length > 0)
        actionMenuLoader.active = true
    }
}
