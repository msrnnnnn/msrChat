/**
 * @file MessageActionMenu.qml
 * @brief 右键消息气泡弹出的操作菜单（Phase 6）
 * @details 6 项菜单 + 分隔符 + hover + 2 分钟超时置灰。
 *          颜色沿用 mockup 03：#2b2b2b 深色背景，#fff 文字，#3a3a3a hover，
 *          #ff7a7a 危险项（删除），#777 disabled 文字。
 *          recall/edit 启用条件：isOwn && messageAgeSec < 120（单位：秒）。
 *          P6 决策（已与用户确认）：
 *          - "复制文字"在纯图无 caption 时隐藏（hasCaption=false）
 *          - "另存为"仅图片显示（isImage=true）
 *          - 菜单显隐由 ChatView 的 Loader 控制（active: true/false）
 */
import QtQuick

// 右键消息操作菜单 — 深色主题，6 项操作 + 分隔符，2 分钟内可撤回/编辑
Rectangle {
    id: menuRoot

    // 菜单上下文：图片/文字、己方消息、消息时间戳、是否有文字说明
    property bool isImage: false
    property bool isOwn: false
    property double messageTimestamp: 0  // 毫秒（QML 不支持 qint64，用 double；JS Number 53-bit 精度足够毫秒时间戳）
    property bool hasCaption: true

    // 撤回窗口：发送后 120 秒内可撤回/编辑
    readonly property int recallWindowSec: 120   // 2 分钟
    // 计算消息距今的秒数
    readonly property int messageAgeSec: {
        if (messageTimestamp <= 0) return recallWindowSec + 1  // 异常值视为超时
        return Math.floor((Date.now() - messageTimestamp) / 1000)
    }
    // 是否在撤回窗口内（己方 && 2 分钟内）
    readonly property bool isWithinRecallWindow: isOwn && messageAgeSec < recallWindowSec

    // 深色背景，圆角 8px
    color: "#2b2b2b"
    radius: 8
    width: 180
    height: column.implicitHeight + 12
    visible: opacity > 0

    // 菜单项数据模型 — 6 项操作 + 3 条分隔符，部分项根据条件控制可见性
    property var items: [
        { kind: "item", icon: "↩",  label: qsTr("回复"),       enabled: true,                sig: "replyRequested" },
        { kind: "item", icon: "📋", label: qsTr("复制文字"),   enabled: true,                sig: "copyTextRequested", visible: hasCaption },
        { kind: "sep" },
        { kind: "item", icon: "↶",  label: qsTr("撤回"),       enabled: isWithinRecallWindow, sig: "recallRequested" },
        { kind: "item", icon: "✎",  label: qsTr("编辑"),       enabled: isWithinRecallWindow, sig: "editRequested" },
        { kind: "sep" },
        { kind: "item", icon: "💾", label: qsTr("另存为…"),    enabled: true,                sig: "saveAsRequested", visible: isImage },
        { kind: "sep" },
        { kind: "item", icon: "🗑", label: qsTr("删除"),       enabled: true,                sig: "deleteRequested", danger: true }
    ]

    Column {
        id: column
        anchors.fill: parent
        anchors.topMargin: 6
        anchors.bottomMargin: 6
        spacing: 0

        Repeater {
            model: menuRoot.items
            delegate: Loader {
                width: column.width
                sourceComponent: modelData.kind === "sep" ? sepComp : itemComp
                visible: modelData.kind !== "item" || modelData.visible !== false

                Component {
                    id: sepComp
                    Rectangle {
                        width: column.width
                        height: 1
                        color: "#444"
                        anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
                    }
                }
                Component {
                    id: itemComp
                    Rectangle {
                        id: row
                        width: column.width
                        height: 32
                        color: itemMouse.containsMouse && modelData.enabled
                               ? "#3a3a3a" : "transparent"

                        Behavior on color {
                            ColorAnimation { duration: 150 }
                        }

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            spacing: 10

                            Text {
                                text: modelData.icon
                                color: !modelData.enabled ? "#777"
                                     : modelData.danger ? "#ff7a7a"
                                     : "#ffffff"
                                font.pixelSize: 14
                                width: 18
                                horizontalAlignment: Text.AlignHCenter
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: modelData.label
                                color: !modelData.enabled ? "#777"
                                     : modelData.danger ? "#ff7a7a"
                                     : "#ffffff"
                                font.pixelSize: 13
                                font.family: "Microsoft YaHei"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        MouseArea {
                            id: itemMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            enabled: modelData.enabled
                            cursorShape: modelData.enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
                            onClicked: {
                                if (!modelData.enabled) return
                                var sig = modelData.sig
                                if (sig === "replyRequested")      menuRoot.replyRequested()
                                else if (sig === "copyTextRequested") menuRoot.copyTextRequested()
                                else if (sig === "recallRequested")   menuRoot.recallRequested()
                                else if (sig === "editRequested")     menuRoot.editRequested()
                                else if (sig === "saveAsRequested")   menuRoot.saveAsRequested()
                                else if (sig === "deleteRequested")   menuRoot.deleteRequested()
                            }
                        }
                    }
                }
            }
        }
    }

    signal replyRequested()
    signal copyTextRequested()
    signal recallRequested()
    signal editRequested()
    signal saveAsRequested()
    signal deleteRequested()
}
