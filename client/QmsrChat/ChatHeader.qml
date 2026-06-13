/**
 * @file ChatHeader.qml
 * @brief 聊天头部 — UID 输入栏 + 状态栏
 * @details Phase 5B.3 从 ChatView.qml 提取。
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: headerRoot

    // ── 属性 ──
    property bool isConnected: false
    property int currentUid: 0
    property int targetUid: 0

    // ── 信号 ──
    signal targetUidChangeRequested(int uid)

    Layout.fillWidth: true
    spacing: 0

    // === UID 输入栏 ===
    Rectangle {
        id: uidBar
        Layout.fillWidth: true
        Layout.preferredHeight: 44
        color: "#F8F9FE"
        border.color: "#EAE9F2"
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            spacing: 12

            Text {
                text: qsTr("目标 UID")
                color: "#9C9AAA"
                font.pixelSize: 12
                font.family: "Microsoft YaHei"
                font.weight: Font.DemiBold
            }

            TextField {
                id: uidInput
                Layout.preferredWidth: 110
                Layout.preferredHeight: 32
                font.pixelSize: 13
                font.family: "Microsoft YaHei"
                placeholderText: qsTr("输入 ID")
                inputMethodHints: Qt.ImhDigitsOnly
                maximumLength: 6
                color: "#1A1A2E"

                background: Rectangle {
                    color: "#FFFFFF"
                    radius: 6
                    border.width: 1.5
                    border.color: uidInput.activeFocus ? "#4F46E5" : "#EAE9F2"
                }

                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Return) {
                        connectBtn.clicked()
                    }
                }
            }

            Button {
                id: connectBtn
                text: qsTr("连接")
                Layout.preferredHeight: 32
                Layout.preferredWidth: 72
                font.pixelSize: 12
                font.family: "Microsoft YaHei"
                font.weight: Font.DemiBold
                enabled: uidInput.text.trim().length > 0 && !isConnecting

                property bool isConnecting: false

                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? "#FFFFFF" : "#9C9AAA"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font: parent.font
                }
                background: Rectangle {
                    color: parent.enabled ? (parent.hovered ? "#3730A3" : "#4F46E5") : "#EAE9F2"
                    radius: 6
                }

                onClicked: {
                    var uidText = uidInput.text.trim()
                    if (uidText.length === 0) return
                    var uid = parseInt(uidText)
                    if (isNaN(uid) || uid <= 0) return
                    if (!chatController) return
                    if (chatController.currentUid <= 0) chatController.initialize()

                    connectBtn.isConnecting = true
                    connectBtn.text = qsTr("…")
                    headerRoot.targetUidChangeRequested(uid)
                    connectTimer.start()
                }
            }

            Timer {
                id: connectTimer
                interval: 800
                onTriggered: {
                    connectBtn.isConnecting = false
                    connectBtn.text = qsTr("连接")
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 32

                RowLayout {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: isConnected ? "#10B981" : "#EF4444"
                        SequentialAnimation on color {
                            running: !isConnected
                            loops: Animation.Infinite
                            ColorAnimation { to: "#F59E0B"; duration: 500 }
                            ColorAnimation { to: "#EF4444"; duration: 500 }
                        }
                    }

                    Text {
                        text: isConnected ? qsTr("已连接") : qsTr("连接断开")
                        color: isConnected ? "#9C9AAA" : "#EF4444"
                        font.pixelSize: 11
                        font.family: "Microsoft YaHei"
                    }
                }
            }
        }
    }

    // === 状态栏 ===
    Rectangle {
        id: statusBar
        Layout.fillWidth: true
        Layout.preferredHeight: 26
        color: "#F8F9FE"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            spacing: 0

            Rectangle {
                width: 6; height: 6; radius: 3
                color: isConnected ? "#10B981" : "#EF4444"
            }

            Text {
                text: "当前UID: " + currentUid + " · 目标UID: " + (targetUid > 0 ? targetUid : "未选择")
                color: "#9C9AAA"
                font.pixelSize: 11
                font.family: "Microsoft YaHei"
            }
        }

        // 底部分隔线
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: "#EAE9F2"
        }
    }
}
