import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: registerRoot

    signal switchToLogin()

    property bool isSubmitting: false
    property string errorMessage: ""
    property string successMessage: ""
    property int verifyCountdown: 0
    property int successCountdown: 5
    property int currentPage: 1
    property real preferredHeight: currentPage === 1
        ? formColumn.implicitHeight + 64
        : successCol.implicitHeight + 64

    Rectangle {
        visible: currentPage === 1
        anchors.fill: parent
        color: "#FFFFFF"
        radius: 16
        border.width: 1
        border.color: "#EAE9F2"

        clip: true

        // 顶部渐变装饰条 — Canvas 绘制，跟随卡片圆角裁切
        Canvas {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 16
            z: 1
            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.beginPath()
                ctx.moveTo(0, 16)
                ctx.arcTo(0, 0, 16, 0, 16)
                ctx.lineTo(width - 16, 0)
                ctx.arcTo(width, 0, width, 16, 16)
                ctx.lineTo(width, 16)
                ctx.closePath()
                ctx.clip()
                var grad = ctx.createLinearGradient(0, 0, width, 0)
                grad.addColorStop(0, "#4F46E5")
                grad.addColorStop(0.5, "#818CF8")
                grad.addColorStop(1, "#4F46E5")
                ctx.fillStyle = grad
                ctx.fillRect(0, 0, width, 3)
            }
        }

        ColumnLayout {
            id: formColumn
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
                    Text { anchors.centerIn: parent; text: "📝"; font.pixelSize: 20 }
                }
                Text { Layout.alignment: Qt.AlignHCenter; text: qsTr("创建账户"); color: "#1A1A2E"; font.pixelSize: 19; font.weight: Font.DemiBold }
                Text { Layout.alignment: Qt.AlignHCenter; text: qsTr("注册一个新的 msrChat 账户"); color: "#9C9AAA"; font.pixelSize: 13 }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: regErrLabel.visible ? regErrLabel.implicitHeight + 16 : 0
                visible: errorMessage !== "" || successMessage !== ""
                radius: 6
                color: successMessage !== "" ? "#F0FDF4" : "#FEF2F2"
                border.width: 1
                border.color: successMessage !== "" ? "#BBF7D0" : "#FECACA"
                Text {
                    id: regErrLabel
                    anchors.centerIn: parent
                    text: errorMessage !== "" ? errorMessage : successMessage
                    color: errorMessage !== "" ? "#EF4444" : "#10B981"
                    font.pixelSize: 12
                }
            }

            ColumnLayout { spacing: 4; Layout.fillWidth: true
                Text { text: qsTr("用户名"); color: "#6B6A7F"; font.pixelSize: 12; font.weight: Font.DemiBold }
                TextField {
                    id: regUser
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    placeholderText: qsTr("输入用户名")
                    font.pixelSize: 14
                    color: "#1A1A2E"
                    background: Rectangle { color: "#F8F9FE"; radius: 8; border.width: 1.5; border.color: regUser.activeFocus ? "#4F46E5" : "#EAE9F2" }
                }
            }

            ColumnLayout { spacing: 4; Layout.fillWidth: true
                Text { text: qsTr("邮箱"); color: "#6B6A7F"; font.pixelSize: 12; font.weight: Font.DemiBold }
                TextField {
                    id: regEmail
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    placeholderText: qsTr("输入邮箱地址")
                    font.pixelSize: 14
                    color: "#1A1A2E"
                    background: Rectangle { color: "#F8F9FE"; radius: 8; border.width: 1.5; border.color: regEmail.activeFocus ? "#4F46E5" : "#EAE9F2" }
                }
            }

            ColumnLayout { spacing: 4; Layout.fillWidth: true
                Text { text: qsTr("密码"); color: "#6B6A7F"; font.pixelSize: 12; font.weight: Font.DemiBold }
                TextField {
                    id: regPass
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    echoMode: regPassShow.checked ? TextInput.Normal : TextInput.Password
                    placeholderText: qsTr("至少 6 位")
                    font.pixelSize: 14
                    color: "#1A1A2E"
                    background: Rectangle { color: "#F8F9FE"; radius: 8; border.width: 1.5; border.color: regPass.activeFocus ? "#4F46E5" : "#EAE9F2" }
                    Button {
                        id: regPassShow
                        checkable: true
                        anchors.right: parent.right
                        anchors.rightMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        height: 30
                        text: checked ? qsTr("隐藏") : qsTr("显示")
                        font.pixelSize: 12
                        flat: true
                        contentItem: Text { text: parent.text; color: parent.checked ? "#4F46E5" : "#9C9AAA"; font: parent.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: regPassShow.hovered ? "#EEF2FF" : "transparent"; radius: 6 }
                    }
                }
            }

            ColumnLayout { spacing: 4; Layout.fillWidth: true
                Text { text: qsTr("确认密码"); color: "#6B6A7F"; font.pixelSize: 12; font.weight: Font.DemiBold }
                TextField {
                    id: regConf
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    echoMode: regConfShow.checked ? TextInput.Normal : TextInput.Password
                    placeholderText: qsTr("再次输入密码")
                    font.pixelSize: 14
                    color: "#1A1A2E"
                    background: Rectangle { color: "#F8F9FE"; radius: 8; border.width: 1.5; border.color: regConf.activeFocus ? "#4F46E5" : "#EAE9F2" }
                    Button {
                        id: regConfShow
                        checkable: true
                        anchors.right: parent.right
                        anchors.rightMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        height: 30
                        text: checked ? qsTr("隐藏") : qsTr("显示")
                        font.pixelSize: 12
                        flat: true
                        contentItem: Text { text: parent.text; color: parent.checked ? "#4F46E5" : "#9C9AAA"; font: parent.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: regConfShow.hovered ? "#EEF2FF" : "transparent"; radius: 6 }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                ColumnLayout { spacing: 4; Layout.fillWidth: true
                    Text { text: qsTr("验证码"); color: "#6B6A7F"; font.pixelSize: 12; font.weight: Font.DemiBold }
                    TextField {
                        id: regCode
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        placeholderText: qsTr("输入验证码")
                        font.pixelSize: 14
                        color: "#1A1A2E"
                        background: Rectangle { color: "#F8F9FE"; radius: 8; border.width: 1.5; border.color: regCode.activeFocus ? "#4F46E5" : "#EAE9F2" }
                    }
                }
                Item {
                    Layout.preferredWidth: 72
                    Layout.preferredHeight: 40
                    Layout.alignment: Qt.AlignBottom
                    Button {
                        id: regVerifyBtn
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
                            authController.sendRegisterVerifyCode(regEmail.text.trim())
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
                    contentItem: Text { text: qsTr("确认注册"); color: parent.enabled ? "#FFFFFF" : "#9C9AAA"; font.pixelSize: 14; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: !parent.enabled ? "#EAE9F2" : parent.hovered ? "#3730A3" : "#4F46E5"; radius: 8 }
                    onClicked: {
                        errorMessage = ""
                        successMessage = ""
                        isSubmitting = true
                        authController.registerUser(regUser.text.trim(), regEmail.text.trim(), regPass.text, regConf.text, regCode.text.trim())
                    }
                }
                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    contentItem: Text { text: qsTr("取消"); color: parent.hovered ? "#4F46E5" : "#6B6A7F"; font.pixelSize: 14; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: parent.hovered ? "#EEF2FF" : "#F8F9FE"; radius: 8; border.width: 1; border.color: parent.hovered ? "#E0E7FF" : "#EAE9F2" }
                    onClicked: registerRoot.switchToLogin()
                }
            }
        }
    }

    Rectangle {
        visible: currentPage === 2
        anchors.fill: parent
        color: "#FFFFFF"
        radius: 16
        border.width: 1
        border.color: "#EAE9F2"

        clip: true

        // 顶部渐变装饰条 — Canvas 绘制，跟随卡片圆角裁切
        Canvas {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 16
            z: 1
            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.beginPath()
                ctx.moveTo(0, 16)
                ctx.arcTo(0, 0, 16, 0, 16)
                ctx.lineTo(width - 16, 0)
                ctx.arcTo(width, 0, width, 16, 16)
                ctx.lineTo(width, 16)
                ctx.closePath()
                ctx.clip()
                var grad = ctx.createLinearGradient(0, 0, width, 0)
                grad.addColorStop(0, "#4F46E5")
                grad.addColorStop(0.5, "#818CF8")
                grad.addColorStop(1, "#4F46E5")
                ctx.fillStyle = grad
                ctx.fillRect(0, 0, width, 3)
            }
        }
        ColumnLayout {
            id: successCol
            anchors.fill: parent
            anchors.margins: 32
            spacing: 12
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 64
                height: 64
                radius: 32
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#D1FAE5" }
                    GradientStop { position: 1.0; color: "#A7F3D0" }
                }
                Text { anchors.centerIn: parent; text: "✅"; font.pixelSize: 28 }
            }
            Text { Layout.alignment: Qt.AlignHCenter; text: qsTr("注册成功"); color: "#1A1A2E"; font.pixelSize: 20; font.weight: Font.DemiBold }
            Text { Layout.alignment: Qt.AlignHCenter; text: qsTr("你已经成功创建 msrChat 账户\n现在可以登录开始聊天了"); color: "#9C9AAA"; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; lineHeight: 1.6 }
            Button {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 160
                Layout.preferredHeight: 42
                contentItem: Text { text: successCountdown > 0 ? qsTr("返回登录 (%1s)").arg(successCountdown) : qsTr("返回登录"); color: "#FFFFFF"; font.pixelSize: 14; font.weight: Font.Medium; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: "#4F46E5"; radius: 8 }
                onClicked: { successTimer.stop(); registerRoot.switchToLogin() }
            }
        }
    }

    Connections {
        target: authController
        function onRegisterResult(success, message) {
            isSubmitting = false
            if (success) { currentPage = 2; successCountdown = 5; successTimer.start() }
            else { errorMessage = message }
        }
        function onVerifyCodeResult(success, message) {
            if (success) { successMessage = message; verifyCountdown = 60; verifyTimer.start() }
            else { errorMessage = message }
        }
    }

    Timer { id: verifyTimer; interval: 1000; repeat: true; onTriggered: { verifyCountdown--; if (verifyCountdown <= 0) stop() } }
    Timer { id: successTimer; interval: 1000; repeat: true; onTriggered: { successCountdown--; if (successCountdown <= 0) { stop(); registerRoot.switchToLogin() } } }
}
