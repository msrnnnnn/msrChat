/**
 * @file EmptyState.qml
 * @brief 空状态引导覆盖层 — 无消息时居中显示引导文案
 * @details Phase 5B 附加提取自 ChatView.qml。
 */
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: emptyStateRoot
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
