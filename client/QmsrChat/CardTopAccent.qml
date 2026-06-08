import QtQuick

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
