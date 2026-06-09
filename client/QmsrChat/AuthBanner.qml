import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string errorMessage: ""
    property string successMessage: ""

    Layout.fillWidth: true
    Layout.preferredHeight: msgLabel.visible ? msgLabel.implicitHeight + 16 : 0
    visible: errorMessage !== "" || successMessage !== ""
    radius: 6
    color: successMessage !== "" ? "#F0FDF4" : "#FEF2F2"
    border.width: 1
    border.color: successMessage !== "" ? "#BBF7D0" : "#FECACA"

    Text {
        id: msgLabel
        anchors.centerIn: parent
        text: errorMessage !== "" ? errorMessage : successMessage
        color: errorMessage !== "" ? "#EF4444" : "#10B981"
        font.pixelSize: 12
    }
}
