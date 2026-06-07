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

    Rectangle {
        id: card
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
            height: 3
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "#4F46E5" }
                GradientStop { position: 0.5; color: "#818CF8" }
                GradientStop { position: 1.0; color: "#4F46E5" }
            }
        }

        ColumnLayout {
            id: cardColumn
            anchors.fill: parent
            anchors.margins: 32
            spacing: 16

            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 8
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 52; height: 52; radius: 14
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#4F46E5" }
                        GradientStop { position: 1.0; color: "#818CF8" }
                    }
                    Text { anchors.centerIn: parent; text: "mC"; color: "#FFFFFF"; font.pixelSize: 20; font.weight: Font.Bold }
                }
                Text { Layout.alignment: Qt.AlignHCenter; text: qsTr("欢迎回来"); color: "#1A1A2E"; font.pixelSize: 19; font.family: "Microsoft YaHei"; font.weight: Font.DemiBold }
                Text { Layout.alignment: Qt.AlignHCenter; text: qsTr("登录你的 msrChat 账户"); color: "#9C9AAA"; font.pixelSize: 13 }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: errorLabel.visible ? errorLabel.implicitHeight + 16 : 0
                visible: errorMessage !== "" || successMessage !== ""
                radius: 6
                color: successMessage !== "" ? "#F0FDF4" : "#FEF2F2"
                border.width: 1
                border.color: successMessage !== "" ? "#BBF7D0" : "#FECACA"
                Text { id: errorLabel; anchors.centerIn: parent; text: errorMessage !== "" ? errorMessage : successMessage; color: errorMessage !== "" ? "#EF4444" : "#10B981"; font.pixelSize: 12 }
            }

            ColumnLayout { spacing: 4; Layout.fillWidth: true
                Text { text: qsTr("用户名"); color: "#6B6A7F"; font.pixelSize: 12; font.weight: Font.Medium }
                TextField { id: loginUser; Layout.fillWidth: true; Layout.preferredHeight: 40; font.pixelSize: 14; placeholderText: qsTr("输入用户名或邮箱"); color: "#1A1A2E"
                    background: Rectangle { color: "#F8F9FE"; radius: 8; border.width: 1.5; border.color: loginUser.activeFocus ? "#4F46E5" : "#EAE9F2" }
                    Keys.onReturnPressed: function(event) { loginPass.forceActiveFocus() }
                    Keys.onEnterPressed: function(event) { loginPass.forceActiveFocus() }
                }
            }

            ColumnLayout { spacing: 4; Layout.fillWidth: true
                Text { text: qsTr("密码"); color: "#6B6A7F"; font.pixelSize: 12; font.weight: Font.Medium }
                TextField { id: loginPass; Layout.fillWidth: true; Layout.preferredHeight: 40; echoMode: showPassBtn.checked ? TextInput.Normal : TextInput.Password; font.pixelSize: 14; placeholderText: qsTr("输入密码"); color: "#1A1A2E"
                    background: Rectangle { color: "#F8F9FE"; radius: 8; border.width: 1.5; border.color: loginPass.activeFocus ? "#4F46E5" : "#EAE9F2" }
                    Keys.onReturnPressed: function(event) { if (loginBtn.enabled) loginBtn.clicked() }
                    Keys.onEnterPressed: function(event) { if (loginBtn.enabled) loginBtn.clicked() }
                    Button { id: showPassBtn; checkable: true; anchors.right: parent.right; anchors.rightMargin: 4; anchors.verticalCenter: parent.verticalCenter; height: 30; text: checked ? "隐藏" : "显示"; font.pixelSize: 12; flat: true
                        contentItem: Text { text: parent.text; color: parent.checked ? "#4F46E5" : "#9C9AAA"; font: parent.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: showPassBtn.hovered ? "#EEF2FF" : "transparent"; radius: 6 }
                    }
                }
                Text { Layout.alignment: Qt.AlignRight; text: qsTr("忘记密码？"); color: "#9C9AAA"; font.pixelSize: 12
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: loginRoot.switchToReset() }
                }
            }

            Button {
                id: loginBtn; Layout.fillWidth: true; Layout.preferredHeight: 42
                enabled: !isSubmitting && loginUser.text.trim().length > 0 && loginPass.text.trim().length > 0
                contentItem: Item {
                    Text { anchors.centerIn: parent; text: qsTr("登录"); color: loginBtn.enabled ? "#FFFFFF" : "#9C9AAA"; font.pixelSize: 14; font.weight: Font.Medium; visible: !isSubmitting }
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
                background: Rectangle { color: parent.enabled ? "#4F46E5" : "#EAE9F2"; radius: 8 }
                onClicked: { errorMessage = ""; successMessage = ""; isSubmitting = true; authController.login(loginUser.text.trim(), loginPass.text.trim()) }
            }

            Button {
                Layout.fillWidth: true; Layout.preferredHeight: 42
                contentItem: Text { anchors.centerIn: parent; text: qsTr("注册新账户"); color: "#6B6A7F"; font.pixelSize: 14; font.weight: Font.Bold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.hovered ? "#EEF2FF" : "#F8F9FE"; radius: 8; border.width: 1; border.color: "#EAE9F2" }
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
