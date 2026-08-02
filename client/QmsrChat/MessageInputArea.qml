/**
 * @file MessageInputArea.qml
 * @brief 消息输入区域 — 工具栏 + 文本输入 + 发送按钮 + 文件/图片选择
 * @details Phase 5B.2 从 ChatView.qml 提取。
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Rectangle {
    id: inputAreaRoot

    // ── 属性 ──
    property bool isConnected: false

    // ── 信号 ──
    signal messageSendRequested(string text)
    signal imageSelected(string path)
    signal fileSelected(string path)

    Layout.fillWidth: true
    Layout.preferredHeight: Math.min(Math.max(44, messageInput.implicitHeight), 120) + 16
    Layout.maximumHeight: 136  // 120 max text + 16 padding
    color: "#FFFFFF"
    border.width: 1
    border.color: "#EAE9F2"

    // ── 公共方法：供回复功能插入前缀 ──
    function insertReplyPrefix(prefix) {
        messageInput.text = prefix + messageInput.text
        messageInput.cursorPosition = prefix.length
        messageInput.forceActiveFocus()
    }

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
                    inputAreaRoot.messageSendRequested(messageInput.text)
                    messageInput.text = ""
                }
            }
        }
    }

    // 通用文件选择对话框
    FileDialog {
        id: fileDialog
        title: "选择文件"
        onAccepted: {
            console.log("[MessageInputArea] FileDialog onAccepted, selectedFile:", selectedFile.toString())
            inputAreaRoot.fileSelected(selectedFile.toString())
        }
    }

    // 图片文件选择对话框
    FileDialog {
        id: imageFileDialog
        title: "选择图片"
        nameFilters: ["图片文件 (*.png *.jpg *.jpeg *.bmp *.gif *.webp)"]
        onAccepted: {
            inputAreaRoot.imageSelected(selectedFile.toString())
        }
    }
}
