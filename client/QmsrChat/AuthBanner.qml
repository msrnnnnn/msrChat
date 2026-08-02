import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    property string errorMessage: ""
    property string successMessage: ""

    Layout.fillWidth: true
    visible: errorMessage !== "" || successMessage !== ""
    radius: 6
    color: successMessage !== "" ? "#F0FDF4" : "#FEF2F2"
    border.width: 1
    border.color: successMessage !== "" ? "#BBF7D0" : "#FECACA"

    implicitHeight: msgLabel.implicitHeight + 16

    Text {
        id: msgLabel
        anchors.centerIn: parent
        text: root.errorMessage !== "" ? root.errorMessage : root.successMessage
        color: root.errorMessage !== "" ? "#EF4444" : "#10B981"
        font.pixelSize: 12
    }
}
