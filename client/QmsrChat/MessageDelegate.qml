/**
 * @file MessageDelegate.qml
 * @brief 消息代理组件 — 根据消息类型选择文字或图片气泡
 * @details Phase 5B.1 从 ChatView.qml 提取，封装 Loader + 两个 Component。
 *          作为 ListView delegate 使用，model 上下文自动可用。
 */
import QtQuick
import QtQuick.Controls

Loader {
    id: delegateRoot

    // ── 信号：向上层传递用户交互事件 ──
    signal actionMenuRequested(real x, real y, var ts, bool isImage, bool isSelf, string content)
    signal imageClicked(string imageId)
    signal retryRequested(var ts)

    // ── recalled/edited 变更同步 ──
    property bool _recalled: model.recalled
    property bool _edited: model.edited
    on_RecalledChanged: {
        console.log("[RECALL-LOADER] _recalled=" + _recalled + " item=" + (item ? "valid" : "null"))
        if (item) item.recalled = _recalled
    }
    on_EditedChanged: {
        if (item) item.edited = _edited
    }

    sourceComponent: model.messageType === 1 ? imageBubbleComponent : textBubbleComponent

    // ── 文字消息气泡组件 ──
    Component {
        id: textBubbleComponent
        MessageBubble {
            viewWidth: chatViewRoot.width
            isSelf: model.isSelf
            content: model.content
            timestamp: model.timestamp           // qint64 ms
            displayTime: model.displayTime       // UI string
            status: model.status
            recalled: model.recalled
            edited: model.edited
            onRightClicked: function(localX, localY, ts) {
                delegateRoot.actionMenuRequested(localX, localY, ts,
                    /*isImage*/ false, model.isSelf, model.content)
            }
            onRetryRequested: function(ts) {
                delegateRoot.retryRequested(ts)
            }
        }
    }

    // ── 图片消息气泡组件 ──
    Component {
        id: imageBubbleComponent
        ImageBubble {
            viewWidth: chatViewRoot.width
            isSelf: model.isSelf
            imagePath: model.imagePath
            caption: model.content
            imageWidth: model.imageWidth
            imageHeight: model.imageHeight
            edited: model.edited
            recalled: model.recalled
            timestamp: model.timestamp           // qint64 ms
            displayTime: model.displayTime       // UI string
            onClicked: delegateRoot.imageClicked(model.imageId)
            onRightClicked: function(localX, localY, ts) {
                delegateRoot.actionMenuRequested(localX, localY, ts,
                    /*isImage*/ true, model.isSelf, model.content)
            }
        }
    }
}
