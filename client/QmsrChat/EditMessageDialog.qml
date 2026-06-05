/**
 * @file EditMessageDialog.qml
 * @brief 消息编辑框 modal (Phase C)
 * @details 收 timestamp + originalContent,用户改完点确定 emit accepted(timestamp, newContent)
 *          上层 ChatView 接 accepted 调 chatController.actionEdit(ts, newContent)
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: dialogRoot
    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.4)

    property var messageTimestamp: 0    // qint64 via var
    property string originalContent: ""

    signal accepted(var timestamp, string newContent)
    signal cancelled()

    // 拦截穿透点击(避免点遮罩反而触发底层)
    MouseArea { anchors.fill: parent; onClicked: {} }

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: 420
        height: 220
        radius: 10
        color: "#ffffff"
        border.color: "#dddddd"
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            Text {
                text: qsTr("编辑消息")
                font.pixelSize: 16
                font.bold: true
                font.family: "Microsoft YaHei"
                color: "#222222"
            }

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

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 8

                Button {
                    text: qsTr("取消")
                    onClicked: dialogRoot.cancelled()
                }
                Button {
                    text: qsTr("确定")
                    enabled: editArea.text.trim().length > 0
                             && editArea.text.trim().length <= 4096
                             && editArea.text !== dialogRoot.originalContent
                    onClicked: dialogRoot.accepted(dialogRoot.messageTimestamp, editArea.text.trim())
                }
            }
        }
    }
}