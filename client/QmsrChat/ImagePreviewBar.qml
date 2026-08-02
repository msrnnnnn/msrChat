/**
 * @file ImagePreviewBar.qml
 * @brief 图片内嵌预览条 — 缩略图 + Caption 输入 + 发送/取消按钮
 * @details Phase 5B 附加提取自 ChatView.qml。
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: previewBarRoot

    property string pendingImagePath: ""

    signal sendRequested(string path, string caption)
    signal cancelled()

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

        // Caption 输入框
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
                previewBarRoot.sendRequested(previewBarRoot.pendingImagePath, inlineCaptionInput.text)
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
                previewBarRoot.cancelled()
                inlineCaptionInput.text = ""
            }
        }
    }
}
