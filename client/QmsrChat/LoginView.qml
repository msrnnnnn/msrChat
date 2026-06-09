import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: loginRoot
    signal switchToRegister()
    signal switchToReset()
    property bool isSubmitting: false
    property string errorMessage: ""
    property string successMessage: ""
    property real preferredHeight: cardColumn.implicitHeight + 64  // 32px top + 32px bottom margin

    Rectangle {
        id: card
        anchors.fill: parent
        color: "#FFFFFF"
        radius: 16
        border.width: 1
        border.color: "#EAE9F2"

        clip: true

        CardTopAccent {}

        ColumnLayout {
            id: cardColumn
            anchors.fill: parent
            anchors.margins: 32
            spacing: 16

            AuthCardHeader {
                iconText: "mC"
                title: qsTr("欢迎回来")
                subtitle: qsTr("登录你的 msrChat 账户")
            }

            AuthBanner {
                id: banner
                errorMessage: loginRoot.errorMessage
                successMessage: loginRoot.successMessage
            }

            ColumnLayout { spacing: 4; Layout.fillWidth: true
                Text { text: qsTr("用户名"); color: "#6B6A7F"; font.pixelSize: 12; font.weight: Font.DemiBold }
                TextField { id: loginUser; Layout.fillWidth: true; Layout.preferredHeight: 40; font.pixelSize: 14; placeholderText: qsTr("输入用户名或邮箱"); color: "#1A1A2E"
                    background: Rectangle { color: "#F8F9FE"; radius: 8; border.width: 1.5; border.color: loginUser.activeFocus ? "#4F46E5" : "#EAE9F2" }
                    Keys.onReturnPressed: loginPass.field.forceActiveFocus()
                    Keys.onEnterPressed: loginPass.field.forceActiveFocus()
                }
            }

            PasswordField {
                id: loginPass
                label: qsTr("密码")
                placeholder: qsTr("输入密码")
                onAccepted: { if (loginBtn.enabled) loginBtn.clicked() }
            }
                Text { Layout.alignment: Qt.AlignRight; text: qsTr("忘记密码？"); color: "#9C9AAA"; font.pixelSize: 12
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: loginRoot.switchToReset() }
                }
            }

            Button {
                id: loginBtn; Layout.fillWidth: true; Layout.preferredHeight: 42
                enabled: !isSubmitting && loginUser.text.trim().length > 0 && loginPass.text.trim().length > 0
                contentItem: Item {
                    Text { anchors.centerIn: parent; text: qsTr("登录"); color: loginBtn.enabled ? "#FFFFFF" : "#9C9AAA"; font.pixelSize: 14; font.weight: Font.DemiBold; visible: !isSubmitting }
                    // 自定义旋转圆环（对齐 HTML .sp）
                    Item {
                        anchors.centerIn: parent; width: 18; height: 18
                        visible: isSubmitting
                        Rectangle {
                            anchors.fill: parent
                            radius: 9
                            color: "transparent"
                            border.width: 2
                            border.color: Qt.rgba(1, 1, 1, 0.3)
                        }
                        Rectangle {
                            width: 18; height: 18; radius: 9
                            color: "transparent"
                            border.width: 2
                            border.color: "transparent"
                            RotationAnimation on rotation {
                                from: 0; to: 360; duration: 600
                                loops: Animation.Infinite
                                running: isSubmitting
                            }
                            // 顶部白色弧段
                            Rectangle {
                                width: 4; height: 2; radius: 1
                                color: "#FFFFFF"
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.top: parent.top
                                anchors.topMargin: -1
                            }
                        }
                    }
                }
                background: Rectangle { color: !parent.enabled ? "#EAE9F2" : parent.hovered ? "#3730A3" : "#4F46E5"; radius: 8 }
                onClicked: { errorMessage = ""; successMessage = ""; isSubmitting = true; authController.login(loginUser.text.trim(), loginPass.text.trim()) }
            }

            Button {
                Layout.fillWidth: true; Layout.preferredHeight: 42
                contentItem: Text { anchors.centerIn: parent; text: qsTr("注册新账户"); color: parent.hovered ? "#4F46E5" : "#6B6A7F"; font.pixelSize: 14; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.hovered ? "#EEF2FF" : "#F8F9FE"; radius: 8; border.width: 1; border.color: parent.hovered ? "#E0E7FF" : "#EAE9F2" }
                onClicked: loginRoot.switchToRegister()
            }
        }
    }

    Connections {
        target: authController
        function onLoginResult(success, message) {
            isSubmitting = false
            if (success) { successMessage = message; errorMessage = "" }
            else { errorMessage = message; successMessage = "" }
        }
    }
}
