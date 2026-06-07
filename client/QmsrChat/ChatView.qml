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

// 聊天视图根组件，承载消息列表、输入区、操作菜单等全部子组件
Rectangle {
    id: chatViewRoot
    objectName: "chatViewRoot"   // Phase B — MessageBubble.mapToItem 上溯查找
    color: "#F8F9FE"

    // 当前用户 ID 和目标用户 ID，通过 Connections 监听信号更新
    property int currentUid: 0
    property int targetUid: 0
    // 连接状态，通过 Connections 监听信号更新
    property bool isConnected: false
    property string pendingImagePath: ""  // 待发送图片路径（选中后内嵌预览）

    // 监听 chatController 信号
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

    // 主布局：消息列表 + 图片预览条 + 输入区，纵向排列
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // === UID 输入栏 ===
        Rectangle {
            id: uidBar
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: "#F8F9FE"
            border.color: "#EAE9F2"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 12

                Text {
                    text: qsTr("目标 UID")
                    color: "#9C9AAA"
                    font.pixelSize: 12
                    font.family: "Microsoft YaHei"
                    font.weight: Font.Medium
                }

                TextField {
                    id: uidInput
                    Layout.preferredWidth: 110
                    Layout.preferredHeight: 32
                    font.pixelSize: 13
                    font.family: "Microsoft YaHei"
                    placeholderText: qsTr("输入 ID")
                    inputMethodHints: Qt.ImhDigitsOnly
                    maximumLength: 6
                    color: "#1A1A2E"

                    background: Rectangle {
                        color: "#FFFFFF"
                        radius: 6
                        border.width: 1.5
                        border.color: uidInput.activeFocus ? "#4F46E5" : "#EAE9F2"
                    }

                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Return) {
                            connectBtn.clicked()
                        }
                    }
                }

                Button {
                    id: connectBtn
                    text: qsTr("连接")
                    Layout.preferredHeight: 32
                    font.pixelSize: 12
                    font.family: "Microsoft YaHei"
                    font.weight: Font.DemiBold
                    enabled: uidInput.text.trim().length > 0 && !isConnecting

                    property bool isConnecting: false

                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? "#FFFFFF" : "#9C9AAA"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font: parent.font
                    }
                    background: Rectangle {
                        color: parent.enabled ? "#4F46E5" : "#EAE9F2"
                        radius: 6
                    }

                    onClicked: {
                        var uidText = uidInput.text.trim()
                        if (uidText.length === 0) return
                        var uid = parseInt(uidText)
                        if (isNaN(uid) || uid <= 0) return
                        if (!chatController) return
                        if (chatController.currentUid <= 0) chatController.initialize()

                        connectBtn.isConnecting = true
                        connectBtn.text = qsTr("…")
                        chatController.setTargetUid(uid)
                        connectTimer.start()
                    }
                }

                Timer {
                    id: connectTimer
                    interval: 800
                    onTriggered: {
                        connectBtn.isConnecting = false
                        connectBtn.text = qsTr("连接")
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32

                    RowLayout {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6

                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: isConnected ? "#10B981" : "#EF4444"
                            SequentialAnimation on color {
                                running: !isConnected
                                loops: Animation.Infinite
                                ColorAnimation { to: "#F59E0B"; duration: 500 }
                                ColorAnimation { to: "#EF4444"; duration: 500 }
                            }
                        }

                        Text {
                            text: isConnected ? qsTr("已连接") : qsTr("连接断开")
                            color: isConnected ? "#9C9AAA" : "#EF4444"
                            font.pixelSize: 11
                            font.family: "Microsoft YaHei"
                        }
                    }
                }
            }
        }

        // === 状态栏 ===
        Rectangle {
            id: statusBar
            Layout.fillWidth: true
            Layout.preferredHeight: 26
            color: "#F8F9FE"
            border.color: "#EAE9F2"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 6

                Rectangle {
                    width: 6; height: 6; radius: 3
                    color: isConnected ? "#10B981" : "#EF4444"
                }

                Text {
                    text: "当前UID: " + currentUid + " · 目标UID: " + (targetUid > 0 ? targetUid : "未选择")
                    color: "#9C9AAA"
                    font.pixelSize: 11
                    font.family: "Microsoft YaHei"
                }
            }
        }

        // 消息列表 — 使用 Loader 按消息类型（文字/图片）动态选择气泡组件
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

            // 根据消息类型选择气泡：type=1 图片气泡，否则文字气泡
            delegate: Loader {
                width: messageListView.width - 12
                sourceComponent: model.messageType === 1 ? imageBubbleComponent : textBubbleComponent
                property bool _recalled: model.recalled
                property bool _edited: model.edited
                // 当模型 recalled 字段变更时，同步到 Loader 内部的 item 实例
                on_RecalledChanged: {
                    console.log("[RECALL-LOADER] _recalled=" + _recalled + " item=" + (item ? "valid" : "null"))
                    if (item) item.recalled = _recalled
                }
                on_EditedChanged: {
                    if (item) item.edited = _edited
                }

                // 文字消息气泡组件
                Component {
                    id: textBubbleComponent
                    MessageBubble {
                        viewWidth: chatViewRoot.width
                        isSelf: model.isSelf
                        content: model.content
                        timestamp: model.timestamp           // qint64 ms
                        displayTime: model.displayTime       // UI string
                        status: model.status
                        recalled: model.recalled
                        edited: model.edited
                        onRightClicked: function(localX, localY, ts) {
                            chatViewRoot.showActionMenu(localX, localY, ts,
                                /*isImage*/ false, model.isSelf, model.content)
                        }
                    }
                }
                // 图片消息气泡组件
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

            // 垂直滚动条 — 浅灰配色，自动显示/隐藏
            ScrollBar.vertical: ScrollBar {
                width: 8
                anchors.right: parent.right
                anchors.rightMargin: 2
                policy: ScrollBar.AsNeeded
                background: Rectangle {
                    color: "#EAE9F2"
                    radius: 4
                }
                contentItem: Rectangle {
                    color: "#9C9AAA"
                    radius: 4
                }
            }

            // 新消息到达时自动滚动到底部
            onCountChanged: {
                Qt.callLater(function() {
                    positionViewAtEnd()
                })
            }
        }

        // 图片内嵌预览条 — 用户选择图片后显示缩略图、说明输入和发送/取消按钮
        Rectangle {
            id: imagePreviewBar
            Layout.fillWidth: true
            Layout.preferredHeight: pendingImagePath !== "" ? 72 : 0
            color: "#EEF2FF"
            visible: pendingImagePath !== ""
            clip: true

            Behavior on Layout.preferredHeight {
                NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                // 缩略图
                Image {
                    source: pendingImagePath
                    Layout.preferredWidth: 56
                    Layout.preferredHeight: 56
                    fillMode: Image.PreserveAspectCrop
                    clip: true

                    Rectangle {
                        anchors.fill: parent
                        radius: 6
                        color: "transparent"
                        border.width: 1
                        border.color: "#CCCCCC"
                    }
                }

                // Caption 输入
                TextField {
                    id: inlineCaptionInput
                    Layout.fillWidth: true
                    placeholderText: qsTr("添加图片说明（可选）")
                    maximumLength: 200
                    font.pixelSize: 13
                    background: Rectangle {
                        color: "#FFFFFF"
                        radius: 6
                        border.width: 1
                        border.color: "#EAE9F2"
                    }
                }

                // 发送图片按钮
                Button {
                    text: qsTr("发送")
                    Layout.preferredWidth: 60
                    Layout.preferredHeight: 36
                    highlighted: true
                    onClicked: {
                        if (chatController) {
                            chatController.sendImage(pendingImagePath, inlineCaptionInput.text)
                        }
                        pendingImagePath = ""
                        inlineCaptionInput.text = ""
                    }
                }

                // 取消按钮
                Button {
                    text: qsTr("✕")
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    flat: true
                    onClicked: {
                        pendingImagePath = ""
                        inlineCaptionInput.text = ""
                    }
                }
            }
        }

        // 消息输入区域 — 参照 HTML .ia：工具栏(左) | TextArea(flex) | 发送按钮(右)
        Rectangle {
            id: inputArea
            Layout.fillWidth: true
            Layout.preferredHeight: messageInput.implicitHeight + 28
            color: "#FFFFFF"
            border.width: 1
            border.color: "#EAE9F2"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                // 左侧工具栏（图片 + 文件按钮，透明背景，hover 变色）
                Row {
                    Layout.alignment: Qt.AlignBottom
                    Layout.bottomMargin: 4
                    spacing: 2

                    Button {
                        id: imageButton
                        width: 36; height: 36
                        text: qsTr("🖼")
                        font.pixelSize: 16
                        enabled: isConnected
                        flat: true
                        background: Rectangle {
                            color: imageButton.hovered ? "#F8F9FE" : "transparent"
                            radius: 6
                        }
                        contentItem: Text {
                            text: parent.text
                            color: parent.enabled ? "#9C9AAA" : "#D0D0D0"
                            font: parent.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            imageFileDialog.open()
                        }
                    }

                    Button {
                        id: fileButton
                        width: 36; height: 36
                        text: qsTr("📎")
                        font.pixelSize: 16
                        enabled: isConnected
                        flat: true
                        background: Rectangle {
                            color: fileButton.hovered ? "#F8F9FE" : "transparent"
                            radius: 6
                        }
                        contentItem: Text {
                            text: parent.text
                            color: parent.enabled ? "#9C9AAA" : "#D0D0D0"
                            font: parent.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            fileDialog.open()
                        }
                    }
                }

                // 消息输入框（flex:1，无边框，高度 44~120 自适应）
                TextArea {
                    id: messageInput
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignBottom
                    Layout.preferredHeight: Math.min(Math.max(44, implicitHeight), 120)
                    placeholderText: qsTr("输入消息… (Enter 发送, Shift+Enter 换行)")
                    placeholderTextColor: "#9C9AAA"
                    wrapMode: TextArea.Wrap
                    font.pixelSize: 14
                    font.family: "Microsoft YaHei"
                    padding: 10
                    color: "#1A1A2E"

                    background: Rectangle {
                        color: messageInput.activeFocus ? "#EEF2FF" : "#F8F9FE"
                        radius: 10
                    }

                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Return && !(event.modifiers & Qt.ShiftModifier)) {
                            event.accepted = true
                            sendButton.clicked()
                        }
                    }
                }

                // 发送按钮（44px 高，渐变紫色，底部对齐）
                Button {
                    id: sendButton
                    Layout.alignment: Qt.AlignBottom
                    Layout.preferredHeight: 44
                    Layout.preferredWidth: 80
                    text: qsTr("发送 →")
                    font.pixelSize: 14
                    font.bold: true
                    font.family: "Microsoft YaHei"
                    enabled: isConnected && messageInput.text.trim().length > 0

                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? "#FFFFFF" : "#9C9AAA"
                        font: parent.font
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: 8
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: sendButton.enabled ? "#4F46E5" : "#EAE9F2" }
                            GradientStop { position: 1.0; color: sendButton.enabled ? "#6366F1" : "#EAE9F2" }
                        }
                    }

                    onClicked: {
                        if (messageInput.text.trim().length > 0) {
                            chatController.sendMessage(messageInput.text)
                            messageInput.text = ""
                        }
                    }
                }
            }
        }
    }

    // === 空状态引导（覆盖层，居中显示） ===
    Rectangle {
        id: emptyState
        anchors.fill: parent
        visible: messageListView.count === 0
        color: "transparent"
        z: 1

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 12

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 72
                height: 72
                radius: 36
                color: "#EEF2FF"
                Text {
                    anchors.centerIn: parent
                    text: "💬"
                    font.pixelSize: 28
                }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("开始聊天")
                color: "#6B6A7F"
                font.pixelSize: 17
                font.family: "Microsoft YaHei"
                font.weight: Font.DemiBold
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("输入目标 UID 并点击「连接」\n即可开始对话")
                color: "#9C9AAA"
                font.pixelSize: 13
                font.family: "Microsoft YaHei"
                horizontalAlignment: Text.AlignHCenter
                lineHeight: 1.7
            }
        }
    }

    // 通用文件选择对话框 — 选中后直接发送文件
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

    // 图片文件选择对话框 — 选中后存入 pendingImagePath 触发内嵌预览
    FileDialog {
        id: imageFileDialog
        title: "选择图片"
        nameFilters: ["图片文件 (*.png *.jpg *.jpeg *.bmp *.gif *.webp)"]
        onAccepted: {
            pendingImagePath = selectedFile.toString()
        }
    }

    // 错误横幅 — 从底部滑入显示错误信息，4 秒后自动消失
    Rectangle {
        id: errorBanner
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 130  // inputArea height
        height: 0
        color: "#EF4444"
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

    // 文件传输进度数据模型
    ListModel {
        id: fileProgressModel
    }

    // 文件传输进度面板 — 右上角浮层，有任务时显示
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
        border.color: "#EAE9F2"
        radius: 10

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

                // 关闭按钮 — 点击移除该任务条目
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
                        color: "#9C9AAA"
                    }
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: closeBtn.color = "#EAE9F2"
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
                        color: "#1A1A2E"
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
                        color: progress < 0 ? "#EF4444" : "#6B6A7F"
                        visible: progress >= 0 || error !== ""
                    }
                }
            }
        }
    }

    // 绑定 chatController 信号
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

    // 绑定 _chatModel 信号 — 响应滚动到底部/顶部请求
    Connections {
        target: _chatModel

        function onScrollToBottomRequested() {
            messageListView.positionViewAtEnd()
        }

        function onScrollToTopRequested() {
            messageListView.positionViewAtIndex(0, ListView.Beginning)
        }
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

    // === Phase C — 编辑消息 modal ===
    Loader {
        id: editDialogLoader
        anchors.fill: parent
        active: false
        z: 1001  // 在 menu loader (z:1000) 之上
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
            onEditRequested: {
                var ts = actionMenuLoader.menuTimestamp
                editDialogLoader.editTs = ts
                editDialogLoader.editOrig = _chatModel.GetContentByTimestamp(ts)
                editDialogLoader.active = true
                actionMenuLoader.active = false
            }
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

 // 在 chatViewRoot 上暴露 showActionMenu(bubble 的 onRightClicked 调用)
    // Phase B — globalX/Y 来自 bubble.mapToItem(chatViewRoot, ...),菜单不会跑屏幕外
    function showActionMenu(globalX, globalY, ts, isImage, isOwn, content) {
        // globalX/Y 已是 chatViewRoot 坐标(bubble 用 mapToItem 转好),无需再加 contentX/Y
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
