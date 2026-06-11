/**
 * @file ChatView.qml
 * @brief 现代化聊天界面视图（组合层）
 * @details Phase 5B 重构：ChatView 仅负责组件组合与信号路由，
 *          具体 UI 逻辑已拆分到 ChatHeader / MessageDelegate / MessageInputArea /
 *          ImagePreviewBar / FileProgressPanel / EmptyState 等子组件。
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml

Rectangle {
    id: chatViewRoot
    objectName: "chatViewRoot"   // Phase B — MessageBubble.mapToItem 上溯查找
    color: "#F8F9FE"

    // ── 状态属性（通过 Connections 监听 chatController 信号更新） ──
    property int currentUid: 0
    property int targetUid: 0
    property bool isConnected: false
    property string pendingImagePath: ""

    // 监听 chatController 状态信号
    Connections {
        target: chatController
        function onSigConnectionStatusChanged() {
            console.log("[ChatView] onSigConnectionStatusChanged, isConnected =", chatController.isConnected)
            isConnected = chatController.isConnected
        }
        function onSigCurrentUidChanged() {
            console.log("[ChatView] onSigCurrentUidChanged, currentUid =", chatController.currentUid)
            currentUid = chatController.currentUid
        }
        function onSigTargetUidChanged() {
            console.log("[ChatView] onSigTargetUidChanged, targetUid =", chatController.targetUid)
            targetUid = chatController.targetUid
        }
    }

    // ── 主布局 ──
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ChatHeader {
            isConnected: chatViewRoot.isConnected
            currentUid: chatViewRoot.currentUid
            targetUid: chatViewRoot.targetUid
            onTargetUidChangeRequested: function(uid) {
                chatController.setTargetUid(uid)
            }
        }

        // 消息列表 — Loader 按消息类型选择气泡组件
        ListView {
            id: messageListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 10
            spacing: 4
            verticalLayoutDirection: ListView.TopToBottom
            clip: true
            cacheBuffer: 2000
            model: _chatModel

            delegate: MessageDelegate {
                width: messageListView.width - 16
                onActionMenuRequested: function(x, y, ts, isImage, isSelf, content) {
                    chatViewRoot.showActionMenu(x, y, ts, isImage, isSelf, content)
                }
                onImageClicked: function(imageId) {
                    chatController.openImageViewer(imageId)
                }
            }

            ScrollBar.vertical: ScrollBar {
                width: 8
                anchors.right: parent.right
                anchors.rightMargin: 0
                policy: ScrollBar.AsNeeded
                background: Rectangle { color: "#EAE9F2"; radius: 4 }
                contentItem: Rectangle { color: "#9C9AAA"; radius: 4 }
            }

            onCountChanged: {
                if (_chatModel && _chatModel.isPrepending())
                    return
                Qt.callLater(function() { positionViewAtEnd() })
            }
        }

        // 错误横幅
        Rectangle {
            id: errorBanner
            Layout.fillWidth: true
            Layout.preferredHeight: _errorHeight
            color: "#EF4444"
            visible: _errorHeight > 0
            clip: true
            property int _errorHeight: 0
            Behavior on Layout.preferredHeight {
                NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
            }
            Text {
                id: errorBannerText
                anchors.centerIn: parent
                color: "#FFFFFF"
                font.pixelSize: 13
            }
            Timer {
                id: errorBannerTimer
                interval: 4000
                onTriggered: errorBanner._errorHeight = 0
            }
            function show(msg) {
                errorBannerText.text = msg
                _errorHeight = 32
                errorBannerTimer.restart()
            }
        }

        ImagePreviewBar {
            pendingImagePath: chatViewRoot.pendingImagePath
            onSendRequested: function(path, caption) {
                chatController.sendImage(path, caption)
                chatViewRoot.pendingImagePath = ""
            }
            onCancelled: {
                chatViewRoot.pendingImagePath = ""
            }
        }

        MessageInputArea {
            id: messageInputArea
            isConnected: chatViewRoot.isConnected
            onMessageSendRequested: function(text) {
                chatController.sendMessage(text)
            }
            onImageSelected: function(path) {
                chatViewRoot.pendingImagePath = path
            }
            onFileSelected: function(path) {
                chatController.sendFile(path)
            }
        }
    }

    // ── 空状态引导 ──
    EmptyState {
        anchors.fill: parent
        visible: messageListView.count === 0
    }

    // ── 文件传输进度面板 ──
    FileProgressPanel {
        id: fileProgressPanel
    }

    // ── chatController 文件/图片信号路由 ──
    Connections {
        target: chatController

        function onSigError(errorMsg) {
            console.error("[Chat]: " + errorMsg)
            errorBanner.show(errorMsg)
        }

        function onSigFileSendStarted(task_id, filename, total_size) {
            fileProgressPanel.onSendStarted(String(task_id), filename, total_size)
        }
        function onSigFileSendProgress(task_id, prog, sent, total) {
            fileProgressPanel.onSendProgress(String(task_id), prog, sent, total)
        }
        function onSigFileSendComplete(task_id, success, error) {
            fileProgressPanel.onSendComplete(String(task_id), success, error)
        }
        function onSigFileRecvStarted(task_id, filename, total_size) {
            fileProgressPanel.onRecvStarted(String(task_id), filename, total_size)
        }
        function onSigFileRecvProgress(task_id, prog, received, total) {
            fileProgressPanel.onRecvProgress(String(task_id), prog, received, total)
        }
        function onSigFileRecvComplete(task_id, filepath, success, error) {
            fileProgressPanel.onRecvComplete(String(task_id), filepath, success, error)
        }

        // 回复菜单：在输入框插入前缀
        function onSigSetReplyContext(prefix) {
            messageInputArea.insertReplyPrefix(prefix)
        }
    }

    // ── _chatModel 滚动信号 ──
    Connections {
        target: _chatModel
        function onScrollToBottomRequested() {
            messageListView.positionViewAtEnd()
        }
        function onScrollToTopRequested() {
            messageListView.positionViewAtIndex(0, ListView.Beginning)
        }
    }

    // ── ImageViewer modal ──
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

    // ── 编辑消息 modal ──
    Loader {
        id: editDialogLoader
        anchors.fill: parent
        active: false
        z: 1001
        sourceComponent: EditMessageDialog {
            messageTimestamp: editDialogLoader.editTs
            originalContent: editDialogLoader.editOrig
            onAccepted: function(ts, newContent) {
                chatController.actionEdit(ts, newContent)
                editDialogLoader.active = false
            }
            onCancelled: editDialogLoader.active = false
        }
        property var editTs: 0
        property string editOrig: ""
    }

    // ── 右键操作菜单 ──
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
            onEditRequested: {
                var ts = actionMenuLoader.menuTimestamp
                editDialogLoader.editTs = ts
                editDialogLoader.editOrig = _chatModel.GetContentByTimestamp(ts)
                editDialogLoader.active = true
                actionMenuLoader.active = false
            }
            onDeleteRequested: { chatController.actionDelete(actionMenuLoader.menuTimestamp); actionMenuLoader.active = false }
        }
        property real menuX: 0
        property real menuY: 0
        property var menuTimestamp: 0
        property bool menuIsImage: false
        property bool menuIsOwn: false
        property bool menuHasCaption: true
    }

    // showActionMenu — 供 MessageDelegate.onActionMenuRequested 调用
    function showActionMenu(globalX, globalY, ts, isImage, isOwn, content) {
        var realTs = (typeof ts === "string") ? Number(ts) : ts
        var menuW = 180
        var menuH = 250
        var maxX = chatViewRoot.width  - menuW - 4
        var maxY = chatViewRoot.height - menuH - 4
        actionMenuLoader.menuX = Math.min(Math.max(0, globalX), Math.max(0, maxX))
        actionMenuLoader.menuY = Math.min(Math.max(0, globalY), Math.max(0, maxY))
        actionMenuLoader.menuTimestamp = realTs
        actionMenuLoader.menuIsImage = isImage
        actionMenuLoader.menuIsOwn = isOwn
        actionMenuLoader.menuHasCaption = (content && content.length > 0)
        actionMenuLoader.active = true
    }
}
