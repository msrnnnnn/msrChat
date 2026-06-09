import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    property string label: ""
    property string placeholder: ""
    property alias text: inputField.text
    property alias field: inputField
    property alias enabled: inputField.enabled
    property var onAccepted: undefined

    spacing: 4
    Layout.fillWidth: true

    Text {
        text: root.label
        color: "#6B6A7F"
        font.pixelSize: 12
        font.weight: Font.DemiBold
    }

    TextField {
        id: inputField
        Layout.fillWidth: true
        Layout.preferredHeight: 40
        echoMode: showBtn.checked ? TextInput.Normal : TextInput.Password
        placeholderText: root.placeholder
        font.pixelSize: 14
        color: "#1A1A2E"
        background: Rectangle {
            color: "#F8F9FE"
            radius: 8
            border.width: 1.5
            border.color: inputField.activeFocus ? "#4F46E5" : "#EAE9F2"
        }
        Keys.onReturnPressed: if (root.onAccepted) root.onAccepted()
        Keys.onEnterPressed: if (root.onAccepted) root.onAccepted()

        Button {
            id: showBtn
            checkable: true
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            height: 30
            text: checked ? qsTr("隐藏") : qsTr("显示")
            font.pixelSize: 12
            flat: true
            contentItem: Text {
                text: parent.text
                color: parent.checked ? "#4F46E5" : "#9C9AAA"
                font: parent.font
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: showBtn.hovered ? "#EEF2FF" : "transparent"
                radius: 6
            }
        }
    }
}
