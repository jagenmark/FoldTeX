import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: figureEditor
    objectName: "figureEditor"
    width: Math.min(820, parent ? parent.width - 32 : 820)
    height: Math.min(640, parent ? parent.height - 32 : 640)
    anchors.centerIn: parent
    modal: true
    focus: true
    padding: 12
    closePolicy: Popup.CloseOnEscape

    required property var backendApi
    required property string documentPath
    required property color backgroundColor
    required property color foregroundColor
    required property color mutedColor
    required property color accentColor
    required property string writingFont
    signal figureSaved(string path)
    signal saveFailed(string message)

    property string tool: "pen"
    property var actions: []
    property var currentAction: null
    property real textX: 0
    property real textY: 0

    function reset() {
        actions = []
        currentAction = null
        tool = "pen"
        textEditor.visible = false
        drawing.requestPaint()
    }

    function commitText() {
        if (!textEditor.visible) return
        var value = textEditor.text.trim()
        if (value.length) {
            var next = actions.slice()
            next.push({ type: "text", x: textX, y: textY,
                        text: value, size: 22 })
            actions = next
        }
        textEditor.visible = false
        textEditor.text = ""
        drawing.requestPaint()
    }

    function beginAction(x, y) {
        commitText()
        if (tool === "text") {
            textX = x
            textY = y
            textEditor.x = Math.min(x, drawingArea.width - textEditor.width)
            textEditor.y = Math.min(y - 20, drawingArea.height - textEditor.height)
            textEditor.visible = true
            textEditor.forceActiveFocus()
            return
        }
        currentAction = tool === "pen" || tool === "eraser"
                ? { type: tool, points: [{ x: x, y: y }] }
                : { type: tool, x1: x, y1: y, x2: x, y2: y }
        drawing.requestPaint()
    }

    function updateAction(x, y) {
        if (!currentAction) return
        if (currentAction.type === "pen" || currentAction.type === "eraser") {
            var points = currentAction.points.slice()
            points.push({ x: x, y: y })
            currentAction.points = points
        } else {
            currentAction.x2 = x
            currentAction.y2 = y
        }
        drawing.requestPaint()
    }

    function finishAction(x, y) {
        if (!currentAction) return
        updateAction(x, y)
        var next = actions.slice()
        next.push(currentAction)
        actions = next
        currentAction = null
        drawing.requestPaint()
    }

    function undo() {
        commitText()
        if (!actions.length) return
        var next = actions.slice()
        next.pop()
        actions = next
        drawing.requestPaint()
    }

    function saveFigure() {
        commitText()
        if (!actions.length) {
            saveFailed("Draw something before saving")
            return
        }
        var target = backendApi.newFigureAsset(documentPath)
        if (target.error) {
            saveFailed(target.error)
            return
        }
        if (!backendApi.saveFigure(target.path, JSON.stringify(actions),
                                Math.round(drawing.width), Math.round(drawing.height),
                                backgroundColor.toString(), foregroundColor.toString(),
                                writingFont)) {
            saveFailed("Could not save the figure")
            return
        }
        figureSaved(target.path)
        close()
    }

    onOpened: reset()

    Shortcut {
        sequence: StandardKey.Undo
        context: Qt.WindowShortcut
        enabled: figureEditor.visible
        onActivated: figureEditor.undo()
    }

    background: Rectangle {
        color: figureEditor.backgroundColor
        radius: 10
        border.width: 1
        border.color: Qt.rgba(figureEditor.foregroundColor.r,
                              figureEditor.foregroundColor.g,
                              figureEditor.foregroundColor.b, 0.18)
    }

    contentItem: ColumnLayout {
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Repeater {
                model: [
                    { tool: "pen", text: "✎", tip: "Pen" },
                    { tool: "eraser", text: "⌫", tip: "Eraser" },
                    { tool: "line", text: "╱", tip: "Line" },
                    { tool: "arrow", text: "→", tip: "Arrow" },
                    { tool: "box", text: "□", tip: "Box" },
                    { tool: "text", text: "T", tip: "Text" }
                ]
                delegate: ToolButton {
                    required property var modelData
                    objectName: "figureTool-" + modelData.tool
                    text: modelData.text
                    checkable: true
                    checked: figureEditor.tool === modelData.tool
                    onClicked: figureEditor.tool = modelData.tool
                    ToolTip.visible: hovered
                    ToolTip.text: modelData.tip
                    contentItem: Text {
                        text: parent.text
                        color: figureEditor.foregroundColor
                        font.family: figureEditor.writingFont
                        font.pixelSize: 18
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 5
                        color: parent.checked
                               ? Qt.rgba(figureEditor.accentColor.r,
                                         figureEditor.accentColor.g,
                                         figureEditor.accentColor.b, 0.28)
                               : "transparent"
                        border.width: parent.checked ? 1 : 0
                        border.color: figureEditor.accentColor
                    }
                }
            }

            Item { Layout.fillWidth: true }
            ToolButton {
                text: "↶"
                onClicked: figureEditor.undo()
                ToolTip.text: "Undo · Ctrl+Z"
                ToolTip.visible: hovered
            }
            ToolButton {
                text: "Clear"
                onClicked: {
                    figureEditor.actions = []
                    drawing.requestPaint()
                }
            }
            Button { text: "Cancel"; onClicked: figureEditor.close() }
            Button { text: "Save"; onClicked: figureEditor.saveFigure() }
        }

        Item {
            id: drawingArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            Item {
                anchors.fill: parent

                Rectangle {
                    anchors.fill: parent
                    color: figureEditor.backgroundColor
                    border.width: 1
                    border.color: Qt.rgba(figureEditor.foregroundColor.r,
                                          figureEditor.foregroundColor.g,
                                          figureEditor.foregroundColor.b, 0.16)
                }

                Canvas {
                    id: drawing
                    objectName: "figureCanvas"
                    anchors.fill: parent
                    antialiasing: true

                    function drawPath(ctx, action) {
                        var erase = action.type === "eraser"
                        ctx.globalCompositeOperation = "source-over"
                        ctx.strokeStyle = erase ? figureEditor.backgroundColor
                                                : figureEditor.foregroundColor
                        ctx.lineWidth = erase ? 22 : 3
                        ctx.lineCap = "round"
                        ctx.lineJoin = "round"
                        if (!action.points || !action.points.length) return
                        ctx.beginPath()
                        ctx.moveTo(action.points[0].x, action.points[0].y)
                        for (var i = 1; i < action.points.length; ++i)
                            ctx.lineTo(action.points[i].x, action.points[i].y)
                        ctx.stroke()
                    }

                    function drawShape(ctx, action) {
                        ctx.globalCompositeOperation = "source-over"
                        ctx.strokeStyle = figureEditor.foregroundColor
                        ctx.fillStyle = figureEditor.foregroundColor
                        ctx.lineWidth = 3
                        ctx.lineCap = "round"
                        if (action.type === "text") {
                            ctx.font = action.size + "px " + figureEditor.writingFont
                            ctx.fillText(action.text, action.x, action.y)
                            return
                        }
                        ctx.beginPath()
                        if (action.type === "box") {
                            ctx.rect(action.x1, action.y1,
                                     action.x2 - action.x1, action.y2 - action.y1)
                        } else {
                            ctx.moveTo(action.x1, action.y1)
                            ctx.lineTo(action.x2, action.y2)
                        }
                        ctx.stroke()
                        if (action.type === "arrow") {
                            var angle = Math.atan2(action.y2 - action.y1,
                                                   action.x2 - action.x1)
                            var size = 13
                            ctx.beginPath()
                            ctx.moveTo(action.x2, action.y2)
                            ctx.lineTo(action.x2 - size * Math.cos(angle - Math.PI / 6),
                                       action.y2 - size * Math.sin(angle - Math.PI / 6))
                            ctx.moveTo(action.x2, action.y2)
                            ctx.lineTo(action.x2 - size * Math.cos(angle + Math.PI / 6),
                                       action.y2 - size * Math.sin(angle + Math.PI / 6))
                            ctx.stroke()
                        }
                    }

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()
                        ctx.clearRect(0, 0, width, height)
                        ctx.fillStyle = figureEditor.backgroundColor
                        ctx.fillRect(0, 0, width, height)
                        var all = figureEditor.actions.slice()
                        if (figureEditor.currentAction) all.push(figureEditor.currentAction)
                        for (var i = 0; i < all.length; ++i) {
                            var action = all[i]
                            if (action.type === "pen" || action.type === "eraser")
                                drawPath(ctx, action)
                            else
                                drawShape(ctx, action)
                        }
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                onPressed: function(mouse) { figureEditor.beginAction(mouse.x, mouse.y) }
                onPositionChanged: function(mouse) {
                    if (pressed) figureEditor.updateAction(mouse.x, mouse.y)
                }
                onReleased: function(mouse) { figureEditor.finishAction(mouse.x, mouse.y) }
            }

            TextField {
                id: textEditor
                visible: false
                width: Math.min(260, drawingArea.width)
                color: figureEditor.foregroundColor
                selectionColor: figureEditor.accentColor
                font.family: figureEditor.writingFont
                font.pixelSize: 20
                placeholderText: "Text"
                Keys.onReturnPressed: function(event) {
                    figureEditor.commitText()
                    event.accepted = true
                }
                Keys.onEscapePressed: function(event) {
                    visible = false
                    text = ""
                    event.accepted = true
                }
            }
        }
    }
}
