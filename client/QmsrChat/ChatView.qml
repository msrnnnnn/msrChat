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
                    font.weight: Font.DemiBold
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
                    Layout.preferredWidth: 72
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
                        color: parent.enabled ? (parent.hovered ? "#3730A3" : "#4F46E5") : "#EAE9F2"
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

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 0

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

            // 底部分隔线
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: "#EAE9F2"
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
                width: messageListView.width - 16
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
                anchors.rightMargin: 0
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

        // 错误横幅 — 位于消息列表和输入框之间，紧贴输入框上方
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

        // 图片内嵌预览条 — 对齐 HTML .ipb：72px 高 (12 + 48 + 12)，缩略图用 Canvas 圆角裁切
        Rectangle {
            id: imagePreviewBar
            Layout.fillWidth: true
            Layout.preferredHeight: pendingImagePath !== "" ? 72 : 0
            visible: pendingImagePath !== ""
            clip: true
            color: "#EEF2FF"

            Behavior on Layout.preferredHeight {
                NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
            }

            // 顶部紫色分隔线
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: "#E0E7FF"
            }

            RowLayout {
                anchors.fill: parent
                anchors.topMargin: 12
                anchors.bottomMargin: 12
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                // 缩略图（48x48，圆角 8，紫色边框 — clip 裁切）
                Rectangle {
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    radius: 8
                    clip: true
                    color: "#EEF2FF"

                    Image {
                        anchors.fill: parent
                        source: pendingImagePath
                        fillMode: Image.PreserveAspectCrop
                    }

                    // 紫色边框 overlay
                    Rectangle {
                        anchors.fill: parent
                        radius: 8
                        color: "transparent"
                        border.width: 1
                        border.color: "#E0E7FF"
                    }
                }

                // Caption 输入框（34px 高，圆角 8，紫色系边框）
                TextField {
                    id: inlineCaptionInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    placeholderText: qsTr("添加图片说明（可选）")
                    maximumLength: 200
                    font.pixelSize: 13
                    font.family: "Microsoft YaHei"
                    color: "#1A1A2E"
                    background: Rectangle {
                        color: "#FFFFFF"
                        radius: 8
                        border.width: 1
                        border.color: inlineCaptionInput.activeFocus ? "#4F46E5" : "#E0E7FF"
                    }
                }

                // 发送图片按钮
                Button {
                    id: imgPreviewSendBtn
                    text: qsTr("发送")
                    Layout.preferredWidth: 60
                    Layout.preferredHeight: 34

                    contentItem: Text {
                        text: parent.text
                        color: "#FFFFFF"
                        font.pixelSize: 12
                        font.family: "Microsoft YaHei"
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: imgPreviewSendBtn.hovered ? "#3730A3" : "#4F46E5"
                        radius: 8
                    }

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
                    id: imgPreviewCancelBtn
                    text: qsTr("✕")
                    Layout.preferredWidth: 34
                    Layout.preferredHeight: 34
                    flat: true

                    contentItem: Text {
                        text: parent.text
                        color: imgPreviewCancelBtn.hovered ? "#1A1A2E" : "#9C9AAA"
                        font.pixelSize: 12
                        font.family: "Microsoft YaHei"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: "transparent"
                        radius: 8
                    }

                    onClicked: {
                        pendingImagePath = ""
                        inlineCaptionInput.text = ""
                    }
                }
            }
        }

        // 消息输入区域 — 参照 HTML .ia：工具栏(左) | Flickable+TextArea(flex) | 发送按钮(右)
        Rectangle {
            id: inputArea
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(Math.max(44, messageInput.implicitHeight), 120) + 16
            Layout.maximumHeight: 136  // 120 max text + 16 padding
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

                // 消息输入框 — Flickable + TextArea 实现长文本可鼠标滚动
                Flickable {
                    id: messageFlickable
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignBottom
                    Layout.preferredHeight: Math.min(Math.max(44, messageInput.implicitHeight), 120)
                    Layout.maximumHeight: 120
                    Layout.minimumHeight: 44
                    clip: true
                    contentHeight: messageInput.implicitHeight
                    boundsMovement: Flickable.StopAtBounds
                    flickableDirection: Flickable.VerticalFlick

                    TextArea.flickable: TextArea {
                        id: messageInput
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

                    ScrollBar.vertical: ScrollBar {
                        width: 6
                        anchors.right: parent.right
                        anchors.rightMargin: 2
                        policy: ScrollBar.AsNeeded
                        background: Rectangle {
                            color: "transparent"
                        }
                        contentItem: Rectangle {
                            color: "#C4C4D4"
                            radius: 3
                            implicitWidth: 6
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
                            GradientStop { position: 0.0; color: sendButton.enabled ? (sendButton.hovered ? "#3730A3" : "#4F46E5") : "#EAE9F2" }
                            GradientStop { position: 1.0; color: sendButton.enabled ? (sendButton.hovered ? "#4338CA" : "#6366F1") : "#EAE9F2" }
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

    // 文件传输进度数据模型
    ListModel {
        id: fileProgressModel
    }

    // 文件传输超时检测 — 如果 10 秒后 progress 仍为 0，标记为等待状态
    Timer {
        id: fileTransferTimeout
        interval: 10000
        repeat: false
        onTriggered: {
            for (var i = 0; i < fileProgressModel.count; i++) {
                var item = fileProgressModel.get(i)
                if (item.progress === 0 && item.error === "") {
                    console.log("[ChatView] File transfer timeout for task " + item.task_id)
                    fileProgressModel.setProperty(i, "error", "waiting")
                }
            }
        }
    }

    // 文件传输进度面板 — 右上角浮层，自定义渐变进度条 + 入场动画 + 完成后 3 秒淡化
    Item {
        id: fileProgressPanel
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 16
        anchors.rightMargin: 16
        width: 240
        height: Math.max(60, fileProgressList.contentHeight)
        visible: false
        opacity: 0
        z: 20

        // 入场动画
        NumberAnimation {
            id: entryAnim
            target: fileProgressPanel
            property: "opacity"
            from: 0; to: 1
            duration: 300
            easing.type: Easing.OutCubic
        }

        // 淡出动画
        NumberAnimation {
            id: fadeAnim
            target: fileProgressPanel
            property: "opacity"
            to: 0
            duration: 500
            easing.type: Easing.OutCubic
            onStopped: {
                fileProgressModel.clear()
                fileProgressPanel.visible = false
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
            if (fileProgressModel.count === 0) return
            for (var i = 0; i < fileProgressModel.count; i++) {
                var p = fileProgressModel.get(i).progress
                if (p >= 0 && p < 100) return
            }
            console.log("[ProgressPanel] All transfers done, scheduling fade in 3s")
            if (!fadeDelayTimer.running && !fadeAnim.running) {
                fadeDelayTimer.start()
            }
        }

        // 主体卡片
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

                        // 百分比 / 状态文字
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

    // 绑定 chatController 信号
    Connections {
        target: chatController
        function onSigError(errorMsg) {
            console.error("[Chat]: " + errorMsg)
            errorBanner.show(errorMsg)
        }

        function onSigFileSendStarted(task_id, filename, total_size) {
            var tid = String(task_id)
            console.log("[ChatView] FileSendStarted: task=" + tid + " file=" + filename + " size=" + total_size)
            fileProgressModel.append({"task_id": tid, "filename": filename, "progress": 0, "error": ""})
            fileProgressPanel.showPanel()
            fileTransferTimeout.restart()  // 启动 10 秒超时检测
        }

        function onSigFileSendProgress(task_id, prog, sent, total) {
            var tid = String(task_id)
            console.log("[ChatView] FileSendProgress: task=" + tid + " prog=" + prog + "% sent=" + sent + "/" + total)
            for (var i = 0; i < fileProgressModel.count; i++) {
                if (fileProgressModel.get(i).task_id === tid) {
                    fileProgressModel.setProperty(i, "progress", prog)
                    break
                }
            }
        }

        function onSigFileSendComplete(task_id, success, error) {
            var tid = String(task_id)
            console.log("[ChatView] FileSendComplete: task=" + tid + " success=" + success + " error=" + error)
            for (var i = 0; i < fileProgressModel.count; i++) {
                if (fileProgressModel.get(i).task_id === tid) {
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
            fileProgressPanel.checkAllComplete()
        }

        function onSigFileRecvProgress(task_id, prog, received, total) {
            var tid = String(task_id)
            console.log("[FileRecv] task=" + tid + " progress=" + prog + "%")
        }

        function onSigFileRecvComplete(task_id, filepath, success, error) {
            var tid = String(task_id)
            fileProgressPanel.showPanel()
            if (success) {
                console.log("[FileRecv] Complete: " + filepath)
                fileProgressModel.append({"task_id": tid, "filename": "已保存: " + filepath, "progress": 100, "error": ""})
            } else {
                console.error("[FileRecv] Failed: " + error)
                fileProgressModel.append({"task_id": tid, "filename": "接收失败", "progress": -1, "error": error})
            }
            fileProgressPanel.checkAllComplete()
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
