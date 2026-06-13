/**
 * @file EditMessageDialog.qml
 * @brief 消息编辑框 modal (Phase C)
 * @details 收 timestamp + originalContent,用户改完点确定 emit accepted(timestamp, newContent)
 *          上层 ChatView 接 accepted 调 chatController.actionEdit(ts, newContent)
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 编辑消息对话框 — 半透明遮罩 + 居中白色卡片，修改消息内容后确定/取消
Rectangle {
    id: dialogRoot
    anchors.fill: parent
    // 半透明黑色遮罩
    color: Qt.rgba(0.059, 0.059, 0.118, 0.25)

    // 要编辑的消息时间戳和原始内容
    property var messageTimestamp: 0    // qint64 via var
    property string originalContent: ""

    // 确定信号 — 携带 timestamp 和新内容
    signal accepted(var timestamp, string newContent)
    // 取消信号
    signal cancelled()

    // 拦截穿透点击 — 点击遮罩不触发底层事件
    MouseArea { anchors.fill: parent; onClicked: {} }

    // 居中对话框卡片
    Rectangle {
        id: card
        anchors.centerIn: parent
        width: 420
        height: 220
        radius: 16
        color: "#ffffff"
        border.color: "#EAE9F2"
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            // 对话框标题
            Text {
                text: qsTr("编辑消息")
                font.pixelSize: 16
                font.bold: true
                font.family: "Microsoft YaHei"
                color: "#1A1A2E"
            }

            // 可滚动的文本编辑区
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                TextArea {
                    id: editArea
                    text: dialogRoot.originalContent
                    wrapMode: TextArea.Wrap
                    font.pixelSize: 14
                    font.family: "Microsoft YaHei"
                    selectByMouse: true
                    selectByKeyboard: true
                    placeholderText: qsTr("输入新内容…")
                }
            }

            // 按钮行 — 右对齐，确定/取消
            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 8

                Button {
                    text: qsTr("确定")
                    enabled: editArea.text.trim().length > 0
                             && editArea.text.trim().length <= 4096
                             && editArea.text !== dialogRoot.originalContent

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

                    onClicked: dialogRoot.accepted(dialogRoot.messageTimestamp, editArea.text.trim())
                }
                Button {
                    text: qsTr("取消")

                    contentItem: Text {
                        text: parent.text
                        color: "#6B6A7F"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font: parent.font
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#EEF2FF" : "#F8F9FE"
                        radius: 6
                        border.width: 1
                        border.color: "#EAE9F2"
                    }

                    onClicked: dialogRoot.cancelled()
                }
            }
        }
    }
}