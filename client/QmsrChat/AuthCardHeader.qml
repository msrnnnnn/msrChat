import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: root
    property string iconText: ""
    property string title: ""
    property string subtitle: ""

    Layout.alignment: Qt.AlignHCenter
    spacing: 8

    Rectangle {
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 52
        Layout.preferredHeight: 52
        radius: 14
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#4F46E5" }
            GradientStop { position: 1.0; color: "#818CF8" }
        }
        Text {
            anchors.centerIn: parent
            text: root.iconText
            color: "#FFFFFF"
            font.pixelSize: 20
            font.weight: Font.Bold
        }
    }

    Text {
        Layout.alignment: Qt.AlignHCenter
        text: root.title
        color: "#1A1A2E"
        font.pixelSize: 19
        font.family: "Microsoft YaHei"
        font.weight: Font.DemiBold
    }

    Text {
        Layout.alignment: Qt.AlignHCenter
        text: root.subtitle
        color: "#9C9AAA"
        font.pixelSize: 13
    }
}
