import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: resetRoot

    signal switchToLogin()

    property bool isSubmitting: false
    property string errorMessage: ""
    property string successMessage: ""
    property int verifyCountdown: 0

    Rectangle {
        anchors.fill: parent
        color: "#FFFFFF"
        radius: 16
        border.width: 1
        border.color: "#EAE9F2"
        clip: true

        // 顶部渐变装饰条（由父级 clip 裁切圆角）
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 4
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "#4F46E5" }
                GradientStop { position: 0.5; color: "#818CF8" }
                GradientStop { position: 1.0; color: "#4F46E5" }
            }
        }

        ColumnLayout {
            id: resetColumn
            anchors.fill: parent
            anchors.margins: 32
            spacing: 14

            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 8
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 52
                    height: 52
                    radius: 14
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#4F46E5" }
                        GradientStop { position: 1.0; color: "#818CF8" }
                    }
                    Text { anchors.centerIn: parent; text: "🔑"; font.pixelSize: 20 }
                }
                Text { Layout.alignment: Qt.AlignHCenter; text: qsTr("重置密码"); color: "#1A1A2E"; font.pixelSize: 19; font.weight: Font.DemiBold }
                Text { Layout.alignment: Qt.AlignHCenter; text: qsTr("通过邮箱验证重置密码"); color: "#9C9AAA"; font.pixelSize: 13 }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: resetErrLabel.visible ? resetErrLabel.implicitHeight + 16 : 0
                visible: errorMessage !== "" || successMessage !== ""
                radius: 6
                color: successMessage !== "" ? "#F0FDF4" : "#FEF2F2"
                border.width: 1
                border.color: successMessage !== "" ? "#BBF7D0" : "#FECACA"
                Text {
                    id: resetErrLabel
                    anchors.centerIn: parent
                    text: errorMessage !== "" ? errorMessage : successMessage
                    color: errorMessage !== "" ? "#EF4444" : "#10B981"
                    font.pixelSize: 12
                }
            }

            ColumnLayout { spacing: 4; Layout.fillWidth: true
                Text { text: qsTr("用户名"); color: "#6B6A7F"; font.pixelSize: 12; font.weight: Font.DemiBold }
                TextField {
                    id: resetUser
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    placeholderText: qsTr("输入用户名")
                    font.pixelSize: 14
                    color: "#1A1A2E"
                    background: Rectangle { color: "#F8F9FE"; radius: 8; border.width: 1.5; border.color: resetUser.activeFocus ? "#4F46E5" : "#EAE9F2" }
                }
            }

            ColumnLayout { spacing: 4; Layout.fillWidth: true
                Text { text: qsTr("邮箱"); color: "#6B6A7F"; font.pixelSize: 12; font.weight: Font.DemiBold }
                TextField {
                    id: resetEmail
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    placeholderText: qsTr("输入注册邮箱")
                    font.pixelSize: 14
                    color: "#1A1A2E"
                    background: Rectangle { color: "#F8F9FE"; radius: 8; border.width: 1.5; border.color: resetEmail.activeFocus ? "#4F46E5" : "#EAE9F2" }
                }
            }

            ColumnLayout { spacing: 4; Layout.fillWidth: true
                Text { text: qsTr("新密码"); color: "#6B6A7F"; font.pixelSize: 12; font.weight: Font.DemiBold }
                TextField {
                    id: resetPass
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    echoMode: resetPassShow.checked ? TextInput.Normal : TextInput.Password
                    placeholderText: qsTr("至少 6 位")
                    font.pixelSize: 14
                    color: "#1A1A2E"
                    background: Rectangle { color: "#F8F9FE"; radius: 8; border.width: 1.5; border.color: resetPass.activeFocus ? "#4F46E5" : "#EAE9F2" }
                    Button {
                        id: resetPassShow
                        checkable: true
                        anchors.right: parent.right
                        anchors.rightMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        height: 30
                        text: checked ? qsTr("隐藏") : qsTr("显示")
                        font.pixelSize: 12
                        flat: true
                        contentItem: Text { text: parent.text; color: parent.checked ? "#4F46E5" : "#9C9AAA"; font: parent.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: resetPassShow.hovered ? "#EEF2FF" : "transparent"; radius: 6 }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                ColumnLayout { spacing: 4; Layout.fillWidth: true
                    Text { text: qsTr("验证码"); color: "#6B6A7F"; font.pixelSize: 12; font.weight: Font.DemiBold }
                    TextField {
                        id: resetCode
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        placeholderText: qsTr("输入验证码")
                        font.pixelSize: 14
                        color: "#1A1A2E"
                        background: Rectangle { color: "#F8F9FE"; radius: 8; border.width: 1.5; border.color: resetCode.activeFocus ? "#4F46E5" : "#EAE9F2" }
                    }
                }
                Item {
                    Layout.preferredWidth: 72
                    Layout.preferredHeight: 40
                    Layout.alignment: Qt.AlignBottom
                    Button {
                        anchors.fill: parent
                        text: verifyCountdown > 0 ? verifyCountdown + "s" : qsTr("获取")
                        enabled: verifyCountdown === 0
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        contentItem: Text { text: parent.text; color: parent.enabled ? "#4F46E5" : "#9C9AAA"; font: parent.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: parent.enabled ? "#EEF2FF" : "#F8F9FE"; radius: 8; border.width: 1; border.color: parent.enabled ? "#4F46E5" : "#EAE9F2" }
                        onClicked: {
                            errorMessage = ""
                            successMessage = ""
                            var code = Math.floor(100000 + Math.random() * 900000)
                            successMessage = "验证码: " + code
                            verifyCountdown = 60
                            verifyTimer.start()
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    enabled: !isSubmitting
                    contentItem: Text { text: qsTr("确认重置"); color: parent.enabled ? "#FFFFFF" : "#9C9AAA"; font.pixelSize: 14; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: !parent.enabled ? "#EAE9F2" : parent.hovered ? "#3730A3" : "#4F46E5"; radius: 8 }
                    onClicked: {
                        errorMessage = ""
                        successMessage = ""
                        isSubmitting = true
                        authController.resetPassword(resetUser.text.trim(), resetEmail.text.trim(), resetPass.text, resetCode.text.trim())
                    }
                }
                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    contentItem: Text { text: qsTr("返回"); color: parent.hovered ? "#4F46E5" : "#6B6A7F"; font.pixelSize: 14; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: parent.hovered ? "#EEF2FF" : "#F8F9FE"; radius: 8; border.width: 1; border.color: parent.hovered ? "#E0E7FF" : "#EAE9F2" }
                    onClicked: resetRoot.switchToLogin()
                }
            }
        }
    }

    Connections {
        target: authController
        function onResetPasswordResult(success, message) {
            isSubmitting = false
            if (success) { successMessage = message; resetTimer.start() }
            else { errorMessage = message }
        }
        function onVerifyCodeResult(success, message) {
            if (success) { successMessage = message; verifyCountdown = 60; verifyTimer.start() }
            else { errorMessage = message }
        }
    }

    Timer { id: verifyTimer; interval: 1000; repeat: true; onTriggered: { verifyCountdown--; if (verifyCountdown <= 0) stop() } }
    Timer { id: resetTimer; interval: 2000; onTriggered: resetRoot.switchToLogin() }
}
