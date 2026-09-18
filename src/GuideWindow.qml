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
    property var backendApi: null
    property var builtInSnippetTriggers: []
    property var builtInSnippetTemplates: ({})
    property var customSnippetEntries: []

    readonly property real marginScale: Math.max(0, Math.min(1,
                                                (width - 560) / (980 - 560)))
    readonly property int pageMargin: 16 + (preferredSideMargin - 16) * marginScale
    readonly property color mutedColor: Qt.rgba(foregroundColor.r, foregroundColor.g,
                                                 foregroundColor.b, 0.52)
    readonly property var sections: [
        {
            title: "Stavningskontroll / Spellchecking",
            body: "Felstavade ord får en röd våglinje. Klicka eller högerklicka på ordet och välj ett förslag, eller ignorera det i dokumentet. Språket väljs under ⋯ → Stavningskontroll: Svenska eller English. Svenska är standard för nya och äldre dokument; språkval och ignorerade ord sparas med anteckningen. Rättningar kan ångras. Kontrollen körs lokalt och hoppar över formler och LaTeX-kommandon.",
            code: "Det häär är en text. → Det här är en text.\nThis sentnce is correct. → This sentence is correct.",
            keywords: "spelling spellcheck language Swedish English svenska engelska rättstavning stavningskontroll språk röd understrykning"
        },
        {
            title: "How FoldTeX reads a note",
            body: "Write continuous text with $…$ inline formulas. Only the formula containing the cursor shows its LaTeX source; other expressions render in place. Enter inserts a source newline at the cursor and preserves environments. Ctrl+Enter inserts a paragraph below the current block. Older bare formulas receive LaTeX delimiters when opened. There is no per-row Math/Text switch.",
            code: "5 \\cdot 5 = 25\nThis is a plain text note.",
            keywords: "foldtex enter row prose text render dollar"
        },
        {
            title: "Fast LaTeX snippets",
            body: "Type a short name and press Tab. Core snippets include text, sqrt, frac, sum, int, cases, and mat2. Course snippets include truth, induct, gcd, graph, adjacency, colvec, linsys, rowswap, rowadd, charpoly, diag, orthcomp, changevar, polar, flux, ode1, and ode2. Type nn, zz, qq, rr, or cc for a number set. Tab and Shift+Tab move between fields.",
            code: "truth    then Tab\nlinsys   then Tab\nchangevar then Tab\nTab / Shift+Tab   next / prior field",
            keywords: "snippet tab fast template truth induction gcd graph adjacency column vector linear system row operation characteristic polynomial diagonalisation orthogonal change variables polar flux differential equation"
        },
        {
            title: "Matching environments",
            body: "Finish typing an environment opening and FoldTeX adds its matching ending, with the cursor on an empty line between them. An existing matching ending is reused. If you rename an environment later, update its ending too. Unclosed math delimiters, braces, and environments stay editable while you finish writing.",
            code: "\\begin{align}\n  a &= b\n\\end{align}",
            keywords: "begin end environment align matching braces incomplete"
        },
        {
            title: "Automatic math pairs",
            body: "Type a dollar sign to insert an inline pair with the cursor between them. Type another immediately to make a display pair. For an existing $…$ formula, type $ at its opening or just after its closing delimiter to upgrade both ends to $$…$$. Double delimiters do not gain a third dollar. Inside math, $ inserts one character or skips an existing closer. Backspace/Delete on a double delimiter reduces both ends to single dollars; undo restores them. Opening LaTeX parentheses or brackets inserts the matching ending too. Type an existing closer to move past it, or Backspace inside an empty pair to remove both sides. A dollar sign, brace, or parenthesis can wrap selected text. Escaped dollars and dollars in comments stay literal.",
            code: "$|$    type $ again    $$|$$\n\\(|\\)    \\[|\\]\n| represents the cursor",
            keywords: "dollar pair delimiters brackets parentheses backspace selection automatic"
        },
        {
            title: "Text formatting and document macros",
            body: "Use textbf for bold, emph or textit for italics, and texttt for code in prose. Type the command prefix and Tab to insert a text template. Put package imports and macro definitions at the top of the same note, before its content. Preview and export share them. Use math delimiters for math macros. Packages need to be installed in your local TeX distribution.",
            code: "\\usepackage{bm}\n\\newcommand{\\RR}{\\mathbb{R}}\n\nA \\textbf{bold statement}: $x \\in \\RR$.",
            keywords: "textbf textit emph texttt bold italic preamble usepackage newcommand macro package"
        },
        {
            title: "Custom Tab snippets",
            body: "Press Ctrl+Alt+S to create snippets and aliases. A new snippet works in all courses by default. You can limit it to named courses, disable it without deleting it, or import and export the full custom list. Use «name» in an expansion to create a Tab stop. A custom trigger overrides a built-in trigger in its course scope.",
            code: "Trigger: basis\nAlias: bas\nExpansion: \\operatorname{span}\\{«v_1», «v_n»\\}\nScope: All courses",
            keywords: "custom snippet alias course global scope tab stop override import export manager"
        },
        {
            title: "Find text or LaTeX",
            body: "Ctrl+F finds and replaces words in both prose and the source behind rendered math. Ctrl+K searches common LaTeX by Swedish or English names such as delmängd/subset, gränsvärde/limit, or integral. Results show the Swedish name first and the English name after it.",
            code: "Ctrl+F   find in this note\nCtrl+K   find LaTeX syntax\nTry: delmängd, gränsvärde, derivata",
            keywords: "find search replace latex command syntax Swedish English svenska engelska sök ctrl f k"
        },
        {
            title: "Select and edit rows",
            body: "Drag, Shift-click, or Shift+arrows to select text across paragraphs and formulas. Copy, cut, Backspace, and Delete act on the selection. Ctrl+A or Ctrl+Shift+A selects the document. Pasted LaTeX environments remain together.",
            code: "Shift+Up / Shift+Down\nCtrl+Shift+A",
            keywords: "select rows lines copy cut delete paste multiline join backspace"
        },
        {
            title: "Scroll the document",
            body: "Up and Down move through wrapped text and paragraphs while keeping the cursor column. Both directions enter rendered formulas and reveal their source. Display formulas are centered within the writing area. Use the wheel, trackpad, or scrollbar to scroll. The visible ⋯ button at the top right opens document tools.",
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
            body: "Ctrl+N starts an empty note immediately and preserves the current course. Set its details later with Ctrl+Alt+L. FoldTeX saves an open note after one second of idle time and keeps a separate recovery copy plus up to 30 timed snapshots. Open Recovery from the notes library to restore or remove them. Safe Save As copies figures and slides with the note. If another app changes the same file, FoldTeX keeps your work in Recovery instead of overwriting it.",
            code: "Ctrl+N   start writing\nCtrl+S   save now\nCtrl+Shift+S   safe save as\nCtrl+O   library and recovery",
            keywords: "new title course lecture date save save as open recovery snapshot autosave file document"
        },
        {
            title: "Source, rendered view, and export",
            body: "Ctrl+Shift+R cycles through automatic, all-source, and all-rendered views. Ctrl+Shift+E exports the full note as a TeX file or PDF, including course data, headings, lists, typed blocks, figure captions, and images.",
            code: "Ctrl+Shift+R   change view\nCtrl+Shift+E   export",
            keywords: "source raw rendered automatic view toggle export tex pdf"
        },
        {
            title: "Definitions, theorems, and proofs",
            body: "Ctrl+period opens the row menu. Choose normal, heading, subheading, bullet, numbered, definition, theorem, proof, example, remark, exercise, or solution. Enter continues lists and typed blocks. Right-click a row to change its type, move it, copy it, or delete it.",
            code: "Ctrl+.        choose row type\nAlt+Up/Down   move row\nCtrl+D        copy row",
            keywords: "definition theorem proof example row type math text automatic override catch up marker lecture"
        },
        {
            title: "Lecture and problem-solving notes",
            body: "Press Ctrl+N to start writing, then use Ctrl+Alt+L to choose Lecture or Problem-solving. Lecture notes have a lecture name; problem-solving notes have a problem set or topic. Both can have a title, course or subject, and date. Ctrl+Alt+L edits the note details later. Ctrl+Alt+F searches every FoldTeX note in the current note's folder.",
            code: "Ctrl+N       start writing\nCtrl+Alt+L   edit note details\nCtrl+Alt+F   search course notes",
            keywords: "course subject lecture problem solving problem set topic date details search folder notes university"
        },
        {
            title: "Notes library",
            body: "Press Ctrl+O to browse notes inside FoldTeX. Search titles, courses, dates, content, or file names, then sort by recent update, lecture date, title, or course. FoldTeX remembers folders when you save or open notes. Use Add folder to include an older notes folder, or Browse files for the normal file picker.",
            code: "Ctrl+O   open notes library",
            keywords: "notes library old recent sort search folder open browse archive anteckningar bibliotek sortera gamla"
        },
        {
            title: "Images",
            body: "Save the note first, then press Ctrl+Shift+V to paste an image from the clipboard. FoldTeX copies images into an asset folder beside the note. Right-click a figure to edit its caption or replace its file. Captions and figures stay in TeX and PDF exports.",
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
            title: "Explicit text line breaks",
            body: "Two backslashes in prose display as a line break and remain unchanged when copied. Export preserves their effect: at paragraph ends it uses explicit spacing, and next to display math it avoids redundant line breaks. A following source newline does not double the break. Move the cursor onto the command, or switch to source view, to edit it. Blank lines can separate paragraphs in LaTeX; their visible spacing depends on the receiving document's settings.",
            code: "First line\\\\\nSecond line",
            keywords: "newline line break backslash text prose copy export radbrytning"
        },
        {
            title: "Aligned equations",
            body: "Shift+Enter starts a new line in both source and rendering without leaving the block. It works in text and math, including inside text braces. For math, FoldTeX adds a left-aligned aligned environment when needed. In an existing equation or matrix environment, it inserts two backslashes and a source newline. Enter inserts a source newline; Shift+Enter also creates a visible break.",
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
            keywords: "limit derivative differential partial calculus approaches gränsvärde derivata derivera"
        },
        {
            title: "Functions, domains, and composition",
            body: "Colon, to, and a pair of sets state a function's domain and codomain. Circ composes two functions. Use a vertical bar after a function when you need to show a restricted domain.",
            code: "f \\colon A \\to B\n(f \\circ g)(x) = f(g(x))\nf|_A",
            keywords: "function domain codomain range mapping composition restriction funktion definitionsmängd målmängd värdemängd sammansättning"
        },
        {
            title: "Antiderivatives and the integral result",
            body: "An integral without bounds is an antiderivative plus a constant. For a definite integral, square brackets with lower and upper indices show where to evaluate the antiderivative.",
            code: "\\int f(x)\\,dx = F(x) + C\n\\int_a^b f(x)\\,dx = \\left[F(x)\\right]_a^b = F(b)-F(a)",
            keywords: "integral antiderivative primitive definite bounds evaluate fundamental theorem primitiv funktion bestämd obestämd insättning"
        },
        {
            title: "Taylor polynomials",
            body: "The kth derivative at a supplies the coefficient of the kth power. The factorial belongs in the denominator.",
            code: "T_n(x)=\\sum_{k=0}^{n} \\frac{f^{(k)}(a)}{k!}(x-a)^k",
            keywords: "taylor maclaurin polynomial expansion derivative factorial approximation taylorpolynom utveckling approximation"
        },
        {
            title: "Vectors and accents",
            body: "An accent applies to the group inside its braces. Use vec for an arrow, hat for a hat, and overline for a bar over longer content.",
            code: "\\vec{v}\n\\hat{x}\n\\bar{x}\n\\overline{AB}\n\\dot{x}\n\\ddot{x}",
            keywords: "vector accent arrow hat bar overline dot"
        },
        {
            title: "Sets and logic",
            body: "Use in and notin for membership. Subseteq and nsubseteq mean subset and not a subset; supseteq and nsupseteq give the matching superset forms. Subset and subsetneq are common strict forms. Cup is union, cap is intersection, setminus is difference, complement marks everything outside a set, and triangle is symmetric difference. Vertical bars give cardinality, times gives a Cartesian product, and bigcup or bigcap joins an indexed family. Emptyset is the empty set. Mathbb gives the standard number sets. Forall and exists write quantified claims; implies and iff connect them.",
            code: "x \\in A, \\quad y \\notin A\nA \\subseteq B, \\quad A \\nsubseteq B\nA \\subsetneq B, \\quad A \\supseteq B, \\quad A \\nsupseteq B\nA \\cup B, \\quad A \\cap B\nA \\setminus B, \\quad A^{\\complement}, \\quad A \\triangle B\n\\left|A\\right|, \\quad A \\times B\n\\bigcup_{i=1}^{n} A_i, \\quad \\bigcap_{i=1}^{n} A_i\n\\emptyset\n\\mathbb{N}, \\mathbb{Z}, \\mathbb{Q}, \\mathbb{R}, \\mathbb{C}\n\\forall x \\in A \\; \\exists y \\in B\nP \\implies Q, \\quad P \\iff Q",
            keywords: "set membership subset not subset superset not superset union intersection difference complement symmetric empty natural integer rational real complex numbers logic forall exists implies iff mängd delmängd inte delmängd övermängd union snitt komplement"
        },
        {
            title: "Set-builder notation and intervals",
            body: "A vertical bar means 'such that' in set-builder notation. Parentheses exclude an endpoint and square brackets include it. Half-open intervals use one of each. Swedish books may write the open interval as ]a,b[; both forms mean the same thing.",
            code: "\\left\\{ x \\in \\mathbb{R} \\middle| x > 0 \\right\\}\n\\left(a,b\\right)\n\\left[a,b\\right]\n\\left[a,b\\right)\n\\left(a,b\\right]",
            keywords: "set builder such that condition interval open closed half endpoint mängd villkor intervall öppet slutet"
        },
        {
            title: "Sequences, series, sup, and inf",
            body: "A sequence uses an indexed symbol. Add a sum for a series. Sup and inf name the least upper bound and greatest lower bound when a maximum or minimum need not exist.",
            code: "(a_n)_{n=1}^{\\infty}\n\\lim_{n \\to \\infty} a_n = L\n\\sum_{n=1}^{\\infty} a_n\n\\sup A, \\quad \\inf A",
            keywords: "sequence series convergence limit supremum infimum upper lower bound följd serie konvergens gränsvärde"
        },
        {
            title: "Continuity and epsilon-delta",
            body: "Continuity at a means that the limit equals the function value. A minus or plus above the target gives a one-sided limit. Epsilon and delta write the precise limit condition.",
            code: "\\lim_{x \\to a} f(x) = f(a)\n\\lim_{x \\to a^-} f(x)\n\\lim_{x \\to a^+} f(x)\n\\forall \\varepsilon > 0\\; \\exists \\delta > 0 \\colon |x-a|<\\delta \\implies |f(x)-f(a)|<\\varepsilon",
            keywords: "continuity one sided limit epsilon delta definition continuous kontinuitet gränsvärde höger vänster"
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
            body: "Check that every opening brace has a closing brace. Use $…$ around formulas in prose. Inside a formula, put prose inside text braces. Incomplete expressions remain editable.",
            code: "\\text{correct words}\n\\frac{complete}{groups}\n\\sqrt[3]{complete root}",
            keywords: "error missing inserted brace linebreak wrong debug"
        },
        {
            title: "Figures",
            body: "Press Ctrl+Alt+I to draw beside your notes, even before the note has a file. The figure tool has a pen, eraser, lines, arrows, boxes, and text. Ctrl+Z removes the last stroke, shape, or text. Save inserts the figure and focuses a text row below it. The first note save moves temporary figures into the note's asset folder.",
            code: "Ctrl+Alt+I   draw a figure\nCtrl+Z       undo the last drawing action",
            keywords: "figure drawing draw pen eraser line arrow box text diagram undo stroke ctrl z"
        },
        {
            title: "Lecture slides",
            body: "Press Ctrl+Alt+P and choose a PDF. FoldTeX copies it beside the saved note, then shows the slides next to the note when the window is wide. Link the active row to the current slide with the arrow button.",
            code: "Ctrl+Alt+P",
            keywords: "slide slides pdf lecture split link page copy"
        }
    ]

    function reloadSnippets() {
        customSnippetEntries = backendApi ? backendApi.customSnippets() : []
    }

    function snippetListSection() {
        var triggers = builtInSnippetTriggers.slice().sort()
        var builtIns = []
        var builtInLines = []
        for (var builtInIndex = 0; builtInIndex < triggers.length; ++builtInIndex) {
            var trigger = triggers[builtInIndex]
            var template = builtInSnippetTemplates[trigger] || ""
            builtIns.push({ trigger: trigger, template: template })
            builtInLines.push(trigger + " Tab | " + template)
        }
        var customLines = []
        for (var i = 0; i < customSnippetEntries.length; ++i) {
            var entry = customSnippetEntries[i]
            var line = entry.trigger + " Tab | " + entry.template
            if ((entry.aliases || []).length)
                line += " (aliases: " + entry.aliases.join(", ") + ")"
            line += entry.allCourses ? " — all courses"
                                     : " — " + (entry.courses || []).join(", ")
            if (entry.enabled === false) line += " — disabled"
            customLines.push(line)
        }
        return {
            title: "All Tab snippets",
            body: "Type a trigger and press Tab. Each row shows the exact expansion; text inside «chevrons» is a field selected by Tab. Click an expansion to copy it. Custom aliases, course limits, and disabled state appear below each custom row.",
            isSnippetList: true,
            builtIns: builtIns,
            customs: customSnippetEntries,
            code: "Built-in\n" + builtInLines.join("\n")
                  + "\n\nCustom\n"
                  + (customLines.length ? customLines.join("\n")
                                        : "No custom snippets yet."),
            keywords: "all complete full tab snippet trigger custom alias "
                      + builtInLines.join(" ") + " " + customLines.join(" ")
        }
    }

    function allSections() {
        return sections.concat([snippetListSection()])
    }

    function snippetTriggerColumnWidth(availableWidth) {
        return Math.min(availableWidth * 0.46,
                        Math.max(160, writingSize * 10.2))
    }

    function matchingSections(query) {
        var needle = query.trim().toLowerCase()
        var availableSections = allSections()
        if (!needle.length) return availableSections
        var result = []
        for (var i = 0; i < availableSections.length; ++i) {
            var section = availableSections[i]
            var text = (section.title + " " + section.body + " " + section.code
                        + " " + section.keywords).toLowerCase()
            if (text.indexOf(needle) >= 0) result.push(section)
        }
        return result
    }

    function copySnippet(text) {
        if (backendApi) backendApi.setClipboardText(text)
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
                        id: sectionDelegate
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
                            height: visible ? snippet.implicitHeight + 8 : 0
                            visible: sectionDelegate.modelData.isSnippetList !== true

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

                        Column {
                            id: snippetListView
                            objectName: "guideSnippetListView"
                            width: parent.width - parent.leftPadding - parent.rightPadding
                            height: visible ? implicitHeight : 0
                            visible: sectionDelegate.modelData.isSnippetList === true
                            spacing: 14
                            property real triggerColumnWidth: guide.snippetTriggerColumnWidth(width)
                            property real tabColumnWidth: Math.max(34, guide.writingSize * 2.5)
                            property real triggerLabelWidth: triggerColumnWidth - tabColumnWidth - 10

                            Text {
                                width: parent.width
                                text: "Built-in (" + (sectionDelegate.modelData.builtIns || []).length + ")"
                                color: guide.foregroundColor
                                font.family: guide.writingFont
                                font.pixelSize: Math.max(14, guide.writingSize - 1)
                                font.weight: Font.DemiBold
                            }

                            Column {
                                id: builtInSnippetList
                                objectName: "guideSnippetList"
                                width: parent.width
                                spacing: 7

                                Repeater {
                                    model: sectionDelegate.modelData.builtIns || []

                                    delegate: RowLayout {
                                        id: builtInSnippetRow
                                        required property var modelData
                                        width: builtInSnippetList.width
                                        spacing: 10

                                        Text {
                                            Layout.preferredWidth: snippetListView.triggerLabelWidth
                                            Layout.alignment: Qt.AlignTop
                                            text: "•  " + builtInSnippetRow.modelData.trigger
                                            color: guide.foregroundColor
                                            font.family: guide.writingFont
                                            font.pixelSize: Math.max(14, guide.writingSize - 1)
                                            elide: Text.ElideRight
                                            wrapMode: Text.NoWrap
                                        }

                                        Text {
                                            Layout.preferredWidth: snippetListView.tabColumnWidth
                                            Layout.alignment: Qt.AlignTop
                                            text: "Tab"
                                            color: guide.foregroundColor
                                            font.family: guide.writingFont
                                            font.pixelSize: Math.max(14, guide.writingSize - 1)
                                            wrapMode: Text.NoWrap
                                        }

                                        Text {
                                            Layout.alignment: Qt.AlignTop
                                            text: "│"
                                            color: guide.mutedColor
                                            font.family: guide.writingFont
                                            font.pixelSize: Math.max(14, guide.writingSize - 1)
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            Layout.alignment: Qt.AlignTop
                                            text: builtInSnippetRow.modelData.template
                                            color: builtInHover.hovered
                                                   ? guide.foregroundColor : guide.accentColor
                                            font.family: guide.writingFont
                                            font.pixelSize: Math.max(14, guide.writingSize - 1)
                                            wrapMode: Text.WrapAnywhere

                                            HoverHandler { id: builtInHover }
                                            TapHandler {
                                                onTapped: guide.copySnippet(
                                                    builtInSnippetRow.modelData.template)
                                            }
                                        }
                                    }
                                }
                            }

                            Text {
                                width: parent.width
                                topPadding: 6
                                text: "Custom (" + (sectionDelegate.modelData.customs || []).length + ")"
                                color: guide.foregroundColor
                                font.family: guide.writingFont
                                font.pixelSize: Math.max(14, guide.writingSize - 1)
                                font.weight: Font.DemiBold
                            }

                            Text {
                                visible: !(sectionDelegate.modelData.customs || []).length
                                width: parent.width
                                text: "No custom snippets yet."
                                color: guide.mutedColor
                                font.family: guide.writingFont
                                font.pixelSize: Math.max(13, guide.writingSize - 2)
                            }

                            Repeater {
                                model: sectionDelegate.modelData.customs || []

                                delegate: Column {
                                    id: customSnippetRow
                                    required property var modelData
                                    width: snippetListView.width
                                    spacing: 4

                                    RowLayout {
                                        width: parent.width
                                        spacing: 10

                                        Text {
                                            Layout.preferredWidth: snippetListView.triggerLabelWidth
                                            Layout.alignment: Qt.AlignTop
                                            text: "•  " + customSnippetRow.modelData.trigger
                                            color: customSnippetRow.modelData.enabled === false
                                                   ? guide.mutedColor : guide.foregroundColor
                                            font.family: guide.writingFont
                                            font.pixelSize: Math.max(14, guide.writingSize - 1)
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                            wrapMode: Text.NoWrap
                                        }

                                        Text {
                                            Layout.preferredWidth: snippetListView.tabColumnWidth
                                            Layout.alignment: Qt.AlignTop
                                            text: "Tab"
                                            color: customSnippetRow.modelData.enabled === false
                                                   ? guide.mutedColor : guide.foregroundColor
                                            font.family: guide.writingFont
                                            font.pixelSize: Math.max(14, guide.writingSize - 1)
                                            wrapMode: Text.NoWrap
                                        }

                                        Text {
                                            Layout.alignment: Qt.AlignTop
                                            text: "│"
                                            color: guide.mutedColor
                                            font.family: guide.writingFont
                                            font.pixelSize: Math.max(14, guide.writingSize - 1)
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            Layout.alignment: Qt.AlignTop
                                            text: customSnippetRow.modelData.template
                                            color: customSnippetRow.modelData.enabled === false
                                                   ? guide.mutedColor
                                                   : (customHover.hovered
                                                      ? guide.foregroundColor : guide.accentColor)
                                            font.family: guide.writingFont
                                            font.pixelSize: Math.max(14, guide.writingSize - 1)
                                            wrapMode: Text.WrapAnywhere

                                            HoverHandler { id: customHover }
                                            TapHandler {
                                                onTapped: guide.copySnippet(
                                                    customSnippetRow.modelData.template)
                                            }
                                        }
                                    }

                                    Text {
                                        width: parent.width
                                        leftPadding: snippetListView.triggerColumnWidth + 22
                                        text: ((customSnippetRow.modelData.name || "")
                                                   !== customSnippetRow.modelData.trigger
                                               ? customSnippetRow.modelData.name + " · " : "")
                                              + ((customSnippetRow.modelData.aliases || []).length
                                               ? "Aliases: " + customSnippetRow.modelData.aliases.join(", ") + " · "
                                               : "")
                                              + (customSnippetRow.modelData.allCourses
                                                 ? "All courses"
                                                 : "Courses: "
                                                   + (customSnippetRow.modelData.courses || []).join(", "))
                                              + (customSnippetRow.modelData.enabled === false
                                                 ? " · Disabled" : "")
                                        color: guide.mutedColor
                                        font.family: guide.writingFont
                                        font.pixelSize: Math.max(12, guide.writingSize - 3)
                                        wrapMode: Text.Wrap
                                    }
                                }
                            }
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
        if (visible) {
            reloadSnippets()
            Qt.callLater(function() { guideSearch.forceActiveFocus() })
        }
    }
}
