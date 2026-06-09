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
    property real preferredHeight: resetColumn.implicitHeight + 64

    Rectangle {
        anchors.fill: parent
        color: "#FFFFFF"
        radius: 16
        border.width: 1
        border.color: "#EAE9F2"

        clip: true

        CardTopAccent {}

        ColumnLayout {
            id: resetColumn
            anchors.fill: parent
            anchors.margins: 32
            spacing: 14

            AuthCardHeader {
                iconText: "🔑"
                title: qsTr("重置密码")
                subtitle: qsTr("通过邮箱验证重置密码")
            }

            AuthBanner {
                id: banner
                errorMessage: resetRoot.errorMessage
                successMessage: resetRoot.successMessage
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

            PasswordField {
                id: resetPass
                label: qsTr("新密码")
                placeholder: qsTr("至少 6 位")
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
                        id: resetVerifyBtn
                        anchors.fill: parent
                        text: verifyCountdown > 0 ? verifyCountdown + "s" : qsTr("获取")
                        enabled: verifyCountdown === 0
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        contentItem: Text { text: parent.text; color: parent.enabled ? "#4F46E5" : "#9C9AAA"; font: parent.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: parent.enabled ? (parent.hovered ? "#DBEAFE" : "#EEF2FF") : "#F8F9FE"; radius: 8; border.width: 1; border.color: parent.enabled ? "#4F46E5" : "#EAE9F2" }
                        onClicked: {
                            errorMessage = ""
                            successMessage = ""
                            authController.sendRegisterVerifyCode(resetEmail.text.trim())
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
