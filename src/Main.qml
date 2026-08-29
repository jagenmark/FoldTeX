import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs as Dialogs
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: win
    width: 980
    height: 760
    minimumWidth: 560
    minimumHeight: 420
    visible: true
    title: (modified ? "* " : "") + documentTitle + " — FoldTeX"
    color: backend.themeBackground

    Material.theme: luminance(backend.themeBackground) < 0.5 ? Material.Dark : Material.Light
    Material.accent: backend.themeAccent

    property int activeIndex: 0
    property string documentPath: ""
    property bool modified: false
    property bool loading: false
    property string documentTitle: "Untitled notes"
    property string courseName: ""
    property string lectureName: ""
    property string lectureDate: ""
    property string sourcePdf: ""
    property string saveStatus: "Saved"
    property int displayMode: 0
    property int selectionAnchor: -1
    property int selectionEnd: -1
    property int searchLine: -1
    property int searchPosition: -1
    property var renderQueue: []
    property bool renderJobActive: false
    property int renderRequestId: 0
    property int pendingRenderRequestId: -1
    property int pendingRenderIndex: -1
    property string pendingRenderSource: ""
    property string pendingRenderColor: ""
    property int pendingRenderSize: 0
    property int queuedVisibleCount: 0
    property var undoHistory: []
    property var redoHistory: []
    property string lastHistoryState: ""
    property string savedHistoryState: ""
    property bool restoringHistory: false
    property var snippetStops: []
    property int snippetStopIndex: -1
    property int snippetStopRow: -1
    readonly property int pendingRenderCount: renderQueue.length + (renderJobActive ? 1 : 0)
    readonly property bool hasLineSelection: selectionAnchor >= 0 && selectionEnd >= 0
    readonly property int firstSelectedLine: Math.min(selectionAnchor, selectionEnd)
    readonly property int lastSelectedLine: Math.max(selectionAnchor, selectionEnd)
    readonly property real marginScale: Math.max(0, Math.min(1, (width - minimumWidth) / (980 - minimumWidth)))
    readonly property int pageMargin: 16 + (backend.editorSideMargin - 16) * marginScale
    readonly property int pageWidth: width - 2 * pageMargin
    readonly property color textColor: backend.themeForeground
    readonly property color mutedColor: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.48)
    readonly property string editorFont: backend.editorFontFamily
    readonly property int editorSize: backend.editorFontSize
    readonly property var latexCommands: [
        { name: "Square root", insertText: "\\sqrt{|}", example: "\\sqrt{x}", keywords: "root square radical sqrt" },
        { name: "Nth root", insertText: "\\sqrt[n]{|}", example: "\\sqrt[n]{x}", keywords: "root nth index radical" },
        { name: "Fraction", insertText: "\\frac{|}{}", example: "\\frac{a}{b}", keywords: "fraction divide division over ratio" },
        { name: "Power", insertText: "^{|}", example: "x^{2}", keywords: "power exponent superscript squared cubed" },
        { name: "Subscript", insertText: "_{|}", example: "x_{1}", keywords: "subscript index below" },
        { name: "Sum", insertText: "\\sum_{i=1}^{n} |", example: "\\sum_{i=1}^{n} x_i", keywords: "sum sigma total series" },
        { name: "Product", insertText: "\\prod_{i=1}^{n} |", example: "\\prod_{i=1}^{n} x_i", keywords: "product multiply pi series" },
        { name: "Integral", insertText: "\\int_{a}^{b} |\\,dx", example: "\\int_{a}^{b} f(x)\\,dx", keywords: "integral integrate area" },
        { name: "Limit", insertText: "\\lim_{x \\to 0} |", example: "\\lim_{x \\to 0} f(x)", keywords: "limit approaches tends to" },
        { name: "Derivative", insertText: "\\frac{d|}{dx}", example: "\\frac{dy}{dx}", keywords: "derivative differentiate dy dx" },
        { name: "Partial derivative", insertText: "\\frac{\\partial |}{\\partial x}", example: "\\frac{\\partial f}{\\partial x}", keywords: "partial derivative differentiate" },
        { name: "Infinity", insertText: "\\infty", example: "\\infty", keywords: "infinity infinite forever" },
        { name: "Plus or minus", insertText: "\\pm", example: "x \\pm y", keywords: "plus minus both" },
        { name: "Not equal", insertText: "\\neq", example: "x \\neq y", keywords: "not equal inequality" },
        { name: "Approximately equal", insertText: "\\approx", example: "x \\approx y", keywords: "approximately about close equal" },
        { name: "Less than or equal", insertText: "\\leq", example: "x \\leq y", keywords: "less smaller equal inequality" },
        { name: "Greater than or equal", insertText: "\\geq", example: "x \\geq y", keywords: "greater bigger equal inequality" },
        { name: "Arrow", insertText: "\\to", example: "x \\to y", keywords: "arrow to approaches tends" },
        { name: "Vector", insertText: "\\vec{|}", example: "\\vec{v}", keywords: "vector arrow" },
        { name: "Absolute value", insertText: "\\left| | \\right|", example: "\\left|x\\right|", keywords: "absolute value magnitude bars modulus" },
        { name: "Parentheses", insertText: "\\left( | \\right)", example: "\\left( x \\right)", keywords: "parentheses brackets group scalable" },
        { name: "Matrix", insertText: "\\begin{bmatrix} | & b \\\\ c & d \\end{bmatrix}", example: "\\begin{bmatrix} a & b \\\\ c & d \\end{bmatrix}", keywords: "matrix grid array brackets" },
        { name: "Cases", insertText: "\\begin{cases} |, & x > 0 \\\\ 0, & x \\leq 0 \\end{cases}", example: "\\begin{cases} x, & x > 0 \\\\ 0, & x \\leq 0 \\end{cases}", keywords: "cases piecewise conditions" },
        { name: "Greek alpha", insertText: "\\alpha", example: "\\alpha", keywords: "greek alpha letter" },
        { name: "Greek beta", insertText: "\\beta", example: "\\beta", keywords: "greek beta letter" },
        { name: "Greek theta", insertText: "\\theta", example: "\\theta", keywords: "greek theta angle letter" },
        { name: "Greek pi", insertText: "\\pi", example: "\\pi", keywords: "greek pi circle letter" },
        { name: "Greek sigma", insertText: "\\sigma", example: "\\sigma", keywords: "greek sigma standard deviation letter" }
    ]

    function luminance(color) {
        return 0.299 * color.r + 0.587 * color.g + 0.114 * color.b
    }

    function looksLikeMath(source) {
        return /\\[A-Za-z]+|[_^=]/.test(source)
    }

    function makeLine(source, kind, label, asset, slide) {
        return {
            source: source || "",
            kind: kind || "normal",
            label: label || "",
            asset: asset || "",
            slide: slide === undefined ? -1 : slide,
            renderedUrl: "",
            error: "",
            math: false
        }
    }

    function serializedLines() {
        var result = []
        for (var i = 0; i < lines.count; ++i) {
            var line = lines.get(i)
            result.push({
                source: line.source || "",
                kind: line.kind || "normal",
                label: line.label || "",
                asset: line.asset || "",
                slide: line.slide === undefined ? -1 : line.slide
            })
        }
        return result
    }

    function documentData() {
        return {
            format: "foldtex-2",
            title: documentTitle,
            course: courseName,
            lecture: lectureName,
            lectureDate: lectureDate,
            sourcePdf: sourcePdf,
            lines: serializedLines()
        }
    }

    function currentStateJson() {
        return JSON.stringify(documentData())
    }

    function resetHistory(markSaved) {
        historyTimer.stop()
        var state = currentStateJson()
        undoHistory = [{ state: state, active: activeIndex }]
        redoHistory = []
        lastHistoryState = state
        if (markSaved) savedHistoryState = state
    }

    function recordHistory() {
        historyTimer.stop()
        if (loading || restoringHistory) return
        var state = currentStateJson()
        if (state === lastHistoryState) return
        var next = undoHistory.slice()
        next.push({ state: state, active: activeIndex })
        if (next.length > 150) next.shift()
        undoHistory = next
        redoHistory = []
        lastHistoryState = state
    }

    function restoreHistoryEntry(entry) {
        if (!entry || !entry.state) return
        restoringHistory = true
        var data = JSON.parse(entry.state)
        applyDocumentData(data, entry.active)
        lastHistoryState = entry.state
        modified = entry.state !== savedHistoryState
        saveStatus = modified ? "Saving…" : "Saved"
        restoringHistory = false
        recoveryTimer.restart()
    }

    function undoDocument() {
        recordHistory()
        if (undoHistory.length < 2) return
        var back = undoHistory.slice()
        var current = back.pop()
        var forward = redoHistory.slice()
        forward.push(current)
        undoHistory = back
        redoHistory = forward
        restoreHistoryEntry(back[back.length - 1])
    }

    function redoDocument() {
        if (!redoHistory.length) return
        var forward = redoHistory.slice()
        var entry = forward.pop()
        var back = undoHistory.slice()
        back.push(entry)
        redoHistory = forward
        undoHistory = back
        restoreHistoryEntry(entry)
    }

    function clearSnippetStops() {
        snippetStops = []
        snippetStopIndex = -1
        snippetStopRow = -1
    }

    function insertSnippet(editor, rowIndex, from, to, template) {
        if (rowIndex >= 0 && rowIndex < lines.count
                && lines.get(rowIndex).source !== editor.text)
            lines.setProperty(rowIndex, "source", editor.text)
        recordHistory()
        var clean = ""
        var stops = []
        var at = 0
        while (at < template.length) {
            var open = template.indexOf("«", at)
            if (open < 0) {
                clean += template.slice(at)
                break
            }
            clean += template.slice(at, open)
            var close = template.indexOf("»", open + 1)
            if (close < 0) {
                clean += template.slice(open)
                break
            }
            var value = template.slice(open + 1, close)
            var start = clean.length
            clean += value
            stops.push({ start: from + start, end: from + clean.length })
            at = close + 1
        }
        editor.remove(from, to)
        editor.insert(from, clean)
        snippetStops = stops
        snippetStopIndex = -1
        snippetStopRow = rowIndex
        advanceSnippet(editor, rowIndex)
        changed()
    }

    function advanceSnippet(editor, rowIndex) {
        if (snippetStopRow !== rowIndex || !snippetStops.length) return false
        snippetStopIndex++
        if (snippetStopIndex >= snippetStops.length) {
            clearSnippetStops()
            return true
        }
        var stop = snippetStops[snippetStopIndex]
        editor.select(stop.start, stop.end)
        return true
    }

    function handleSnippetTab(editor, rowIndex) {
        if (advanceSnippet(editor, rowIndex)) return true
        var cursor = editor.cursorPosition
        var before = editor.text.slice(0, cursor)
        var postfix = before.match(/([^\s=+\-*\/]+)\.(sqrt|sq|cb|hat|vec|bar|inv)$/)
        if (postfix) {
            var expression = postfix[1]
            var operation = postfix[2]
            var replacement = operation === "sqrt" ? "\\sqrt{" + expression + "}"
                    : operation === "sq" ? expression + "^{2}"
                    : operation === "cb" ? expression + "^{3}"
                    : operation === "hat" ? "\\hat{" + expression + "}"
                    : operation === "vec" ? "\\vec{" + expression + "}"
                    : operation === "bar" ? "\\overline{" + expression + "}"
                    : "\\frac{1}{" + expression + "}"
            insertSnippet(editor, rowIndex, cursor - postfix[0].length, cursor,
                          replacement + "«»")
            return true
        }

        var trigger = before.match(/([A-Za-z][A-Za-z0-9]*)$/)
        if (!trigger) {
            editor.insert(cursor, "    ")
            return true
        }
        var snippets = {
            sqrt: "\\sqrt{«x»}",
            root: "\\sqrt[«n»]{«x»}",
            frac: "\\frac{«a»}{«b»}",
            sum: "\\sum_{«i=1»}^{«n»} «x_i»",
            prod: "\\prod_{«i=1»}^{«n»} «x_i»",
            int: "\\int_{«a»}^{«b»} «f(x)»\\,d«x»",
            lim: "\\lim_{«x \\to 0»} «f(x)»",
            cases: "\\begin{cases} «x», & «x > 0» \\\\ «0», & «x \\leq 0» \\end{cases}",
            mat2: "\\begin{bmatrix} «a» & «b» \\\\ «c» & «d» \\end{bmatrix}"
        }
        var template = snippets[trigger[1].toLowerCase()]
        if (!template) {
            editor.insert(cursor, "    ")
            return true
        }
        insertSnippet(editor, rowIndex, cursor - trigger[1].length, cursor, template)
        return true
    }

    function changed() {
        if (loading) return
        modified = true
        saveStatus = "Saving…"
        recoveryTimer.restart()
        historyTimer.restart()
    }

    function saveRecoveryNow() {
        backend.saveRecoveryData(documentData())
        saveStatus = "Saved"
    }

    function quitApp() {
        saveRecoveryNow()
        Qt.quit()
    }

    function documentHasText() {
        for (var i = 0; i < lines.count; ++i)
            if (lines.get(i).source.trim().length) return true
        return false
    }

    function startNewDocument() {
        recoveryTimer.stop()
        cancelQueuedRenders()
        loading = true
        lines.clear()
        lines.append(makeLine(""))
        activeIndex = 0
        documentPath = ""
        documentTitle = "Untitled notes"
        courseName = ""
        lectureName = ""
        lectureDate = ""
        sourcePdf = ""
        displayMode = 0
        modified = false
        loading = false
        resetHistory(true)
        saveRecoveryNow()
        Qt.callLater(function() { win.editLine(0) })
    }

    function requestNewDocument() {
        if (documentHasText() && (modified || !documentPath.length))
            newDocumentDialog.open()
        else
            startNewDocument()
    }

    onClosing: saveRecoveryNow()

    Timer {
        id: recoveryTimer
        interval: 300
        repeat: false
        onTriggered: win.saveRecoveryNow()
    }

    Timer {
        id: historyTimer
        interval: 450
        repeat: false
        onTriggered: win.recordHistory()
    }

    Timer {
        interval: 300000
        repeat: true
        running: win.documentHasText()
        onTriggered: backend.saveSnapshot(win.documentData())
    }

    Timer {
        id: documentScrollLinger
        interval: 700
    }

    Timer {
        id: renderQueueTimer
        interval: 0
        repeat: false
        onTriggered: win.renderNextQueuedLine()
    }

    function renderLine(index) {
        if (index < 0 || index >= lines.count) return
        renderQueueTimer.stop()
        var queued = []
        for (var i = 0; i < renderQueue.length; ++i)
            if (renderQueue[i] !== index) queued.push(renderQueue[i])
        queued.unshift(index)
        renderQueue = queued
        if (!renderJobActive) renderQueueTimer.start()
    }

    function lineIsVisible(index) {
        var item = list.itemAtIndex(index)
        if (!item) return false
        return item.y + item.height >= list.contentY
                && item.y <= list.contentY + list.height
    }

    function renderQueueHasVisiblePrefix() {
        var passedVisibleRows = false
        for (var i = 0; i < renderQueue.length; ++i) {
            if (lineIsVisible(renderQueue[i])) {
                if (passedVisibleRows) return false
            } else {
                passedVisibleRows = true
            }
        }
        return true
    }

    function queueRenders(includeActiveLine) {
        renderQueueTimer.stop()
        var visible = []
        var later = []
        for (var i = 0; i < lines.count; ++i) {
            if (!includeActiveLine && i === activeIndex) continue
            if (lineIsVisible(i)) visible.push(i)
            else later.push(i)
        }
        queuedVisibleCount = visible.length
        renderQueue = visible.concat(later)
        if (renderQueue.length && !renderJobActive) renderQueueTimer.start()
    }

    function renderNextQueuedLine() {
        if (renderJobActive) return
        while (renderQueue.length) {
            var queued = renderQueue.slice()
            var index = queued.shift()
            renderQueue = queued
            if (index < 0 || index >= lines.count) continue
            var source = lines.get(index).source.trim()
            if (!source.length) {
                lines.setProperty(index, "renderedUrl", "")
                lines.setProperty(index, "error", "")
                lines.setProperty(index, "math", false)
                continue
            }
            var math = looksLikeMath(source)
            lines.setProperty(index, "math", math)
            if (!math) {
                lines.setProperty(index, "renderedUrl", "")
                lines.setProperty(index, "error", "")
                continue
            }
            renderJobActive = true
            pendingRenderRequestId = ++renderRequestId
            pendingRenderIndex = index
            pendingRenderSource = source
            pendingRenderColor = backend.themeForeground
            pendingRenderSize = win.editorSize
            backend.renderAsync(pendingRenderRequestId, source,
                                pendingRenderColor, pendingRenderSize)
            return
        }
    }

    function finishQueuedRender(requestId, result) {
        if (requestId !== pendingRenderRequestId) return
        if (pendingRenderIndex >= 0 && pendingRenderIndex < lines.count
                && lines.get(pendingRenderIndex).source.trim() === pendingRenderSource
                && backend.themeForeground === pendingRenderColor
                && win.editorSize === pendingRenderSize) {
            lines.setProperty(pendingRenderIndex, "renderedUrl", result.url || "")
            lines.setProperty(pendingRenderIndex, "error", result.error
                              ? backend.latexHint(pendingRenderSource, result.error) : "")
        }
        renderJobActive = false
        pendingRenderIndex = -1
        pendingRenderSource = ""
        if (renderQueue.length) renderQueueTimer.start()
    }

    function cancelQueuedRenders() {
        renderQueueTimer.stop()
        renderQueue = []
    }

    function renderAll() {
        queueRenders(false)
    }

    function renderEveryLine() {
        queueRenders(true)
    }

    function clearLineSelection() {
        selectionAnchor = -1
        selectionEnd = -1
    }

    function lineIsSelected(index) {
        return hasLineSelection && index >= firstSelectedLine && index <= lastSelectedLine
    }

    function extendLineSelection(index) {
        if (selectionAnchor < 0) selectionAnchor = activeIndex
        selectionEnd = Math.max(0, Math.min(lines.count - 1, index))
        list.positionViewAtIndex(selectionEnd, ListView.Contain)
    }

    function extendLineSelectionBy(amount) {
        var from = selectionEnd >= 0 ? selectionEnd : activeIndex
        extendLineSelection(from + amount)
    }

    function scrollDocumentBy(amount) {
        var minimum = list.originY
        var maximum = Math.max(minimum, minimum + list.contentHeight - list.height)
        list.contentY = Math.max(minimum, Math.min(maximum, list.contentY + amount))
    }

    function wheelDistance(pixelDelta, angleDelta) {
        if (pixelDelta !== 0) return pixelDelta * 40
        return Math.abs(angleDelta) < 120 ? angleDelta * 40 : angleDelta * 2
    }

    function selectedLinesText() {
        var selected = []
        for (var i = firstSelectedLine; i <= lastSelectedLine; ++i)
            selected.push(lines.get(i).source)
        return selected.join("\n")
    }

    function copySelectedLines() {
        if (hasLineSelection) backend.setClipboardText(selectedLinesText())
    }

    function deleteSelectedLines() {
        if (!hasLineSelection) return
        recordHistory()
        var nextIndex = firstSelectedLine
        lines.remove(firstSelectedLine, lastSelectedLine - firstSelectedLine + 1)
        if (lines.count === 0 || lines.get(lines.count - 1).source.length !== 0)
            lines.append(makeLine(""))
        activeIndex = Math.min(nextIndex, lines.count - 1)
        clearLineSelection()
        changed()
        Qt.callLater(function() { editLine(activeIndex) })
    }

    function cutSelectedLines() {
        if (!hasLineSelection) return
        copySelectedLines()
        deleteSelectedLines()
    }

    function pasteMultiline(editor, index, clipboard) {
        var normalized = clipboard.replace(/\r\n/g, "\n").replace(/\r/g, "\n")
        var parts = normalized.split("\n")
        if (parts.length < 2) return false
        recordHistory()
        var start = editor.selectionStart
        var end = editor.selectionEnd
        var before = editor.text.slice(0, start)
        var after = editor.text.slice(end)
        var current = lines.get(index)
        lines.set(index, makeLine(before + parts[0], current.kind, current.label,
                                  current.asset, current.slide))
        for (var i = 1; i < parts.length; ++i) {
            var source = parts[i] + (i === parts.length - 1 ? after : "")
            lines.insert(index + i, makeLine(source))
        }
        activeIndex = index + parts.length - 1
        changed()
        Qt.callLater(function() {
            list.positionViewAtIndex(activeIndex, ListView.Contain)
            var item = list.itemAtIndex(activeIndex)
            if (item && item.currentContent) {
                item.currentContent.cursorPosition = parts[parts.length - 1].length
                item.currentContent.forceActiveFocus()
            }
        })
        return true
    }

    function joinWithPrevious(editor, index) {
        if (index <= 0 || editor.cursorPosition !== 0 || editor.selectedText.length) return false
        recordHistory()
        var priorLength = lines.get(index - 1).source.length
        var joined = lines.get(index - 1).source + editor.text
        var prior = lines.get(index - 1)
        lines.set(index - 1, makeLine(joined, prior.kind, prior.label,
                                      prior.asset, prior.slide))
        lines.remove(index)
        activeIndex = index - 1
        changed()
        Qt.callLater(function() {
            list.positionViewAtIndex(activeIndex, ListView.Contain)
            var item = list.itemAtIndex(activeIndex)
            if (item && item.currentContent) item.currentContent.cursorPosition = priorLength
        })
        return true
    }

    function joinWithNext(editor, index) {
        if (index >= lines.count - 1 || editor.cursorPosition !== editor.text.length
                || editor.selectedText.length) return false
        recordHistory()
        var cursor = editor.cursorPosition
        var joined = editor.text + lines.get(index + 1).source
        var current = lines.get(index)
        lines.set(index, makeLine(joined, current.kind, current.label,
                                  current.asset, current.slide))
        lines.remove(index + 1)
        changed()
        Qt.callLater(function() { editor.cursorPosition = cursor })
        return true
    }

    function editLine(index) {
        if (index < 0 || index >= lines.count) return
        clearLineSelection()
        if (displayMode !== 0) displayMode = 0
        if (index !== snippetStopRow) clearSnippetStops()
        if (activeIndex !== index) renderLine(activeIndex)
        activeIndex = index
        Qt.callLater(function() { list.positionViewAtIndex(index, ListView.Contain) })
    }

    function activeEditorItem() {
        var rowItem = list.itemAtIndex(activeIndex)
        return rowItem ? rowItem.currentContent : null
    }

    function commitLine(index) {
        renderLine(index)
        if (index === lines.count - 1) {
            recordHistory()
            lines.append(makeLine(""))
        }
        activeIndex = Math.min(index + 1, lines.count - 1)
        changed()
        Qt.callLater(function() { list.positionViewAtIndex(activeIndex, ListView.Contain) })
    }

    function saveTo(path) {
        if (!path) return
        recordHistory()
        if (backend.saveDocumentData(path, documentData())) {
            documentPath = path
            modified = false
            saveStatus = "Saved"
            savedHistoryState = currentStateJson()
            backend.saveSnapshot(documentData())
        }
    }

    function cycleDisplayMode() {
        cancelQueuedRenders()
        displayMode = (displayMode + 1) % 3
        clearLineSelection()
        if (displayMode === 2) renderEveryLine()
    }

    function displayModeName() {
        return displayMode === 1 ? "Source" : displayMode === 2 ? "Rendered" : "Auto"
    }

    function openSearch() {
        searchPopup.open()
    }

    function setActiveRowKind(kind) {
        if (activeIndex < 0 || activeIndex >= lines.count) return
        recordHistory()
        lines.setProperty(activeIndex, "kind", kind)
        lines.setProperty(activeIndex, "label",
                          kind === "normal" ? "" : kind.charAt(0).toUpperCase() + kind.slice(1))
        changed()
        rowTypePopup.close()
    }

    function addCatchupMarker() {
        recordHistory()
        var now = new Date()
        var clock = Qt.formatTime(now, "HH:mm")
        var target = Math.max(0, Math.min(activeIndex + 1, lines.count))
        lines.insert(target, makeLine("What I missed:", "catchup", "Catch up · " + clock))
        activeIndex = target
        changed()
        Qt.callLater(function() { editLine(target) })
    }

    function addImportedImage(result) {
        if (result.error) {
            messageDialog.message = result.error
            messageDialog.open()
            return
        }
        recordHistory()
        var target = Math.max(0, Math.min(activeIndex + 1, lines.count))
        lines.insert(target, makeLine("", "image", "Figure", result.path, -1))
        activeIndex = target
        changed()
        Qt.callLater(function() { list.positionViewAtIndex(target, ListView.Contain) })
    }

    function pasteClipboardImage() {
        addImportedImage(backend.importClipboardImage(documentPath))
    }

    function runCourseSearch() {
        courseResults.clear()
        var found = backend.searchCourse(documentPath, courseSearchInput.text)
        for (var i = 0; i < found.length; ++i)
            courseResults.append(found[i])
        courseResultList.currentIndex = courseResults.count ? 0 : -1
    }

    function openCourseResult(item) {
        if (!item) return
        if (documentPath && modified) saveTo(documentPath)
        var data = backend.loadDocument(item.path)
        if (data.error) return
        loadData(data, item.path)
        activeIndex = Math.max(0, Math.min(lines.count - 1, item.line))
        courseSearchPopup.close()
        Qt.callLater(function() { editLine(activeIndex) })
    }

    function searchHighlightLevel(index) {
        if (!searchPopup.visible || index < 0 || index >= lines.count) return 0
        var query = searchInput.text.trim().toLowerCase()
        if (!query.length || lines.get(index).source.toLowerCase().indexOf(query) < 0) return 0
        return index === searchLine ? 2 : 1
    }

    function searchMatchCount() {
        var query = searchInput.text.trim().toLowerCase()
        if (!query.length) return 0
        var count = 0
        for (var i = 0; i < lines.count; ++i)
            if (lines.get(i).source.toLowerCase().indexOf(query) >= 0) count++
        return count
    }

    function runLiveSearch() {
        searchLine = -1
        searchPosition = -1
        if (searchInput.text.trim().length) findNext(false)
    }

    function showSearchMatch(lineIndex, position, length) {
        searchLine = lineIndex
        searchPosition = position
        Qt.callLater(function() {
            list.positionViewAtIndex(lineIndex, ListView.Contain)
            searchInput.forceActiveFocus()
        })
    }

    function findNext(backward) {
        var query = searchInput.text.toLowerCase()
        if (!query.length || !lines.count) return
        var startLine = searchLine >= 0 ? searchLine : activeIndex
        for (var pass = 0; pass < lines.count; ++pass) {
            var lineIndex = backward
                    ? (startLine - pass + lines.count) % lines.count
                    : (startLine + pass) % lines.count
            var source = lines.get(lineIndex).source
            var lower = source.toLowerCase()
            var position
            if (pass === 0 && lineIndex === searchLine) {
                position = backward
                        ? lower.lastIndexOf(query, Math.max(0, searchPosition - 1))
                        : lower.indexOf(query, searchPosition + 1)
            } else {
                position = backward ? lower.lastIndexOf(query) : lower.indexOf(query)
            }
            if (position >= 0) {
                showSearchMatch(lineIndex, position, query.length)
                return
            }
        }
    }

    function replaceCurrentMatch() {
        if (searchLine < 0 || searchPosition < 0 || !searchInput.text.length) return
        var source = lines.get(searchLine).source
        if (source.slice(searchPosition, searchPosition + searchInput.text.length).toLowerCase()
                !== searchInput.text.toLowerCase()) return
        recordHistory()
        var replacement = replaceInput.text
        lines.setProperty(searchLine, "source", source.slice(0, searchPosition)
                          + replacement + source.slice(searchPosition + searchInput.text.length))
        changed()
        searchPosition += replacement.length - 1
        findNext(false)
    }

    function replaceAllMatches() {
        var query = searchInput.text
        if (!query.length) return
        var lowerQuery = query.toLowerCase()
        recordHistory()
        var count = 0
        for (var i = 0; i < lines.count; ++i) {
            var source = lines.get(i).source
            var lower = source.toLowerCase()
            var result = ""
            var from = 0
            var at = lower.indexOf(lowerQuery, from)
            while (at >= 0) {
                result += source.slice(from, at) + replaceInput.text
                from = at + query.length
                count++
                at = lower.indexOf(lowerQuery, from)
            }
            if (from > 0) lines.setProperty(i, "source", result + source.slice(from))
        }
        if (count) changed()
        saveStatus = count ? "Replaced " + count : "No matches"
        searchLine = -1
        searchPosition = -1
    }

    function applyDocumentData(data, preferredActive) {
        loading = true
        lines.clear()
        documentTitle = data.title || "Untitled notes"
        courseName = data.course || ""
        lectureName = data.lecture || ""
        lectureDate = data.lectureDate || ""
        sourcePdf = data.sourcePdf || ""
        var loaded = data.lines || []
        for (var i = 0; i < loaded.length; ++i) {
            var item = loaded[i]
            lines.append(makeLine(item.source || "", item.kind || "normal",
                                  item.label || "", item.asset || "",
                                  item.slide === undefined ? -1 : item.slide))
        }
        if (lines.count === 0 || lines.get(lines.count - 1).source.length !== 0)
            lines.append(makeLine(""))
        activeIndex = preferredActive === undefined
                ? Math.max(0, lines.count - 1)
                : Math.max(0, Math.min(lines.count - 1, preferredActive))
        loading = false
        cancelQueuedRenders()
        Qt.callLater(renderAll)
    }

    function loadData(data, path) {
        if (data.error) return
        applyDocumentData(data)
        documentPath = path || ""
        modified = false
        resetHistory(true)
    }

    function updateCommandResults(query) {
        commandResults.clear()
        var q = query.trim().toLowerCase()
        var matches = []
        for (var i = 0; i < latexCommands.length; ++i) {
            var command = latexCommands[i]
            var name = command.name.toLowerCase()
            var haystack = name + " " + command.keywords + " " + command.example.toLowerCase()
            var score = 1
            if (q.length) {
                var terms = q.split(/\s+/)
                var found = true
                score = 0
                for (var j = 0; j < terms.length; ++j) {
                    var at = haystack.indexOf(terms[j])
                    if (at < 0) {
                        found = false
                        break
                    }
                    score += 20
                }
                if (!found) continue
                if (name === q) score += 100
                else if (name.indexOf(q) === 0) score += 60
                else if (name.indexOf(q) >= 0) score += 30
            }
            matches.push({ score: score, order: i, command: command })
        }
        matches.sort(function(a, b) {
            if (a.score !== b.score) return b.score - a.score
            return a.order - b.order
        })
        for (var k = 0; k < matches.length; ++k) {
            var item = matches[k].command
            commandResults.append({
                commandName: item.name,
                insertText: item.insertText,
                example: item.example
            })
        }
        commandList.currentIndex = commandResults.count ? 0 : -1
    }

    function openCommandFinder() {
        commandPopup.open()
    }

    function openGuide() {
        guideWindow.show()
        guideWindow.raise()
        guideWindow.requestActivate()
    }

    function insertCommand(text) {
        if (activeIndex < 0 || activeIndex >= lines.count) return
        var marker = text.indexOf("|")
        var clean = marker >= 0 ? text.slice(0, marker) + text.slice(marker + 1) : text
        var rowItem = list.itemAtIndex(activeIndex)
        var editor = rowItem ? rowItem.currentContent : null
        if (editor && editor.insert) {
            var start = editor.cursorPosition
            editor.insert(start, clean)
            editor.cursorPosition = start + (marker >= 0 ? marker : clean.length)
            editor.forceActiveFocus()
        } else {
            var source = lines.get(activeIndex).source
            lines.setProperty(activeIndex, "source", source + clean)
            changed()
        }
        commandPopup.close()
    }

    Connections {
        target: backend
        function onThemeChanged() { Qt.callLater(win.renderAll) }
        function onEditorFontChanged() { Qt.callLater(win.renderAll) }
        function onRenderFinished(requestId, result) {
            win.finishQueuedRender(requestId, result)
        }
    }

    ListModel { id: lines }
    ListModel { id: commandResults }
    ListModel { id: courseResults }

    Shortcut { sequence: StandardKey.Save; onActivated: documentPath ? saveTo(documentPath) : saveDialog.open() }
    Shortcut { sequence: StandardKey.Undo; context: Qt.ApplicationShortcut; onActivated: undoDocument() }
    Shortcut { sequence: StandardKey.Redo; context: Qt.ApplicationShortcut; onActivated: redoDocument() }
    Shortcut { sequence: StandardKey.SaveAs; onActivated: saveDialog.open() }
    Shortcut { sequence: StandardKey.Open; onActivated: openDialog.open() }
    Shortcut { sequence: "Ctrl+N"; context: Qt.ApplicationShortcut; onActivated: requestNewDocument() }
    Shortcut { sequence: StandardKey.Quit; onActivated: quitApp() }
    Shortcut { sequence: "Ctrl+,"; onActivated: fontDialog.open() }
    Shortcut { sequence: "Ctrl+'"; onActivated: fontDialog.open() }
    Shortcut { sequence: "Ctrl+Shift+F"; onActivated: fontDialog.open() }
    Shortcut { sequence: "Ctrl+K"; context: Qt.ApplicationShortcut; onActivated: openCommandFinder() }
    Shortcut { sequence: "Ctrl+."; context: Qt.ApplicationShortcut; onActivated: rowTypePopup.open() }
    Shortcut { sequence: "Ctrl+M"; context: Qt.ApplicationShortcut; onActivated: addCatchupMarker() }
    Shortcut { sequence: "Ctrl+Alt+L"; context: Qt.ApplicationShortcut; onActivated: lectureDialog.open() }
    Shortcut { sequence: "Ctrl+Alt+F"; context: Qt.ApplicationShortcut; onActivated: courseSearchPopup.open() }
    Shortcut { sequence: "Ctrl+Shift+V"; context: Qt.ApplicationShortcut; onActivated: pasteClipboardImage() }
    Shortcut { sequence: "Ctrl+G"; context: Qt.WindowShortcut; onActivated: openGuide() }
    Shortcut { sequence: StandardKey.Find; context: Qt.ApplicationShortcut; onActivated: openSearch() }
    Shortcut { sequence: "Ctrl+Shift+R"; context: Qt.ApplicationShortcut; onActivated: cycleDisplayMode() }
    Shortcut { sequence: "Ctrl+Shift+E"; context: Qt.ApplicationShortcut; onActivated: exportMenu.open() }
    Shortcut { sequence: "F1"; context: Qt.ApplicationShortcut; onActivated: helpDialog.open() }
    Shortcut { sequence: "Ctrl+Shift+A"; context: Qt.ApplicationShortcut; onActivated: {
        selectionAnchor = 0; selectionEnd = lines.count - 1
    } }
    Shortcut { sequence: StandardKey.Copy; enabled: win.hasLineSelection; onActivated: copySelectedLines() }
    Shortcut { sequence: StandardKey.Cut; enabled: win.hasLineSelection; onActivated: cutSelectedLines() }
    Shortcut { sequence: "Delete"; enabled: win.hasLineSelection; onActivated: deleteSelectedLines() }

    Popup {
        id: searchPopup
        parent: Overlay.overlay
        x: Math.round((win.width - width) / 2)
        y: 12
        width: Math.min(620, win.width - 40)
        modal: false
        focus: true
        padding: 12
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onAboutToShow: {
            searchLine = -1
            searchPosition = -1
        }
        onOpened: {
            Qt.callLater(function() { searchInput.forceActiveFocus(); searchInput.selectAll() })
        }
        onClosed: {
            searchLine = -1
            searchPosition = -1
        }

        background: Rectangle {
            color: backend.themeBackground
            radius: 8
            border.width: 1
            border.color: Qt.rgba(win.textColor.r, win.textColor.g, win.textColor.b, 0.18)
        }

        contentItem: ColumnLayout {
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: searchInput
                    objectName: "searchInput"
                    Layout.fillWidth: true
                    placeholderText: "Find"
                    color: win.textColor
                    selectionColor: backend.themeSelection
                    font.family: win.editorFont
                    onTextEdited: runLiveSearch()
                    Keys.onReturnPressed: function(event) {
                        findNext(event.modifiers & Qt.ShiftModifier)
                        event.accepted = true
                    }
                }
                Button { text: "↑"; onClicked: findNext(true) }
                Button {
                    objectName: "searchNextButton"
                    text: "↓"
                    onClicked: findNext(false)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: replaceInput
                    Layout.fillWidth: true
                    placeholderText: "Replace with"
                    color: win.textColor
                    selectionColor: backend.themeSelection
                    font.family: win.editorFont
                }
                Button { text: "Replace"; onClicked: replaceCurrentMatch() }
                Button { text: "All"; onClicked: replaceAllMatches() }
            }
        }
    }

    Popup {
        id: rowTypePopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(380, win.width - 40)
        height: Math.min(430, win.height - 64)
        modal: true
        focus: true
        padding: 8
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onOpened: rowTypeList.currentIndex = Math.max(0,
            ["normal", "definition", "theorem", "proof", "example"].indexOf(lines.get(activeIndex).kind))
        background: Rectangle {
            color: backend.themeBackground
            radius: 10
            border.width: 1
            border.color: Qt.rgba(win.textColor.r, win.textColor.g, win.textColor.b, 0.18)
        }
        contentItem: ColumnLayout {
            Label {
                text: "Row type"
                color: win.textColor
                font.family: win.editorFont
                font.pixelSize: win.editorSize
                leftPadding: 10
                topPadding: 8
                bottomPadding: 8
            }
            ListView {
                id: rowTypeList
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: ["Normal", "Definition", "Theorem", "Proof", "Example"]
                focus: true
                keyNavigationWraps: true
                delegate: ItemDelegate {
                    required property string modelData
                    required property int index
                    width: rowTypeList.width
                    text: modelData
                    highlighted: ListView.isCurrentItem
                    font.family: win.editorFont
                    onClicked: win.setActiveRowKind(modelData.toLowerCase())
                }
                Keys.onReturnPressed: function(event) {
                    win.setActiveRowKind(model[currentIndex].toLowerCase())
                    event.accepted = true
                }
            }
        }
    }

    Dialog {
        id: lectureDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(520, win.width - 48)
        modal: true
        title: "Lecture details"
        standardButtons: Dialog.Cancel | Dialog.Ok
        Material.accent: fontDialog.dialogText
        onOpened: {
            courseField.text = win.courseName
            lectureField.text = win.lectureName
            lectureDateField.text = win.lectureDate
            Qt.callLater(function() { courseField.forceActiveFocus(); courseField.selectAll() })
        }
        onAccepted: {
            win.recordHistory()
            win.courseName = courseField.text.trim()
            win.lectureName = lectureField.text.trim()
            win.lectureDate = lectureDateField.text.trim()
            win.changed()
        }
        contentItem: ColumnLayout {
            spacing: 8
            Label { text: "Course"; color: fontDialog.dialogText; font.family: win.editorFont }
            TextField {
                id: courseField
                Layout.fillWidth: true
                color: fontDialog.dialogText
                font.family: win.editorFont
                placeholderText: "Calculus I"
            }
            Label { text: "Lecture"; color: fontDialog.dialogText; font.family: win.editorFont }
            TextField {
                id: lectureField
                Layout.fillWidth: true
                color: fontDialog.dialogText
                font.family: win.editorFont
                placeholderText: "Lecture 4 — Limits"
            }
            Label { text: "Date"; color: fontDialog.dialogText; font.family: win.editorFont }
            TextField {
                id: lectureDateField
                Layout.fillWidth: true
                color: fontDialog.dialogText
                font.family: win.editorFont
                placeholderText: "2026-08-29"
            }
        }
    }

    Popup {
        id: courseSearchPopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(680, win.width - 40)
        height: Math.min(560, win.height - 64)
        modal: true
        focus: true
        padding: 1
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onOpened: {
            courseSearchInput.text = ""
            courseResults.clear()
            Qt.callLater(function() { courseSearchInput.forceActiveFocus() })
        }
        background: Rectangle {
            color: backend.themeBackground
            radius: 10
            border.width: 1
            border.color: Qt.rgba(win.textColor.r, win.textColor.g, win.textColor.b, 0.18)
        }
        contentItem: ColumnLayout {
            spacing: 0
            TextField {
                id: courseSearchInput
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                leftPadding: 18
                rightPadding: 18
                placeholderText: documentPath ? "Search this course folder" : "Save this note to search its course folder"
                enabled: documentPath.length > 0
                color: win.textColor
                font.family: win.editorFont
                font.pixelSize: win.editorSize
                onTextEdited: win.runCourseSearch()
                Keys.onDownPressed: function(event) {
                    if (courseResults.count) courseResultList.forceActiveFocus()
                    event.accepted = true
                }
                Keys.onReturnPressed: function(event) {
                    if (courseResultList.currentIndex >= 0)
                        win.openCourseResult(courseResults.get(courseResultList.currentIndex))
                    event.accepted = true
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Qt.rgba(win.textColor.r, win.textColor.g, win.textColor.b, 0.1)
            }
            ListView {
                id: courseResultList
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: courseResults
                clip: true
                focus: true
                keyNavigationWraps: true
                delegate: ItemDelegate {
                    required property int index
                    required property int line
                    required property string title
                    required property string lecture
                    required property string source
                    width: courseResultList.width
                    height: 72
                    highlighted: ListView.isCurrentItem
                    contentItem: Column {
                        spacing: 3
                        Text {
                            width: parent.width
                            text: (lecture.length ? lecture : title) + " · line " + (line + 1)
                            color: win.textColor
                            elide: Text.ElideRight
                            font.family: win.editorFont
                            font.pixelSize: win.editorSize - 2
                        }
                        Text {
                            width: parent.width
                            text: source
                            color: win.mutedColor
                            elide: Text.ElideRight
                            font.family: win.editorFont
                            font.pixelSize: win.editorSize - 3
                        }
                    }
                    onClicked: win.openCourseResult(courseResults.get(index))
                }
                Keys.onReturnPressed: function(event) {
                    if (currentIndex >= 0) win.openCourseResult(courseResults.get(currentIndex))
                    event.accepted = true
                }
                Keys.onUpPressed: function(event) {
                    if (currentIndex <= 0) courseSearchInput.forceActiveFocus()
                    else decrementCurrentIndex()
                    event.accepted = true
                }
            }
        }
    }

    Popup {
        id: commandPopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(560, win.width - 40)
        height: Math.min(500, win.height - 64)
        modal: true
        focus: true
        padding: 1
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onOpened: {
            commandSearch.text = ""
            updateCommandResults("")
            Qt.callLater(function() { commandSearch.forceActiveFocus() })
        }

        background: Rectangle {
            color: backend.themeBackground
            radius: 10
            border.width: 1
            border.color: Qt.rgba(win.textColor.r, win.textColor.g, win.textColor.b, 0.16)
        }

        contentItem: ColumnLayout {
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 54

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 18
                    visible: commandSearch.text.length === 0
                    text: "Search LaTeX — try “root”"
                    color: win.mutedColor
                    verticalAlignment: Text.AlignVCenter
                    font.family: win.editorFont
                    font.pixelSize: win.editorSize
                }

                TextInput {
                    id: commandSearch
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 18
                    color: win.textColor
                    selectionColor: backend.themeSelection
                    selectedTextColor: backend.themeBackground
                    verticalAlignment: TextInput.AlignVCenter
                    clip: true
                    font.family: win.editorFont
                    font.pixelSize: win.editorSize
                    onTextChanged: updateCommandResults(text)

                    Keys.onDownPressed: function(event) {
                        if (commandResults.count) {
                            commandList.currentIndex = Math.min(commandList.currentIndex + 1,
                                                                commandResults.count - 1)
                        }
                        event.accepted = true
                    }
                    Keys.onUpPressed: function(event) {
                        if (commandResults.count)
                            commandList.currentIndex = Math.max(commandList.currentIndex - 1, 0)
                        event.accepted = true
                    }
                    Keys.onReturnPressed: function(event) {
                        if (commandList.currentIndex >= 0) {
                            insertCommand(commandResults.get(commandList.currentIndex).insertText)
                            event.accepted = true
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Qt.rgba(win.textColor.r, win.textColor.g, win.textColor.b, 0.10)
            }

            ListView {
                id: commandList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: commandResults
                boundsBehavior: Flickable.StopAtBounds
                currentIndex: -1
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Item {
                    id: commandRow
                    required property int index
                    required property string commandName
                    required property string insertText
                    required property string example
                    width: commandList.width
                    height: 62

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 4
                        radius: 6
                        color: commandRow.index === commandList.currentIndex
                               ? Qt.rgba(win.textColor.r, win.textColor.g, win.textColor.b, 0.10)
                               : "transparent"
                    }

                    Column {
                        anchors.left: parent.left
                        anchors.leftMargin: 18
                        anchors.right: parent.right
                        anchors.rightMargin: 18
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 3

                        Text {
                            text: commandRow.commandName
                            color: win.textColor
                            font.family: win.editorFont
                            font.pixelSize: Math.max(13, win.editorSize)
                        }
                        Text {
                            text: commandRow.example
                            color: win.mutedColor
                            font.family: win.editorFont
                            font.pixelSize: Math.max(12, win.editorSize - 1)
                            elide: Text.ElideRight
                            width: parent.width
                        }
                    }

                    HoverHandler {
                        onHoveredChanged: {
                            if (hovered) commandList.currentIndex = commandRow.index
                        }
                    }
                    TapHandler { onTapped: insertCommand(commandRow.insertText) }
                }

                Label {
                    anchors.centerIn: parent
                    visible: commandResults.count === 0
                    text: "No matching LaTeX"
                    color: win.mutedColor
                    font.family: win.editorFont
                }
            }
        }
    }

    Dialog {
        id: fontDialog
        objectName: "fontDialog"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(440, win.width - 48)
        modal: true
        title: "Writing font"
        Material.theme: win.luminance(backend.themeBackground) < 0.5 ? Material.Dark : Material.Light
        Material.accent: fontDialog.dialogText
        readonly property color dialogText: "#171717"
        readonly property color dialogMuted: "#666666"
        property int keyboardRow: 0
        property int keyboardButton: 0

        function selectSavedFont() {
            for (var i = 0; i < fontChoice.count; ++i) {
                if (fontChoice.textAt(i) === backend.editorFontFamily) {
                    fontChoice.currentIndex = i
                    break
                }
            }
            sizeChoice.value = backend.editorFontSize
            marginChoice.value = backend.editorSideMargin
        }

        function focusKeyboardTarget() {
            var target = null
            if (keyboardRow < 3) target = fontContent
            else target = keyboardButton === 0 ? cancelButton : okButton
            if (target) target.forceActiveFocus()
        }

        function moveKeyboardFocus(amount) {
            keyboardRow = Math.max(0, Math.min(3, keyboardRow + amount))
            if (keyboardRow === 3) keyboardButton = 0
            focusKeyboardTarget()
        }

        function adjustKeyboardValue(amount) {
            if (keyboardRow === 0) {
                fontChoice.currentIndex = Math.max(0, Math.min(fontChoice.count - 1,
                                                                fontChoice.currentIndex + amount))
            } else if (keyboardRow === 1) {
                sizeChoice.value = Math.max(sizeChoice.from, Math.min(sizeChoice.to,
                                                                      sizeChoice.value + amount))
            } else if (keyboardRow === 2) {
                marginChoice.value = Math.max(marginChoice.from, Math.min(marginChoice.to,
                                                                          marginChoice.value
                                                                          + amount * marginChoice.stepSize))
            } else {
                keyboardButton = amount < 0 ? 0 : 1
                focusKeyboardTarget()
            }
        }

        function activateKeyboardTarget() {
            if (fontChoice.popup.visible) fontChoice.popup.close()
            if (keyboardRow === 3 && keyboardButton === 0) reject()
            else accept()
        }

        onOpened: {
            selectSavedFont()
            keyboardRow = 0
            keyboardButton = 0
            Qt.callLater(focusKeyboardTarget)
        }
        onAccepted: {
            backend.setEditorFont(fontChoice.currentText, sizeChoice.value)
            backend.setEditorSideMargin(marginChoice.value)
        }

        Shortcut {
            sequence: "Up"
            context: Qt.WindowShortcut
            enabled: fontDialog.visible
            onActivated: fontDialog.moveKeyboardFocus(-1)
        }
        Shortcut {
            sequence: "Down"
            context: Qt.WindowShortcut
            enabled: fontDialog.visible
            onActivated: fontDialog.moveKeyboardFocus(1)
        }
        Shortcut {
            sequence: "Left"
            context: Qt.WindowShortcut
            enabled: fontDialog.visible
            onActivated: fontDialog.adjustKeyboardValue(-1)
        }
        Shortcut {
            sequence: "Right"
            context: Qt.WindowShortcut
            enabled: fontDialog.visible
            onActivated: fontDialog.adjustKeyboardValue(1)
        }
        Shortcut {
            sequences: ["Return", "Enter"]
            context: Qt.WindowShortcut
            enabled: fontDialog.visible
            onActivated: fontDialog.activateKeyboardTarget()
        }

        contentItem: ColumnLayout {
            id: fontContent
            spacing: 16
            focus: true

            Label {
                text: "Font"
                color: fontDialog.keyboardRow === 0 ? fontDialog.dialogText : fontDialog.dialogMuted
                font.pixelSize: 12
            }

            Rectangle {
                id: fontFocusFrame
                objectName: "fontFocusFrame"
                property bool keyboardSelected: fontDialog.keyboardRow === 0
                Layout.fillWidth: true
                implicitHeight: fontChoice.implicitHeight + 4
                color: "transparent"
                radius: 5
                border.width: keyboardSelected ? 2 : 1
                border.color: keyboardSelected
                              ? fontDialog.dialogText
                              : Qt.rgba(fontDialog.dialogText.r, fontDialog.dialogText.g,
                                        fontDialog.dialogText.b, 0.22)

                ComboBox {
                    id: fontChoice
                    objectName: "fontChoice"
                    anchors.fill: parent
                    anchors.margins: 2
                    model: backend.availableFonts
                    font.family: currentText
                    onActiveFocusChanged: {
                        if (activeFocus) {
                            fontDialog.keyboardRow = 0
                            Qt.callLater(fontContent.forceActiveFocus)
                        }
                    }
                }
            }

            Label {
                text: "Size"
                color: fontDialog.keyboardRow === 1 ? fontDialog.dialogText : fontDialog.dialogMuted
                font.pixelSize: 12
            }

            Rectangle {
                id: sizeFocusFrame
                objectName: "sizeFocusFrame"
                property bool keyboardSelected: fontDialog.keyboardRow === 1
                Layout.preferredWidth: sizeChoice.implicitWidth + 4
                implicitHeight: sizeChoice.implicitHeight + 4
                color: "transparent"
                radius: 5
                border.width: keyboardSelected ? 2 : 1
                border.color: keyboardSelected
                              ? fontDialog.dialogText
                              : Qt.rgba(fontDialog.dialogText.r, fontDialog.dialogText.g,
                                        fontDialog.dialogText.b, 0.22)

                SpinBox {
                    id: sizeChoice
                    objectName: "sizeChoice"
                    anchors.fill: parent
                    anchors.margins: 2
                    from: 10
                    to: 40
                    editable: false
                    onActiveFocusChanged: {
                        if (activeFocus) {
                            fontDialog.keyboardRow = 1
                            Qt.callLater(fontContent.forceActiveFocus)
                        }
                    }
                }
            }

            Label {
                text: "Side margin"
                color: fontDialog.keyboardRow === 2 ? fontDialog.dialogText : fontDialog.dialogMuted
                font.pixelSize: 12
            }

            Rectangle {
                id: marginFocusFrame
                objectName: "marginFocusFrame"
                property bool keyboardSelected: fontDialog.keyboardRow === 2
                Layout.preferredWidth: marginChoice.implicitWidth + 4
                implicitHeight: marginChoice.implicitHeight + 4
                color: "transparent"
                radius: 5
                border.width: keyboardSelected ? 2 : 1
                border.color: keyboardSelected
                              ? fontDialog.dialogText
                              : Qt.rgba(fontDialog.dialogText.r, fontDialog.dialogText.g,
                                        fontDialog.dialogText.b, 0.22)

                SpinBox {
                    id: marginChoice
                    objectName: "marginChoice"
                    anchors.fill: parent
                    anchors.margins: 2
                    from: 16
                    to: 240
                    stepSize: 4
                    editable: false
                    onActiveFocusChanged: {
                        if (activeFocus) {
                            fontDialog.keyboardRow = 2
                            Qt.callLater(fontContent.forceActiveFocus)
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: "The quick brown fox — \\sqrt[n]{x+y}"
                color: fontDialog.dialogText
                font.family: fontChoice.currentText
                font.pixelSize: sizeChoice.value
                wrapMode: Text.Wrap
            }

            Label {
                Layout.fillWidth: true
                text: "The margin shrinks with the window. A lower value puts text nearer the sides."
                color: fontDialog.dialogMuted
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }
        }

        footer: DialogButtonBox {
            background: Rectangle { color: "transparent" }

            Button {
                id: cancelButton
                objectName: "fontCancelButton"
                property bool keyboardSelected: fontDialog.keyboardRow === 3
                                                && fontDialog.keyboardButton === 0
                text: "Cancel"
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                onClicked: fontDialog.reject()
                contentItem: Text {
                    text: cancelButton.text
                    color: fontDialog.dialogText
                    font: cancelButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: "transparent"
                    radius: 5
                    border.width: cancelButton.keyboardSelected ? 2 : 0
                    border.color: fontDialog.dialogText
                }
            }

            Button {
                id: okButton
                objectName: "fontOkButton"
                property bool keyboardSelected: fontDialog.keyboardRow === 3
                                                && fontDialog.keyboardButton === 1
                text: "OK"
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: fontDialog.accept()
                contentItem: Text {
                    text: okButton.text
                    color: fontDialog.dialogText
                    font: okButton.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: "transparent"
                    radius: 5
                    border.width: okButton.keyboardSelected ? 2 : 0
                    border.color: fontDialog.dialogText
                }
            }
        }
    }

    GuideWindow {
        id: guideWindow
        backgroundColor: backend.themeBackground
        foregroundColor: backend.themeForeground
        accentColor: backend.themeAccent
        selectionColor: backend.themeSelection
        writingFont: win.editorFont
        writingSize: win.editorSize
        preferredSideMargin: backend.editorSideMargin
    }

    Dialog {
        id: helpDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(500, win.width - 48)
        modal: true
        title: "Keyboard shortcuts"
        standardButtons: Dialog.Ok
        Material.accent: fontDialog.dialogText

        contentItem: Label {
            color: fontDialog.dialogText
            font.family: win.editorFont
            font.pixelSize: 14
            lineHeight: 1.35
            text: "Enter                 Render and move down\n"
                  + "Shift+click           Select a range of lines\n"
                  + "Shift+Up / Down       Extend line selection\n"
                  + "Ctrl+Shift+A          Select every line\n"
                  + "Ctrl+F                Find and replace\n"
                  + "Ctrl+G                Open the LaTeX guide\n"
                  + "Ctrl+K                Find LaTeX syntax\n"
                  + "Ctrl+Shift+R          Auto / source / rendered view\n"
                  + "Ctrl+Shift+E          Export TeX or PDF\n"
                  + "Ctrl+N                New document\n"
                  + "Ctrl+S                Save\n"
                  + "Ctrl+O                Open\n"
                  + "Ctrl+Q                Quit\n"
                  + "F1                    Show this help"
        }
    }

    Dialog {
        id: messageDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(420, win.width - 48)
        modal: true
        title: "FoldTeX"
        standardButtons: Dialog.Ok
        Material.accent: fontDialog.dialogText
        property string message: ""
        contentItem: Label {
            text: messageDialog.message
            color: fontDialog.dialogText
            wrapMode: Text.Wrap
            font.family: win.editorFont
        }
    }

    Dialog {
        id: newDocumentDialog
        objectName: "newDocumentDialog"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(460, win.width - 48)
        modal: true
        title: "New document"
        standardButtons: Dialog.Cancel | Dialog.Ok
        Material.accent: fontDialog.dialogText
        onAccepted: win.startNewDocument()
        contentItem: Label {
            text: "Start a new document? Changes not saved to a .foldtex file will be lost."
            color: fontDialog.dialogText
            wrapMode: Text.Wrap
            font.family: win.editorFont
        }
    }

    Menu {
        id: exportMenu
        parent: exportButton
        x: exportButton.width - width
        y: exportButton.height
        MenuItem { text: "Export TeX…"; onTriggered: texExportDialog.open() }
        MenuItem { text: "Export PDF…"; onTriggered: pdfExportDialog.open() }
    }

    Dialogs.FileDialog {
        id: saveDialog
        title: "Save FoldTeX notes"
        fileMode: Dialogs.FileDialog.SaveFile
        nameFilters: ["FoldTeX notes (*.foldtex)"]
        defaultSuffix: "foldtex"
        onAccepted: saveTo(selectedFile.toString())
    }

    Dialogs.FileDialog {
        id: openDialog
        title: "Open FoldTeX notes"
        fileMode: Dialogs.FileDialog.OpenFile
        nameFilters: ["FoldTeX notes (*.foldtex)"]
        onAccepted: loadData(backend.loadDocument(selectedFile.toString()), selectedFile.toString())
    }

    Dialogs.FileDialog {
        id: texExportDialog
        title: "Export TeX"
        fileMode: Dialogs.FileDialog.SaveFile
        nameFilters: ["TeX document (*.tex)"]
        defaultSuffix: "tex"
        onAccepted: {
            var result = backend.exportTex(selectedFile.toString(), documentTitle, serializedLines())
            messageDialog.message = result.error || "TeX exported"
            messageDialog.open()
        }
    }

    Dialogs.FileDialog {
        id: pdfExportDialog
        title: "Export PDF"
        fileMode: Dialogs.FileDialog.SaveFile
        nameFilters: ["PDF document (*.pdf)"]
        defaultSuffix: "pdf"
        onAccepted: {
            var result = backend.exportPdf(selectedFile.toString(), documentTitle, serializedLines())
            messageDialog.message = result.error || "PDF exported"
            messageDialog.open()
        }
    }

    Item {
        id: edgeMenuArea
        anchors.top: parent.top
        anchors.right: parent.right
        width: 54
        height: edgeMenuPanel.height + 12
        z: 100

        HoverHandler { id: edgeMenuHover }

        Rectangle {
            id: edgeMenuPanel
            objectName: "edgeMenuPanel"
            anchors.top: parent.top
            anchors.topMargin: 6
            anchors.right: parent.right
            anchors.rightMargin: 6
            width: 46
            height: edgeMenuColumn.implicitHeight + 8
            radius: 9
            color: Qt.rgba(win.color.r, win.color.g, win.color.b, 0.96)
            border.width: 1
            border.color: Qt.rgba(win.textColor.r, win.textColor.g, win.textColor.b, 0.14)
            opacity: edgeMenuHover.hovered ? 1 : 0
            enabled: edgeMenuHover.hovered
            Behavior on opacity { NumberAnimation { duration: 150 } }

            Item {
                id: edgeMenuColumn
                anchors.fill: parent
                anchors.margins: 3
                implicitHeight: 240
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 58

            Text {
                id: saveLabel
                objectName: "saveLabel"
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                text: win.saveStatus
                color: win.mutedColor
                font.family: win.editorFont
                font.pixelSize: 12
            }

            Item {
                id: titleSlot
                objectName: "titleSlot"
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: Math.min(520, Math.max(180, parent.width - 220))
                clip: true

                TextField {
                    id: titleField
                    objectName: "titleField"
                    anchors.fill: parent
                    text: win.documentTitle
                    color: win.textColor
                    selectionColor: backend.themeSelection
                    selectedTextColor: backend.themeBackground
                    font.pixelSize: 18
                    font.family: win.editorFont
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    clip: true
                    opacity: activeFocus ? 1 : 0
                    background: null
                    onTextEdited: { win.documentTitle = text; win.changed() }
                }

                Text {
                    id: titleDisplay
                    objectName: "titleDisplay"
                    anchors.fill: parent
                    anchors.leftMargin: titleField.leftPadding
                    anchors.rightMargin: titleField.rightPadding
                    visible: !titleField.activeFocus
                    text: win.documentTitle
                    color: win.textColor
                    font: titleField.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                MouseArea {
                    anchors.fill: parent
                    visible: !titleField.activeFocus
                    onClicked: function(mouse) {
                        var position = titleField.positionAt(mouse.x, mouse.y)
                        titleField.forceActiveFocus()
                        titleField.cursorPosition = position
                    }
                }
            }

            ToolButton {
                objectName: "helpEdgeButton"
                parent: edgeMenuColumn
                x: 0
                y: 40
                width: 40
                height: 40
                text: "?"
                flat: true
                onClicked: helpDialog.open()
                ToolTip.visible: hovered
                ToolTip.text: "Keyboard help  F1"

                contentItem: Text {
                    text: parent.text
                    color: win.textColor
                    font.family: win.editorFont
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            ToolButton {
                objectName: "syntaxEdgeButton"
                parent: edgeMenuColumn
                x: 0
                y: 80
                width: 40
                height: 40
                text: "\\"
                flat: true
                onClicked: openCommandFinder()
                ToolTip.visible: hovered
                ToolTip.text: "Find LaTeX  Ctrl+K"

                contentItem: Text {
                    text: parent.text
                    color: win.textColor
                    font.family: win.editorFont
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            ToolButton {
                objectName: "viewEdgeButton"
                parent: edgeMenuColumn
                x: 0
                y: 120
                width: 40
                height: 40
                text: win.displayMode === 1 ? "S" : win.displayMode === 2 ? "R" : "A"
                flat: true
                onClicked: cycleDisplayMode()
                ToolTip.visible: hovered
                ToolTip.text: "View: " + win.displayModeName() + "  Ctrl+Shift+R"

                contentItem: Text {
                    text: parent.text
                    color: win.textColor
                    font.family: win.editorFont
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            ToolButton {
                id: exportButton
                objectName: "exportEdgeButton"
                parent: edgeMenuColumn
                x: 0
                y: 160
                width: 40
                height: 40
                text: "⇩"
                flat: true
                onClicked: exportMenu.open()
                ToolTip.visible: hovered
                ToolTip.text: "Export TeX or PDF  Ctrl+Shift+E"

                contentItem: Text {
                    text: parent.text
                    color: win.textColor
                    font.family: win.editorFont
                    font.pixelSize: 18
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            ToolButton {
                id: guideHeaderButton
                objectName: "guideHeaderButton"
                parent: edgeMenuColumn
                x: 0
                y: 200
                width: 40
                height: 40
                text: "G"
                flat: true
                onClicked: openGuide()
                ToolTip.visible: hovered
                ToolTip.text: "LaTeX guide  Ctrl+G"

                contentItem: Text {
                    text: parent.text
                    color: win.textColor
                    font.family: win.editorFont
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            ToolButton {
                objectName: "settingsEdgeButton"
                parent: edgeMenuColumn
                x: 0
                y: 0
                width: 40
                height: 40
                text: "Aa"
                flat: true
                onClicked: fontDialog.open()
                ToolTip.visible: hovered
                ToolTip.text: "Font, size, and margin  Ctrl+', Ctrl+, or Ctrl+Shift+F"

                contentItem: Text {
                    text: parent.text
                    color: win.textColor
                    font.family: win.editorFont
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Qt.rgba(win.textColor.r, win.textColor.g, win.textColor.b, 0.08)
        }

        ListView {
            id: list
            objectName: "documentList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: lines
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            spacing: 2
            topMargin: 46
            bottomMargin: 120

            WheelHandler {
                target: null
                onWheel: function(event) {
                    documentScrollLinger.restart()
                    var delta = win.wheelDistance(event.pixelDelta.y,
                                                  event.angleDelta.y)
                    win.scrollDocumentBy(-delta)
                    event.accepted = true
                }
            }

            DropArea {
                anchors.fill: parent
                z: 20
                onDropped: function(drop) {
                    if (drop.urls && drop.urls.length)
                        win.addImportedImage(backend.importAsset(win.documentPath, drop.urls[0]))
                }
            }

            delegate: Item {
                id: row
                required property int index
                required property string source
                required property string renderedUrl
                required property string error
                required property bool math
                required property string kind
                required property string label
                required property string asset
                required property int slide
                readonly property var currentContent: contentLoader.item
                readonly property bool hasTypeLabel: kind !== "normal" && kind !== "image"
                width: list.width
                height: Math.max(win.editorSize * 2.8,
                                 contentLoader.y + contentLoader.implicitHeight + 10)

                Rectangle {
                    visible: row.hasTypeLabel
                    anchors.left: contentLoader.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 2
                    radius: 1
                    color: row.kind === "catchup" ? "#e2ad5b" : backend.themeAccent
                    opacity: 0.72
                }

                Text {
                    visible: row.hasTypeLabel
                    anchors.left: contentLoader.left
                    anchors.leftMargin: 10
                    anchors.top: parent.top
                    text: row.label.length ? row.label : row.kind
                    color: row.kind === "catchup" ? "#e2ad5b" : win.mutedColor
                    font.family: win.editorFont
                    font.pixelSize: Math.max(10, win.editorSize - 5)
                    font.capitalization: Font.AllUppercase
                }

                Rectangle {
                    anchors.left: contentLoader.left
                    anchors.right: contentLoader.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    radius: 4
                    color: {
                        var searchLevel = win.searchHighlightLevel(row.index)
                        if (searchLevel === 2)
                            return Qt.rgba(backend.themeSelection.r, backend.themeSelection.g,
                                           backend.themeSelection.b, 0.42)
                        if (searchLevel === 1)
                            return Qt.rgba(backend.themeSelection.r, backend.themeSelection.g,
                                           backend.themeSelection.b, 0.16)
                        return win.lineIsSelected(row.index)
                                ? Qt.rgba(win.textColor.r, win.textColor.g, win.textColor.b, 0.12)
                                : "transparent"
                    }
                }

                Loader {
                    id: contentLoader
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: row.hasTypeLabel ? 22 : 0
                    width: win.pageWidth
                    sourceComponent: row.kind === "image" ? imageComponent
                                     : win.displayMode === 1 ? sourceViewComponent
                                     : win.displayMode === 2 ? renderComponent
                                     : row.index === win.activeIndex ? editorComponent : renderComponent
                }

                Text {
                    visible: row.kind !== "image" && row.error.length > 0
                    anchors.right: contentLoader.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: "⚠"
                    color: "#ff6b63"
                    font.pixelSize: 17
                }

                TapHandler {
                    acceptedModifiers: Qt.NoModifier
                    onTapped: win.editLine(row.index)
                }
                TapHandler {
                    acceptedModifiers: Qt.ShiftModifier
                    onTapped: win.extendLineSelection(row.index)
                }

                Component {
                    id: imageComponent
                    Column {
                        spacing: 6
                        Image {
                            width: Math.min(parent.width, implicitWidth)
                            height: implicitWidth > parent.width
                                    ? implicitHeight * parent.width / implicitWidth : implicitHeight
                            source: row.asset.length ? "file://" + row.asset : ""
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            smooth: true
                        }
                        Text {
                            visible: row.source.length > 0
                            width: parent.width
                            text: row.source
                            color: win.mutedColor
                            wrapMode: Text.Wrap
                            font.family: win.editorFont
                            font.pixelSize: win.editorSize - 2
                        }
                    }
                }

                Component {
                    id: sourceViewComponent
                    Text {
                        text: row.source
                        color: win.textColor
                        font.family: win.editorFont
                        font.pixelSize: win.editorSize
                        wrapMode: Text.Wrap
                    }
                }

                Component {
                    id: editorComponent
                    TextArea {
                        id: editor
                        text: row.source
                        color: win.textColor
                        selectionColor: backend.themeSelection
                        selectedTextColor: backend.themeBackground
                        font.family: win.editorFont
                        font.pixelSize: win.editorSize
                        wrapMode: TextEdit.WrapAtWordBoundaryOrAnywhere
                        padding: 0
                        leftPadding: 0
                        rightPadding: 28
                        background: null
                        focus: true
                        implicitHeight: Math.max(42, contentHeight + 8)
                        Keys.priority: Keys.BeforeItem
                        onTextChanged: {
                            if (text !== row.source) {
                                lines.setProperty(row.index, "source", text)
                                lines.setProperty(row.index, "error", "")
                                win.changed()
                            }
                        }
                        Keys.onReturnPressed: function(event) {
                            event.accepted = true
                            win.commitLine(row.index)
                        }
                        Keys.onTabPressed: function(event) {
                            event.accepted = win.handleSnippetTab(editor, row.index)
                        }
                        Keys.onUpPressed: function(event) {
                            if (event.modifiers & Qt.ShiftModifier) {
                                win.extendLineSelectionBy(-1)
                                event.accepted = true
                            } else if (editor.cursorRectangle.y > 1) {
                                event.accepted = false
                            } else if (row.index > 0) {
                                event.accepted = true
                                win.editLine(row.index - 1)
                            } else event.accepted = false
                        }
                        Keys.onDownPressed: function(event) {
                            if (event.modifiers & Qt.ShiftModifier) {
                                win.extendLineSelectionBy(1)
                                event.accepted = true
                            } else if (row.index < lines.count - 1) {
                                event.accepted = true
                                win.editLine(row.index + 1)
                            } else event.accepted = false
                        }
                        Keys.onPressed: function(event) {
                            if (event.key === Qt.Key_Z && (event.modifiers & Qt.ControlModifier)) {
                                if (event.modifiers & Qt.ShiftModifier) win.redoDocument()
                                else win.undoDocument()
                                event.accepted = true
                            } else if (win.hasLineSelection
                                    && event.key === Qt.Key_C
                                    && (event.modifiers & Qt.ControlModifier)) {
                                win.copySelectedLines()
                                event.accepted = true
                            } else if (win.hasLineSelection
                                       && event.key === Qt.Key_X
                                       && (event.modifiers & Qt.ControlModifier)) {
                                win.cutSelectedLines()
                                event.accepted = true
                            } else if (win.hasLineSelection
                                       && (event.key === Qt.Key_Delete
                                           || event.key === Qt.Key_Backspace)) {
                                win.deleteSelectedLines()
                                event.accepted = true
                            } else if (event.key === Qt.Key_Backspace) {
                                event.accepted = win.joinWithPrevious(editor, row.index)
                            } else if (event.key === Qt.Key_Delete) {
                                event.accepted = win.joinWithNext(editor, row.index)
                            } else if (event.key === Qt.Key_V
                                       && (event.modifiers & Qt.ControlModifier)) {
                                event.accepted = win.pasteMultiline(editor, row.index,
                                                                    backend.clipboardText())
                            } else event.accepted = false
                        }
                        Component.onCompleted: {
                            forceActiveFocus()
                            cursorPosition = text.length
                        }
                    }
                }

                Component {
                    id: renderComponent
                    Item {
                        implicitHeight: Math.max(42, visual.implicitHeight)
                        Text {
                            id: plainText
                            visible: !row.math || row.error.length > 0 || row.renderedUrl.length === 0
                            width: parent.width - 30
                            text: row.source.length ? row.source : ""
                            color: row.error.length ? "#ff8c85" : win.textColor
                            font.family: win.editorFont
                            font.pixelSize: win.editorSize
                            wrapMode: Text.Wrap
                        }
                        Image {
                            id: visual
                            property real logicalSourceWidth: 0
                            property real logicalSourceHeight: 0
                            visible: row.math && row.error.length === 0 && row.renderedUrl.length > 0
                            source: row.renderedUrl
                            asynchronous: false
                            cache: false
                            smooth: true
                            mipmap: true
                            fillMode: Image.PreserveAspectFit
                            sourceSize.width: logicalSourceWidth > 0
                                              ? Math.ceil(logicalSourceWidth * Screen.devicePixelRatio)
                                              : 0
                            sourceSize.height: logicalSourceHeight > 0
                                               ? Math.ceil(logicalSourceHeight * Screen.devicePixelRatio)
                                               : 0
                            width: Math.min(parent.width - 30,
                                            logicalSourceWidth > 0 ? logicalSourceWidth : implicitWidth)
                            height: {
                                var naturalWidth = logicalSourceWidth > 0 ? logicalSourceWidth : implicitWidth
                                var naturalHeight = logicalSourceHeight > 0 ? logicalSourceHeight : implicitHeight
                                return naturalWidth > width ? naturalHeight * width / naturalWidth : naturalHeight
                            }

                            onSourceChanged: {
                                logicalSourceWidth = 0
                                logicalSourceHeight = 0
                            }
                            onStatusChanged: {
                                if (status === Image.Ready && logicalSourceWidth === 0
                                        && implicitWidth > 0 && implicitHeight > 0) {
                                    logicalSourceWidth = implicitWidth
                                    logicalSourceHeight = implicitHeight
                                }
                            }
                        }
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {
                id: documentScrollBar
                objectName: "documentScrollBar"
                policy: ScrollBar.AsNeeded
                active: hovered || pressed || list.moving || documentScrollLinger.running
                implicitWidth: 9
                leftPadding: 3
                rightPadding: 3
                topPadding: 8
                bottomPadding: 8
                opacity: active ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 160 } }
                background: Item { }
                contentItem: Rectangle {
                    implicitWidth: 3
                    radius: width / 2
                    color: Qt.rgba(win.textColor.r, win.textColor.g, win.textColor.b, 0.48)
                }
            }
        }
    }

    Component.onCompleted: {
        var recovered = backend.loadRecovery()
        if (!recovered.error && recovered.lines && recovered.lines.length)
            loadData(recovered, "")
        else {
            loading = true
            lines.append(makeLine("\\sqrt[n]{x_1 + x_2 + \\cdots + x_k}"))
            lines.append(makeLine(""))
            activeIndex = 1
            loading = false
            resetHistory(true)
            Qt.callLater(renderAll)
        }
    }
}
