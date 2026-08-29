import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: guide
    objectName: "guideWindow"
    width: 720
    height: 660
    minimumWidth: 520
    minimumHeight: 420
    visible: false
    title: "FoldTeX Guide"
    color: backgroundColor
    flags: Qt.Window | Qt.FramelessWindowHint
    transientParent: null

    property color backgroundColor: "#101010"
    property color foregroundColor: "#eeeeee"
    property color accentColor: "#7aa2c7"
    property color selectionColor: "#315e7d"
    property string writingFont: "monospace"
    property int writingSize: 17
    property int preferredSideMargin: 100
    property string copiedText: ""

    readonly property real marginScale: Math.max(0, Math.min(1,
                                                (width - 560) / (980 - 560)))
    readonly property int pageMargin: 16 + (preferredSideMargin - 16) * marginScale
    readonly property color mutedColor: Qt.rgba(foregroundColor.r, foregroundColor.g,
                                                 foregroundColor.b, 0.52)
    readonly property var sections: [
        {
            title: "How FoldTeX reads a note",
            body: "Use one formula or one prose line per row. A new empty note focuses its first row; you can also click any unused writing space to start. Press Enter to render the row and move down. Click a rendered formula to edit it again. FoldTeX adds math mode, so dollar signs are optional.",
            code: "5 \\cdot 5 = 25\nThis is a plain text note.",
            keywords: "foldtex enter row prose text render dollar"
        },
        {
            title: "Fast LaTeX snippets",
            body: "Type a short name and press Tab. FoldTeX expands sqrt, root, frac, sum, prod, int, lim, cases, and mat2. Tab then moves through the parts you need to replace.",
            code: "frac  then Tab\nroot  then Tab\nmat2  then Tab",
            keywords: "snippet tab fast template placeholder sqrt root frac sum product integral limit cases matrix"
        },
        {
            title: "Find text or LaTeX",
            body: "Ctrl+F finds and replaces words in both prose and the source behind rendered math. Ctrl+K searches common LaTeX by plain names such as root, fraction, or integral.",
            code: "Ctrl+F   find in this note\nCtrl+K   find LaTeX syntax",
            keywords: "find search replace latex command syntax ctrl f k"
        },
        {
            title: "Select and edit rows",
            body: "Shift-click or use Shift+Up and Shift+Down to select rows. Copy, cut, Backspace, and Delete act on the selection. Ctrl+Shift+A selects every row. Pasted text with line breaks becomes separate rows.",
            code: "Shift+Up / Shift+Down\nCtrl+Shift+A",
            keywords: "select rows lines copy cut delete paste multiline join backspace"
        },
        {
            title: "Scroll the document",
            body: "When you are not editing a row, Up and Down scroll the page. While typing, they move through wrapped text and between rows. The right scrollbar stays faintly visible, remains bright after scrolling, and can be dragged. Only the top-right corner opens the tool menu.",
            code: "Up / Down",
            keywords: "scroll document page arrow up down scrollbar drag trackpad menu corner"
        },
        {
            title: "Undo and redo",
            body: "Ctrl+Z undoes changes across the whole note. Ctrl+Shift+Z redoes them. A snippet expansion counts as one change.",
            code: "Ctrl+Z\nCtrl+Shift+Z",
            keywords: "undo redo history restore change"
        },
        {
            title: "Save, open, and recover",
            body: "Ctrl+N starts a note. Ctrl+S saves, Ctrl+Shift+S saves as, and Ctrl+O opens a note. FoldTeX keeps recovery data as you type and up to 30 timed snapshots.",
            code: "Ctrl+N   new\nCtrl+S   save\nCtrl+Shift+S   save as\nCtrl+O   open",
            keywords: "new save save as open recovery snapshot autosave file document"
        },
        {
            title: "Source, rendered view, and export",
            body: "Ctrl+Shift+R cycles through automatic, all-source, and all-rendered views. Ctrl+Shift+E exports the note as a TeX file or PDF.",
            code: "Ctrl+Shift+R   change view\nCtrl+Shift+E   export",
            keywords: "source raw rendered automatic view toggle export tex pdf"
        },
        {
            title: "Definitions, theorems, and proofs",
            body: "Ctrl+period marks the active row as a definition, theorem, proof, example, or normal row. Ctrl+M adds a timed catch-up mark when the lecture moves ahead.",
            code: "Ctrl+.   choose row type\nCtrl+M   add catch-up mark",
            keywords: "definition theorem proof example row type catch up marker lecture"
        },
        {
            title: "Courses and lecture details",
            body: "Ctrl+Alt+L sets the course, lecture name, and date. Ctrl+Alt+F searches every FoldTeX note in the current note's folder.",
            code: "Ctrl+Alt+L   lecture details\nCtrl+Alt+F   search course notes",
            keywords: "course lecture date details search folder notes university"
        },
        {
            title: "Images",
            body: "Save the note first, then press Ctrl+Shift+V to paste an image from the clipboard. FoldTeX copies images into an asset folder beside the note and inserts an image row.",
            code: "Ctrl+Shift+V",
            keywords: "image picture screenshot clipboard paste asset figure"
        },
        {
            title: "Writing settings",
            body: "Open Aa with Ctrl+comma, Ctrl+apostrophe, or Ctrl+Shift+F. Choose the font, text size, and side margin. The margin shrinks with the window, and the size applies to source and rendered math.",
            code: "Ctrl+,\nCtrl+'\nCtrl+Shift+F",
            keywords: "settings font size margin aa writing scale theme"
        },
        {
            title: "Roots",
            body: "Use sqrt for a square root. Put the root number in square brackets for an nth root.",
            code: "\\sqrt{x}\n\\sqrt[3]{x}\n\\sqrt[n]{x_1 + x_2}",
            keywords: "root square cube nth radical sqrt"
        },
        {
            title: "Fractions",
            body: "The first brace group is the top and the second is the bottom.",
            code: "\\frac{a}{b}\n\\frac{x + 1}{x - 1}",
            keywords: "fraction divide over numerator denominator"
        },
        {
            title: "Powers and subscripts",
            body: "A caret raises text. An underscore lowers it. Use braces when the part has more than one character.",
            code: "x^2\nx^{10}\nx_1\nx_{n+1}",
            keywords: "power exponent superscript subscript index"
        },
        {
            title: "Multiply and group",
            body: "Use cdot or times for multiplication. Left and right make brackets grow with their contents.",
            code: "a \\cdot b\na \\times b\n\\left( \\frac{a}{b} \\right)",
            keywords: "multiply multiplication times dot parentheses brackets group"
        },
        {
            title: "Text inside a formula",
            body: "Normal words inside math need text followed by braces. Spaces inside plain math are otherwise ignored.",
            code: "x = 2 \\text{ when } y > 0",
            keywords: "words prose normal text spaces equation"
        },
        {
            title: "Aligned equations",
            body: "Use aligned when several equation rows must form one rendered block. Two backslashes end a row; ampersands mark the alignment point.",
            code: "\\begin{aligned}\n  x + y &= 10 \\\\\n  x - y &= 2\n\\end{aligned}",
            keywords: "align aligned multiline multiple rows line break equations"
        },
        {
            title: "Sums, products, and integrals",
            body: "Lower and upper limits use subscript and power syntax.",
            code: "\\sum_{i=1}^{n} x_i\n\\prod_{i=1}^{n} x_i\n\\int_{a}^{b} f(x)\\,dx",
            keywords: "sum product integral sigma limits calculus"
        },
        {
            title: "Greek letters",
            body: "Write the letter name after a backslash. Capital forms begin with a capital letter when LaTeX provides one.",
            code: "\\alpha  \\beta  \\theta  \\pi  \\sigma  \\Omega",
            keywords: "greek alpha beta theta pi sigma omega"
        },
        {
            title: "Comparisons and arrows",
            body: "These commands produce common relation signs.",
            code: "x \\neq y\nx \\leq y\nx \\geq y\nx \\approx y\nx \\to \\infty",
            keywords: "equal not less greater approximately arrow infinity comparison"
        },
        {
            title: "Matrices",
            body: "Ampersands split columns and two backslashes split rows.",
            code: "\\begin{bmatrix}\n  a & b \\\\\n  c & d\n\\end{bmatrix}",
            keywords: "matrix grid rows columns bmatrix"
        },
        {
            title: "Cases",
            body: "Cases are useful for rules that change under different conditions.",
            code: "\\begin{cases}\n  x, & x > 0 \\\\\n  0, & x \\leq 0\n\\end{cases}",
            keywords: "cases piecewise conditions"
        },
        {
            title: "Spacing",
            body: "Math spacing is automatic. Use these only when you need to tune it: comma is small, colon is medium, semicolon is large, and quad is very large.",
            code: "a\\,b\na\\:b\na\\;b\na\\quad b",
            keywords: "space spacing gap comma colon semicolon quad"
        },
        {
            title: "Functions and logarithms",
            body: "Use named commands for common functions so they receive the right shape and spacing. Powers and subscripts work on them as usual.",
            code: "\\sin(x)\n\\cos^2(x)\n\\tan(\\theta)\n\\log_{10}(x)\n\\ln(x)\n\\exp(x)",
            keywords: "function sine cosine tangent log logarithm ln exponential"
        },
        {
            title: "Limits and derivatives",
            body: "Put the value being approached below lim. Fractions are the usual way to write ordinary, higher-order, and partial derivatives.",
            code: "\\lim_{x \\to 0} \\frac{\\sin x}{x}\n\\frac{dy}{dx}\n\\frac{d^2y}{dx^2}\n\\frac{\\partial f}{\\partial x}",
            keywords: "limit derivative differential partial calculus approaches"
        },
        {
            title: "Vectors and accents",
            body: "An accent applies to the group inside its braces. Use vec for an arrow, hat for a hat, and overline for a bar over longer content.",
            code: "\\vec{v}\n\\hat{x}\n\\bar{x}\n\\overline{AB}\n\\dot{x}\n\\ddot{x}",
            keywords: "vector accent arrow hat bar overline dot"
        },
        {
            title: "Sets and logic",
            body: "These commands cover membership, subsets, unions, intersections, number sets, and common logic signs.",
            code: "x \\in A\nA \\subseteq B\nA \\cup B\nA \\cap B\n\\mathbb{R}\nP \\land Q\nP \\lor Q\n\\forall x \\in A",
            keywords: "set membership subset union intersection real numbers logic and or forall"
        },
        {
            title: "Brackets and absolute values",
            body: "Use left and right when brackets must grow around a fraction, matrix, or other tall expression. A vertical bar can mark an absolute value.",
            code: "\\left( \\frac{a}{b} \\right)\n\\left[ \\frac{x}{y} \\right]\n\\left| x - 2 \\right|\n\\left\\{ x \\right\\}",
            keywords: "bracket parentheses square brace absolute value delimiter grow"
        },
        {
            title: "Binomials and combinations",
            body: "Use binom for a binomial coefficient. It places the two groups above and below without a fraction line.",
            code: "\\binom{n}{k}\n(1+x)^n = \\sum_{k=0}^{n} \\binom{n}{k}x^k",
            keywords: "binomial coefficient choose combination expansion"
        },
        {
            title: "Floor, ceiling, and rounding",
            body: "Floor rounds down and ceiling rounds up. Pair each opening delimiter with its matching closing delimiter.",
            code: "\\lfloor x \\rfloor\n\\lceil x \\rceil\n\\left\\lfloor \\frac{n}{2} \\right\\rfloor",
            keywords: "floor ceiling round rounding delimiter"
        },
        {
            title: "Math styles",
            body: "Use mathbf for bold symbols, mathrm for upright text, mathcal for calligraphic capitals, and mathbb for blackboard-bold number sets.",
            code: "\\mathbf{v}\n\\mathrm{d}x\n\\mathcal{F}\n\\mathbb{N}\n\\mathbb{R}",
            keywords: "bold upright roman calligraphic blackboard font style"
        },
        {
            title: "Common errors",
            body: "Check that every opening brace has a closing brace. Keep prose on its own FoldTeX row. Do not use linebreak between a formula and prose. Write text{words}, not text words.",
            code: "\\text{correct words}\n\\frac{complete}{groups}\n\\sqrt[3]{complete root}",
            keywords: "error missing inserted brace linebreak wrong debug"
        },
        {
            title: "Figures",
            body: "Press Ctrl+Alt+I to draw beside your notes. The figure tool has a pen, eraser, lines, arrows, boxes, and text. Save inserts the figure as its own note row.",
            code: "Ctrl+Alt+I",
            keywords: "figure drawing draw pen eraser line arrow box text diagram"
        },
        {
            title: "Lecture slides",
            body: "Press Ctrl+Alt+P and choose a PDF. FoldTeX copies it beside the saved note, then shows the slides next to the note when the window is wide. Link the active row to the current slide with the arrow button.",
            code: "Ctrl+Alt+P",
            keywords: "slide slides pdf lecture split link page copy"
        }
    ]

    function matchingSections(query) {
        var needle = query.trim().toLowerCase()
        if (!needle.length) return sections
        var result = []
        for (var i = 0; i < sections.length; ++i) {
            var section = sections[i]
            var text = (section.title + " " + section.body + " " + section.code
                        + " " + section.keywords).toLowerCase()
            if (text.indexOf(needle) >= 0) result.push(section)
        }
        return result
    }

    function copySnippet(text) {
        backend.setClipboardText(text)
        copiedText = text
        copiedTimer.restart()
    }

    function scrollBy(amount) {
        var view = guideScroll.contentItem
        var minimum = view.originY
        var maximum = Math.max(minimum, minimum + view.contentHeight - view.height)
        view.contentY = Math.max(minimum, Math.min(maximum, view.contentY + amount))
    }

    function wheelDistance(pixelDelta, angleDelta) {
        if (pixelDelta !== 0) return pixelDelta * 80
        return Math.abs(angleDelta) < 120 ? angleDelta * 80 : angleDelta
    }

    Timer {
        id: copiedTimer
        interval: 1200
        onTriggered: guide.copiedText = ""
    }

    Timer {
        id: guideScrollLinger
        interval: 700
    }

    Shortcut {
        sequence: "Escape"
        context: Qt.WindowShortcut
        onActivated: guide.close()
    }
    Shortcut {
        sequence: "Ctrl+G"
        context: Qt.WindowShortcut
        onActivated: guide.close()
    }
    Shortcut {
        sequence: "Up"
        context: Qt.WindowShortcut
        enabled: guide.visible
        onActivated: guide.scrollBy(-Math.max(48, guide.writingSize * 3))
    }
    Shortcut {
        sequence: "Down"
        context: Qt.WindowShortcut
        enabled: guide.visible
        onActivated: guide.scrollBy(Math.max(48, guide.writingSize * 3))
    }
    Shortcut {
        sequence: "PageUp"
        context: Qt.WindowShortcut
        enabled: guide.visible
        onActivated: guide.scrollBy(-guideScroll.height * 0.8)
    }
    Shortcut {
        sequence: "PageDown"
        context: Qt.WindowShortcut
        enabled: guide.visible
        onActivated: guide.scrollBy(guideScroll.height * 0.8)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 58

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: guide.pageMargin
                anchors.rightMargin: guide.pageMargin
                spacing: 16

                Text {
                    text: "LaTeX guide"
                    color: guide.foregroundColor
                    font.family: guide.writingFont
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }

                TextField {
                    id: guideSearch
                    objectName: "guideSearch"
                    Layout.fillWidth: true
                    placeholderText: "Search roots, fractions, spacing…"
                    color: guide.foregroundColor
                    selectionColor: guide.selectionColor
                    selectedTextColor: guide.backgroundColor
                    font.family: guide.writingFont
                    font.pixelSize: Math.max(14, guide.writingSize - 2)
                    background: null
                }

                Text {
                    text: "×"
                    color: closeHover.hovered ? guide.foregroundColor : guide.mutedColor
                    font.pixelSize: 24
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    HoverHandler { id: closeHover }
                    TapHandler { onTapped: guide.close() }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Qt.rgba(guide.foregroundColor.r, guide.foregroundColor.g,
                           guide.foregroundColor.b, 0.10)
        }

        ScrollView {
            id: guideScroll
            objectName: "guideScroll"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            WheelHandler {
                target: null
                onWheel: function(event) {
                    guideScrollLinger.restart()
                    var delta = guide.wheelDistance(event.pixelDelta.y,
                                                    event.angleDelta.y)
                    guide.scrollBy(-delta)
                    event.accepted = true
                }
            }

            ScrollBar.vertical: ScrollBar {
                id: guideScrollBar
                objectName: "guideScrollBar"
                policy: ScrollBar.AsNeeded
                active: hovered || pressed || guideScroll.contentItem.moving
                        || guideScrollLinger.running
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
                    color: Qt.rgba(guide.foregroundColor.r, guide.foregroundColor.g,
                                   guide.foregroundColor.b, 0.48)
                }
            }

            Column {
                id: guidePage
                objectName: "guidePage"
                width: guideScroll.availableWidth
                topPadding: 26
                bottomPadding: 54

                Repeater {
                    model: guide.matchingSections(guideSearch.text)

                    delegate: Column {
                        required property var modelData
                        width: guidePage.width
                        leftPadding: guide.pageMargin
                        rightPadding: guide.pageMargin
                        spacing: 8

                        Text {
                            width: parent.width - parent.leftPadding - parent.rightPadding
                            text: parent.modelData.title
                            color: guide.foregroundColor
                            font.family: guide.writingFont
                            font.pixelSize: guide.writingSize
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                        }

                        Text {
                            width: parent.width - parent.leftPadding - parent.rightPadding
                            text: parent.modelData.body
                            color: guide.mutedColor
                            font.family: guide.writingFont
                            font.pixelSize: Math.max(13, guide.writingSize - 2)
                            wrapMode: Text.Wrap
                            lineHeight: 1.2
                        }

                        Item {
                            width: parent.width - parent.leftPadding - parent.rightPadding
                            height: snippet.implicitHeight + 8

                            Text {
                                id: snippet
                                anchors.left: parent.left
                                anchors.right: copyHint.left
                                anchors.rightMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                text: parent.parent.modelData.code
                                color: guide.accentColor
                                font.family: guide.writingFont
                                font.pixelSize: Math.max(14, guide.writingSize - 1)
                                wrapMode: Text.Wrap
                            }

                            Text {
                                id: copyHint
                                anchors.right: parent.right
                                anchors.top: parent.top
                                text: guide.copiedText === parent.parent.modelData.code ? "copied" : "copy"
                                color: snippetHover.hovered ? guide.foregroundColor : guide.mutedColor
                                font.family: guide.writingFont
                                font.pixelSize: 11
                            }

                            HoverHandler { id: snippetHover }
                            TapHandler { onTapped: guide.copySnippet(parent.parent.modelData.code) }
                        }

                        Rectangle {
                            width: parent.width - parent.leftPadding - parent.rightPadding
                            height: 1
                            color: Qt.rgba(guide.foregroundColor.r, guide.foregroundColor.g,
                                           guide.foregroundColor.b, 0.08)
                        }

                        Item { width: 1; height: 20 }
                    }
                }

                Text {
                    visible: guide.matchingSections(guideSearch.text).length === 0
                    width: guidePage.width
                    horizontalAlignment: Text.AlignHCenter
                    text: "No guide entry found"
                    color: guide.mutedColor
                    font.family: guide.writingFont
                    font.pixelSize: guide.writingSize
                }
            }
        }
    }

    onVisibleChanged: {
        if (visible) Qt.callLater(function() { guideSearch.forceActiveFocus() })
    }
}
