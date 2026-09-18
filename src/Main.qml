import QtQuick
import FoldTeX 1.0
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs as Dialogs
import QtQuick.Layouts
import QtQuick.Pdf
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

    Material.theme: luminance(win.color) < 0.5 ? Material.Dark : Material.Light
    Material.accent: backend.themeAccent

    property bool syncingDocument: false
    property bool editingDocument: false
    property int activeIndex: 0
    property string documentPath: ""
    property string startupPath: typeof startupDocument === "undefined" ? "" : startupDocument
    property string documentFileRevision: ""
    property bool externalConflictShown: false
    property string recoveryId: ""
    property bool modified: false
    property bool loading: false
    property string documentTitle: "Untitled notes"
    property string courseName: ""
    property string noteKind: "lecture"
    property string lectureName: ""
    property string problemSetName: ""
    property string lectureDate: ""
    property string sourcePdf: ""
    property string spellLanguage: "sv"
    property bool spellcheckEnabled: true
    property var spellingIgnored: []
    property string newDocumentCoursePreset: ""
    property var noteLibraryLastNote: ({})
    property bool pdfOpen: false
    property string saveStatus: "Saved"
    property int displayMode: 0
    onDisplayModeChanged: {
        if (displayMode !== 0)
            Qt.callLater(function() { documentEditor.forceActiveFocus() })
    }
    property int selectionAnchor: -1
    property int selectionEnd: -1
    property int searchLine: -1
    property int searchPosition: -1
    property var undoHistory: []
    property var redoHistory: []
    property string lastHistoryState: ""
    property string savedHistoryState: ""
    property bool restoringHistory: false
    property var snippetStops: []
    property int snippetStopIndex: -1
    property int snippetStopRow: -1
    property int snippetExitPosition: -1
    property string snippetTrackedText: ""
    property bool snippetSelectionChange: false
    readonly property int pendingRenderCount: documentEditor.pendingRenderCount
    readonly property bool hasLineSelection: selectionAnchor >= 0 && selectionEnd >= 0
    readonly property int firstSelectedLine: Math.min(selectionAnchor, selectionEnd)
    readonly property int lastSelectedLine: Math.max(selectionAnchor, selectionEnd)
    readonly property bool widePdfSplit: pdfOpen && width >= 900
    readonly property real noteWidth: notePane.width > 0 ? notePane.width : width
    readonly property real marginScale: Math.max(0, Math.min(1, (noteWidth - minimumWidth) / (980 - minimumWidth)))
    readonly property int pageMargin: 16 + (backend.editorSideMargin - 16) * marginScale
    readonly property int pageWidth: Math.max(180, noteWidth - 2 * pageMargin)
    readonly property color textColor: backend.themeForeground
    readonly property color mutedColor: Qt.rgba(textColor.r, textColor.g, textColor.b, 0.48)
    readonly property string editorFont: backend.editorFontFamily
    readonly property int editorSize: backend.editorFontSize
    readonly property var builtInSnippetTriggers: Object.keys(builtInSnippetTemplates())
    readonly property var latexCommands: [
        { name: "Bold text", insertText: "\\textbf{|}", example: "\\textbf{bold}", keywords: "bold text fetstil textbf" },
        { name: "Italic text", insertText: "\\textit{|}", example: "\\textit{italic}", keywords: "italic text kursiv textit" },
        { name: "Emphasized text", insertText: "\\emph{|}", example: "\\emph{emphasis}", keywords: "emphasis text betoning emph" },
        { name: "Monospace text", insertText: "\\texttt{|}", example: "\\texttt{code}", keywords: "monospace code text kod texttt" },
        { name: "Square root", insertText: "\\sqrt{|}", example: "\\sqrt{x}", keywords: "root square radical sqrt rot kvadratrot" },
        { name: "Nth root", insertText: "\\sqrt[n]{|}", example: "\\sqrt[n]{x}", keywords: "root nth index radical rot n-te n:te" },
        { name: "Fraction", insertText: "\\frac{|}{}", example: "\\frac{a}{b}", keywords: "fraction divide division over ratio bråk kvot division" },
        { name: "Power", insertText: "^{|}", example: "x^{2}", keywords: "power exponent superscript squared cubed" },
        { name: "Subscript", insertText: "_{|}", example: "x_{1}", keywords: "subscript index below" },
        { name: "Sum", insertText: "\\sum_{i=1}^{n} |", example: "\\sum_{i=1}^{n} x_i", keywords: "sum sigma total series summa serie" },
        { name: "Product", insertText: "\\prod_{i=1}^{n} |", example: "\\prod_{i=1}^{n} x_i", keywords: "product multiply pi series produkt serie" },
        { name: "Integral", insertText: "\\int_{a}^{b} |\\,dx", example: "\\int_{a}^{b} f(x)\\,dx", keywords: "integral integrate area primitiv funktion integrera" },
        { name: "Limit", insertText: "\\lim_{x \\to 0} |", example: "\\lim_{x \\to 0} f(x)", keywords: "limit approaches tends to gränsvärde gransvarde" },
        { name: "Derivative", insertText: "\\frac{d|}{dx}", example: "\\frac{dy}{dx}", keywords: "derivative differentiate dy dx derivata derivera" },
        { name: "Partial derivative", insertText: "\\frac{\\partial |}{\\partial x}", example: "\\frac{\\partial f}{\\partial x}", keywords: "partial derivative differentiate" },
        { name: "Infinity", insertText: "\\infty", example: "\\infty", keywords: "infinity infinite forever oändlighet oandlighet" },
        { name: "Plus or minus", insertText: "\\pm", example: "x \\pm y", keywords: "plus minus both" },
        { name: "Not equal", insertText: "\\neq", example: "x \\neq y", keywords: "not equal inequality" },
        { name: "Approximately equal", insertText: "\\approx", example: "x \\approx y", keywords: "approximately about close equal" },
        { name: "Less than or equal", insertText: "\\leq", example: "x \\leq y", keywords: "less smaller equal inequality" },
        { name: "Greater than or equal", insertText: "\\geq", example: "x \\geq y", keywords: "greater bigger equal inequality" },
        { name: "Arrow", insertText: "\\to", example: "x \\to y", keywords: "arrow to approaches tends" },
        { name: "Vector", insertText: "\\vec{|}", example: "\\vec{v}", keywords: "vector arrow" },
        { name: "Absolute value", insertText: "\\left\\lvert | \\right\\rvert", example: "\\left|x\\right|", keywords: "absolute value magnitude bars modulus absolutbelopp belopp" },
        { name: "Parentheses", insertText: "\\left( | \\right)", example: "\\left( x \\right)", keywords: "parentheses brackets group scalable" },
        { name: "Matrix", insertText: "\\begin{bmatrix} | & b \\\\ c & d \\end{bmatrix}", example: "\\begin{bmatrix} a & b \\\\ c & d \\end{bmatrix}", keywords: "matrix grid array brackets" },
        { name: "Cases", insertText: "\\begin{cases} |, & x > 0 \\\\ 0, & x \\leq 0 \\end{cases}", example: "\\begin{cases} x, & x > 0 \\\\ 0, & x \\leq 0 \\end{cases}", keywords: "cases piecewise conditions" },
        { name: "Set builder", insertText: "\\left\\{ x \\in A \\middle\\vert | \\right\\}", example: "\\left\\{ x \\in \\mathbb{R} \\middle| x > 0 \\right\\}", keywords: "set builder condition mängd mängdbyggare villkor" },
        { name: "Element of", insertText: "\\in", example: "x \\in A", keywords: "element member belongs membership in tillhör element i mängd" },
        { name: "Not an element of", insertText: "\\notin", example: "x \\notin A", keywords: "not element member belongs membership inte tillhör ej element mängd" },
        { name: "Subset or equal", insertText: "\\subseteq", example: "A \\subseteq B", keywords: "subset included contained delmängd delmangd mängd" },
        { name: "Not a subset", insertText: "\\nsubseteq", example: "A \\nsubseteq B", keywords: "not subset not included inte delmängd ej delmängd" },
        { name: "Subset symbol", insertText: "\\subset", example: "A \\subset B", keywords: "subset strict proper delmängd äkta symbol" },
        { name: "Proper subset", insertText: "\\subsetneq", example: "A \\subsetneq B", keywords: "proper strict subset äkta delmängd akta delmangd" },
        { name: "Superset or equal", insertText: "\\supseteq", example: "A \\supseteq B", keywords: "superset contains övermängd overmangd" },
        { name: "Not a superset", insertText: "\\nsupseteq", example: "A \\nsupseteq B", keywords: "not superset does not contain inte övermängd ej overmangd" },
        { name: "Union", insertText: "\\cup", example: "A \\cup B", keywords: "union combine sets förening forening mängd" },
        { name: "Intersection", insertText: "\\cap", example: "A \\cap B", keywords: "intersection common sets snitt skärning skarning mängd" },
        { name: "Set difference", insertText: "\\setminus", example: "A \\setminus B", keywords: "set difference minus complement differens mängddifferens mängd" },
        { name: "Set complement", insertText: "^{\\complement}", example: "A^{\\complement}", keywords: "set complement outside komplement komplementmängd mängd" },
        { name: "Symmetric difference", insertText: "\\triangle", example: "A \\triangle B", keywords: "symmetric difference either set symmetrisk differens mängd" },
        { name: "Cardinality", insertText: "\\left\\lvert | \\right\\rvert", example: "\\left|A\\right|", keywords: "cardinality size number elements kardinalitet antal element mängd" },
        { name: "Cartesian product", insertText: "\\times", example: "A \\times B", keywords: "cartesian product ordered pairs kartesisk produkt mängd" },
        { name: "Indexed union", insertText: "\\bigcup_{i=1}^{n} |", example: "\\bigcup_{i=1}^{n} A_i", keywords: "indexed big union family stor union indexerad förening mängdfamilj" },
        { name: "Indexed intersection", insertText: "\\bigcap_{i=1}^{n} |", example: "\\bigcap_{i=1}^{n} A_i", keywords: "indexed big intersection family stort snitt indexerad skärning mängdfamilj" },
        { name: "Empty set", insertText: "\\emptyset", example: "A = \\emptyset", keywords: "empty set null tom mängd tomma mängden" },
        { name: "Power set", insertText: "\\mathcal{P}(|)", example: "\\mathcal{P}(A)", keywords: "power set subsets potensmängd potensmangd" },
        { name: "Natural numbers", insertText: "\\mathbb{N}", example: "n \\in \\mathbb{N}", keywords: "natural numbers naturals heltal naturliga talmängd N" },
        { name: "Integers", insertText: "\\mathbb{Z}", example: "k \\in \\mathbb{Z}", keywords: "integers whole numbers heltal talmängd Z" },
        { name: "Rational numbers", insertText: "\\mathbb{Q}", example: "q \\in \\mathbb{Q}", keywords: "rational numbers rationella tal bråk talmängd Q" },
        { name: "Real numbers", insertText: "\\mathbb{R}", example: "x \\in \\mathbb{R}", keywords: "real numbers reella talmängd R analysis" },
        { name: "Complex numbers", insertText: "\\mathbb{C}", example: "z \\in \\mathbb{C}", keywords: "complex numbers komplexa talmängd C" },
        { name: "For all", insertText: "\\forall", example: "\\forall x \\in A", keywords: "for all every universal alla för alla kvantifikator" },
        { name: "There exists", insertText: "\\exists", example: "\\exists x \\in A", keywords: "exists there is existential finns existerar kvantifikator" },
        { name: "Does not exist", insertText: "\\nexists", example: "\\nexists x \\in A", keywords: "not exists does not exist finns inte existerar ej" },
        { name: "Implies", insertText: "\\implies", example: "P \\implies Q", keywords: "implies therefore medför implikation" },
        { name: "If and only if", insertText: "\\iff", example: "P \\iff Q", keywords: "equivalent iff if only if ekvivalent om och endast om" },
        { name: "Closed interval", insertText: "\\left[ |, b \\right]", example: "\\left[a,b\\right]", keywords: "closed interval endpoint slutet intervall" },
        { name: "Open interval", insertText: "\\left( |, b \\right)", example: "\\left(a,b\\right)", keywords: "open interval endpoint öppet intervall" },
        { name: "Half-open interval", insertText: "\\left[ |, b \\right)", example: "\\left[a,b\\right)", keywords: "half open interval endpoint halvöppet intervall" },
        { name: "Function mapping", insertText: "f \\colon | \\to B", example: "f \\colon A \\to B", keywords: "function mapping domain codomain funktion avbildning definitionsmängd värdemängd" },
        { name: "Sequence", insertText: "(a_n)_{n=1}^{\\infty}", example: "(a_n)_{n=1}^{\\infty}", keywords: "sequence indexed följd talföljd analys analysis" },
        { name: "Infinite series", insertText: "\\sum_{n=1}^{\\infty} |", example: "\\sum_{n=1}^{\\infty} a_n", keywords: "infinite series convergence serie oändlig konvergens" },
        { name: "Left-hand limit", insertText: "\\lim_{x \\to a^-} |", example: "\\lim_{x \\to a^-} f(x)", keywords: "left hand one sided limit vänstergränsvärde gränsvärde" },
        { name: "Right-hand limit", insertText: "\\lim_{x \\to a^+} |", example: "\\lim_{x \\to a^+} f(x)", keywords: "right hand one sided limit högergränsvärde gränsvärde" },
        { name: "Continuity at a point", insertText: "\\lim_{x \\to a} f(x) = f(a)", example: "\\lim_{x \\to a} f(x) = f(a)", keywords: "continuous continuity point kontinuitet kontinuerlig analys" },
        { name: "Supremum", insertText: "\\sup |", example: "\\sup A", keywords: "supremum least upper bound minsta övre gräns" },
        { name: "Infimum", insertText: "\\inf |", example: "\\inf A", keywords: "infimum greatest lower bound största undre gräns" },
        { name: "Norm", insertText: "\\left\\lVert | \\right\\rVert", example: "\\left\\lVert x \\right\\rVert", keywords: "norm length magnitude norm belopp" },
        { name: "Evaluation bar", insertText: "\\left[ | \\right]_{a}^{b}", example: "\\left[F(x)\\right]_{a}^{b}", keywords: "evaluate antiderivative integral bounds insättning primitiv funktion" },
        { name: "Epsilon-delta statement", insertText: "\\forall \\varepsilon > 0\\; \\exists \\delta > 0 \\colon |", example: "\\forall \\varepsilon > 0\\; \\exists \\delta > 0 \\colon |x-a|<\\delta \\implies |f(x)-f(a)|<\\varepsilon", keywords: "epsilon delta definition limit continuity gränsvärde kontinuitet" },
        { name: "Taylor polynomial", insertText: "\\sum_{k=0}^{n} \\frac{f^{(k)}(a)}{k!} (x-a)^k", example: "T_n(x)=\\sum_{k=0}^{n} \\frac{f^{(k)}(a)}{k!}(x-a)^k", keywords: "taylor polynomial expansion serie polynom utveckling" },
        { name: "Sine", insertText: "\\sin(|)", example: "\\sin(x)", keywords: "sine sin trigonometry sinus trigonometrisk" },
        { name: "Cosine", insertText: "\\cos(|)", example: "\\cos(x)", keywords: "cosine cos trigonometry cosinus trigonometrisk" },
        { name: "Tangent", insertText: "\\tan(|)", example: "\\tan(x)", keywords: "tangent tan trigonometry trigonometrisk" },
        { name: "Natural logarithm", insertText: "\\ln(|)", example: "\\ln(x)", keywords: "natural logarithm ln log naturlig logaritm" },
        { name: "Exponential function", insertText: "\\exp(|)", example: "\\exp(x)", keywords: "exponential exp e power exponentialfunktion" },
        { name: "Function composition", insertText: "\\circ", example: "(f \\circ g)(x)", keywords: "function composition composed sammansättning funktion" },
        { name: "Indefinite integral", insertText: "\\int |\\,dx", example: "\\int f(x)\\,dx", keywords: "indefinite integral antiderivative obestämd integral primitiv funktion" },
        { name: "Second derivative", insertText: "\\frac{d^2 |}{dx^2}", example: "\\frac{d^2 y}{dx^2}", keywords: "second derivative differentiate andra derivata" },
        { name: "Maximum", insertText: "\\max |", example: "\\max_{x \\in A} f(x)", keywords: "maximum largest max största värde" },
        { name: "Minimum", insertText: "\\min |", example: "\\min_{x \\in A} f(x)", keywords: "minimum smallest min minsta värde" },
        { name: "Greek alpha", insertText: "\\alpha", example: "\\alpha", keywords: "greek alpha letter" },
        { name: "Greek beta", insertText: "\\beta", example: "\\beta", keywords: "greek beta letter" },
        { name: "Greek theta", insertText: "\\theta", example: "\\theta", keywords: "greek theta angle letter" },
        { name: "Greek pi", insertText: "\\pi", example: "\\pi", keywords: "greek pi circle letter" },
        { name: "Greek sigma", insertText: "\\sigma", example: "\\sigma", keywords: "greek sigma standard deviation letter" },
        { name: "Logical and", insertText: "\\land", example: "P \\land Q", keywords: "logic conjunction and och konjunktion diskret" },
        { name: "Logical or", insertText: "\\lor", example: "P \\lor Q", keywords: "logic disjunction or eller disjunktion diskret" },
        { name: "Logical not", insertText: "\\neg", example: "\\neg P", keywords: "logic negation not inte negation diskret" },
        { name: "Divides", insertText: "\\mid", example: "a \\mid b", keywords: "divides divisibility delar delbarhet diskret" },
        { name: "Does not divide", insertText: "\\nmid", example: "a \\nmid b", keywords: "not divides delar inte delbarhet diskret" },
        { name: "Congruent modulo", insertText: "\\equiv | \\pmod{n}", example: "a \\equiv b \\pmod n", keywords: "congruent modulo modular arithmetic kongruent modulär aritmetik diskret" },
        { name: "Binomial coefficient", insertText: "\\binom{n}{|}", example: "\\binom{n}{k}", keywords: "choose combination binomial kombinatorik binomialkoefficient diskret" },
        { name: "Recurrence relation", insertText: "a_{n+1}=|", example: "a_{n+1}=2a_n+1", keywords: "recurrence recursive rekursion rekursionsformel diskret" },
        { name: "Graph degree", insertText: "\\deg(|)", example: "\\deg(v)", keywords: "graph vertex degree grad nod graf diskret" },
        { name: "Determinant", insertText: "\\det(|)", example: "\\det(A)", keywords: "determinant matrix linjär algebra" },
        { name: "Matrix transpose", insertText: "^{\\mathsf T}", example: "A^{\\mathsf T}", keywords: "transpose matrix transponat linjär algebra" },
        { name: "Matrix inverse", insertText: "^{-1}", example: "A^{-1}", keywords: "inverse matrix invers linjär algebra" },
        { name: "Augmented matrix", insertText: "\\left[\\begin{array}{cc|c} | & b & e \\\\ c & d & f \\end{array}\\right]", example: "\\left[\\begin{array}{cc|c}a&b&e\\\\c&d&f\\end{array}\\right]", keywords: "augmented matrix system utökad matris ekvationssystem linjär algebra" },
        { name: "Span", insertText: "\\operatorname{span}\\{|\\}", example: "\\operatorname{span}\\{v_1,v_2\\}", keywords: "span linear hull linjärt hölje linjär algebra" },
        { name: "Dimension", insertText: "\\dim(|)", example: "\\dim(V)", keywords: "dimension vector space vektorrum linjär algebra" },
        { name: "Rank", insertText: "\\operatorname{rank}(|)", example: "\\operatorname{rank}(A)", keywords: "rank matrix rang linjär algebra" },
        { name: "Kernel", insertText: "\\ker(|)", example: "\\ker(T)", keywords: "kernel null space nollrum kärna linjär algebra" },
        { name: "Image", insertText: "\\operatorname{im}(|)", example: "\\operatorname{im}(T)", keywords: "image range värderum bildrum linjär algebra" },
        { name: "Inner product", insertText: "\\langle |, \\rangle", example: "\\langle u,v\\rangle", keywords: "inner product dot scalar skalärprodukt linjär algebra" },
        { name: "Projection", insertText: "\\operatorname{proj}_{|}", example: "\\operatorname{proj}_{u}(v)", keywords: "projection orthogonal projektion ortogonal linjär algebra" },
        { name: "Eigenvalue equation", insertText: "A|=\\lambda |", example: "Av=\\lambda v", keywords: "eigenvalue eigenvector egenvärde egenvektor linjär algebra" },
        { name: "Gradient", insertText: "\\nabla |", example: "\\nabla f", keywords: "gradient multivariable flervariabel analys 2" },
        { name: "Directional derivative", insertText: "D_{|} f", example: "D_{u}f=\\nabla f\\cdot u", keywords: "directional derivative riktningsderivata analys 2" },
        { name: "Jacobian", insertText: "J_{|}", example: "J_f(x)", keywords: "jacobian derivative matrix jacobimatris analys 2" },
        { name: "Hessian", insertText: "H_{|}", example: "H_f(x)", keywords: "hessian second partial matrix hessianmatris analys 2" },
        { name: "Double integral", insertText: "\\iint_{|} \\,dA", example: "\\iint_D f(x,y)\\,dA", keywords: "double integral dubbelintegral analys 2" },
        { name: "Triple integral", insertText: "\\iiint_{|} \\,dV", example: "\\iiint_E f(x,y,z)\\,dV", keywords: "triple integral trippelintegral analys 2" },
        { name: "Line integral", insertText: "\\int_{|} \\,ds", example: "\\int_C f\\,ds", keywords: "line integral kurvintegral analys 2" },
        { name: "Surface integral", insertText: "\\iint_{|} \\,dS", example: "\\iint_S f\\,dS", keywords: "surface integral ytintegral analys 2" },
        { name: "Divergence", insertText: "\\nabla \\cdot |", example: "\\nabla\\cdot F", keywords: "divergence vector field divergens vektorfält analys 2" },
        { name: "Curl", insertText: "\\nabla \\times |", example: "\\nabla\\times F", keywords: "curl rotation vector field rotation vektorfält analys 2" },
        { name: "Laplacian", insertText: "\\Delta |", example: "\\Delta f", keywords: "laplacian laplace operator laplaceoperator analys 2" },
        { name: "Truth table", insertText: "\\begin{array}{c|c|c} P & Q & P \\land Q \\\\ \\hline T&T&T \\\\ T&F&F \\\\ F&T&F \\\\ F&F&F \\end{array}", example: "P, Q, P \\land Q", keywords: "truth table logic sanningsvärdestabell sanningsvardestabell diskret" },
        { name: "Equivalence class", insertText: "[|]_{R}", example: "[a]_{R}", keywords: "equivalence class relation ekvivalensklass relation diskret" },
        { name: "Greatest common divisor", insertText: "\\gcd(|,)", example: "\\gcd(a,b)", keywords: "gcd greatest common divisor största gemensamma delare sgd diskret" },
        { name: "Graph", insertText: "G=(|,)", example: "G=(V,E)", keywords: "graph vertices edges graf noder kanter diskret" },
        { name: "Adjacency matrix", insertText: "A_{|}", example: "A_{ij}=1", keywords: "adjacency matrix graph grannmatris graf diskret" },
        { name: "Factorial", insertText: "!", example: "n!", keywords: "factorial permutations fakultet kombinatorik diskret" },
        { name: "Column vector", insertText: "\\begin{bmatrix} | \\\\ b \\end{bmatrix}", example: "\\begin{bmatrix}a\\\\b\\end{bmatrix}", keywords: "column vector vektor kolonn linjär algebra" },
        { name: "Row swap", insertText: "R_{|} \\leftrightarrow R_{}", example: "R_1 \\leftrightarrow R_2", keywords: "row operation swap radoperation radbyte gauss linjär algebra" },
        { name: "Row replacement", insertText: "R_{|} \\leftarrow R_{} + cR_{}", example: "R_2 \\leftarrow R_2-3R_1", keywords: "row operation replacement radoperation gauss linjär algebra" },
        { name: "Linear system", insertText: "\\begin{cases} | \\\\  \\end{cases}", example: "\\begin{cases}x+y=1\\\\x-y=0\\end{cases}", keywords: "linear system equations ekvationssystem linjär algebra" },
        { name: "Characteristic polynomial", insertText: "\\det(|-\\lambda I)", example: "\\det(A-\\lambda I)=0", keywords: "characteristic polynomial eigenvalue karakteristiskt polynom egenvärde linjär algebra" },
        { name: "Diagonalisation", insertText: "|=PDP^{-1}", example: "A=PDP^{-1}", keywords: "diagonalization diagonalisation diagonalisering eigenvectors linjär algebra" },
        { name: "Orthogonal complement", insertText: "|^{\\perp}", example: "W^{\\perp}", keywords: "orthogonal complement ortogonalt komplement linjär algebra" },
        { name: "Change of variables", insertText: "\\left|\\det \\frac{\\partial(|)}{\\partial()}\\right|", example: "\\left|\\det \\frac{\\partial(x,y)}{\\partial(u,v)}\\right|", keywords: "change variables jacobian substitution variabelbyte jacobi analys 2" },
        { name: "Polar coordinates", insertText: "|=r\\cos\\theta, \\quad =r\\sin\\theta", example: "x=r\\cos\\theta, y=r\\sin\\theta", keywords: "polar coordinates polära koordinater variabelbyte analys 2" },
        { name: "Flux integral", insertText: "\\iint_{|} F\\cdot n\\,dS", example: "\\iint_S F\\cdot n\\,dS", keywords: "flux integral surface normal flöde ytintegral analys 2" },
        { name: "First-order ODE", insertText: "|'+p(x)y=q(x)", example: "y'+p(x)y=q(x)", keywords: "ordinary differential equation ode differentialekvation första ordningen analys 2" },
        { name: "Second-order ODE", insertText: "|''+ay'+by=f(x)", example: "y''+ay'+by=f(x)", keywords: "ordinary differential equation ode differentialekvation andra ordningen analys 2" }
    ]
    readonly property var latexCommandSwedishNames: ({
        "Square root": "Kvadratrot",
        "Nth root": "N:te rot",
        "Fraction": "Bråk",
        "Power": "Potens",
        "Subscript": "Nedsänkt index",
        "Sum": "Summa",
        "Product": "Produkt",
        "Integral": "Integral",
        "Limit": "Gränsvärde",
        "Derivative": "Derivata",
        "Partial derivative": "Partiell derivata",
        "Infinity": "Oändlighet",
        "Plus or minus": "Plus eller minus",
        "Not equal": "Inte lika med",
        "Approximately equal": "Ungefär lika med",
        "Less than or equal": "Mindre än eller lika med",
        "Greater than or equal": "Större än eller lika med",
        "Arrow": "Pil",
        "Vector": "Vektor",
        "Absolute value": "Absolutbelopp",
        "Parentheses": "Parenteser",
        "Matrix": "Matris",
        "Cases": "Falluppdelning",
        "Set builder": "Mängdbyggare",
        "Element of": "Tillhör",
        "Not an element of": "Tillhör inte",
        "Subset or equal": "Delmängd eller lika",
        "Not a subset": "Inte delmängd",
        "Subset symbol": "Delmängdssymbol",
        "Proper subset": "Äkta delmängd",
        "Superset or equal": "Övermängd eller lika",
        "Not a superset": "Inte övermängd",
        "Union": "Union",
        "Intersection": "Snitt",
        "Set difference": "Mängddifferens",
        "Set complement": "Mängdkomplement",
        "Symmetric difference": "Symmetrisk differens",
        "Cardinality": "Kardinalitet",
        "Cartesian product": "Kartesisk produkt",
        "Indexed union": "Indexerad union",
        "Indexed intersection": "Indexerat snitt",
        "Empty set": "Tomma mängden",
        "Power set": "Potensmängd",
        "Natural numbers": "Naturliga tal",
        "Integers": "Heltal",
        "Rational numbers": "Rationella tal",
        "Real numbers": "Reella tal",
        "Complex numbers": "Komplexa tal",
        "For all": "För alla",
        "There exists": "Det finns",
        "Does not exist": "Finns inte",
        "Implies": "Medför",
        "If and only if": "Om och endast om",
        "Closed interval": "Slutet intervall",
        "Open interval": "Öppet intervall",
        "Half-open interval": "Halvöppet intervall",
        "Function mapping": "Funktionsavbildning",
        "Sequence": "Talföljd",
        "Infinite series": "Oändlig serie",
        "Left-hand limit": "Vänstergränsvärde",
        "Right-hand limit": "Högergränsvärde",
        "Continuity at a point": "Kontinuitet i en punkt",
        "Supremum": "Supremum",
        "Infimum": "Infimum",
        "Norm": "Norm",
        "Evaluation bar": "Insättningsnotation",
        "Epsilon-delta statement": "Epsilon-delta-villkor",
        "Taylor polynomial": "Taylorpolynom",
        "Sine": "Sinus",
        "Cosine": "Cosinus",
        "Tangent": "Tangens",
        "Natural logarithm": "Naturlig logaritm",
        "Exponential function": "Exponentialfunktion",
        "Function composition": "Funktionssammansättning",
        "Indefinite integral": "Obestämd integral",
        "Second derivative": "Andraderivata",
        "Maximum": "Maximum",
        "Minimum": "Minimum",
        "Greek alpha": "Grekiska alfa",
        "Greek beta": "Grekiska beta",
        "Greek theta": "Grekiska theta",
        "Greek pi": "Grekiska pi",
        "Greek sigma": "Grekiska sigma",
        "Logical and": "Logiskt och",
        "Logical or": "Logiskt eller",
        "Logical not": "Logiskt inte",
        "Divides": "Delar",
        "Does not divide": "Delar inte",
        "Congruent modulo": "Kongruent modulo",
        "Binomial coefficient": "Binomialkoefficient",
        "Recurrence relation": "Rekursionsformel",
        "Graph degree": "Nodgrad",
        "Determinant": "Determinant",
        "Matrix transpose": "Transponerad matris",
        "Matrix inverse": "Invers matris",
        "Augmented matrix": "Utökad matris",
        "Span": "Linjärt hölje",
        "Dimension": "Dimension",
        "Rank": "Rang",
        "Kernel": "Kärna",
        "Image": "Bildrum",
        "Inner product": "Skalärprodukt",
        "Projection": "Projektion",
        "Eigenvalue equation": "Egenvärdesekvation",
        "Gradient": "Gradient",
        "Directional derivative": "Riktningsderivata",
        "Jacobian": "Jacobimatris",
        "Hessian": "Hessianmatris",
        "Double integral": "Dubbelintegral",
        "Triple integral": "Trippelintegral",
        "Line integral": "Kurvintegral",
        "Surface integral": "Ytintegral",
        "Divergence": "Divergens",
        "Curl": "Rotation",
        "Laplacian": "Laplaceoperator"
        ,"Truth table": "Sanningsvärdestabell"
        ,"Equivalence class": "Ekvivalensklass"
        ,"Greatest common divisor": "Största gemensamma delare"
        ,"Graph": "Graf"
        ,"Adjacency matrix": "Grannmatris"
        ,"Factorial": "Fakultet"
        ,"Column vector": "Kolonnvektor"
        ,"Row swap": "Radbyte"
        ,"Row replacement": "Radoperation"
        ,"Linear system": "Ekvationssystem"
        ,"Characteristic polynomial": "Karakteristiskt polynom"
        ,"Diagonalisation": "Diagonalisering"
        ,"Orthogonal complement": "Ortogonalt komplement"
        ,"Change of variables": "Variabelbyte"
        ,"Polar coordinates": "Polära koordinater"
        ,"Flux integral": "Flödesintegral"
        ,"First-order ODE": "Differentialekvation av första ordningen"
        ,"Second-order ODE": "Differentialekvation av andra ordningen"
    })

    function commandDisplayName(englishName) {
        var swedishName = latexCommandSwedishNames[englishName] || ""
        return swedishName.length && swedishName !== englishName
                ? swedishName + " · " + englishName : englishName
    }

    function luminance(color) {
        return 0.299 * color.r + 0.587 * color.g + 0.114 * color.b
    }

    function makeLine(source, kind, label, asset, slide, mode) {
        return {
            source: source || "",
            kind: kind || "normal",
            label: label || "",
            asset: asset || "",
            slide: slide === undefined ? -1 : slide,
            mode: mode || "latex",
            renderedUrl: "",
            error: "",
            math: false
        }
    }

    function isContinuingRowKind(kind) {
        return kind === "definition" || kind === "theorem"
                || kind === "proof" || kind === "example" || kind === "remark"
                || kind === "exercise" || kind === "solution"
                || kind === "bullet" || kind === "numbered"
    }

    function makeLineAfter(index) {
        if (index >= 0 && index < lines.count) {
            var prior = lines.get(index)
            if (isContinuingRowKind(prior.kind))
                return makeLine("", prior.kind, prior.label, "", -1, prior.mode)
        }
        return makeLine("")
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
                slide: line.slide === undefined ? -1 : line.slide,
                mode: line.mode || "latex"
            })
        }
        return result
    }

    function setDocumentSpellingLanguage(language) {
        recordHistory()
        spellLanguage = language === "en" ? "en" : "sv"
        changed()
    }

    function ignoreSpellingWord(word) {
        if (spellingIgnored.some(function(item) { return item.toLowerCase() === word.toLowerCase() })) return
        recordHistory()
        spellingIgnored = spellingIgnored.concat([word])
        changed()
    }

    function documentData() {
        return {
            format: "foldtex-3",
            documentPath: documentPath,
            baseRevision: documentFileRevision,
            recoveryId: recoveryId,
            title: documentTitle,
            course: courseName,
            noteKind: noteKind,
            lecture: lectureName,
            problemSet: problemSetName,
            lectureDate: lectureDate,
            sourcePdf: sourcePdf,
            spellLanguage: spellLanguage,
            spellcheckEnabled: spellcheckEnabled,
            spellingIgnored: spellingIgnored,
            lines: serializedLines()
        }
    }

    function currentStateJson() {
        var data = documentData()
        delete data.baseRevision
        delete data.recoveryId
        delete data.documentPath
        return JSON.stringify(data)
    }

    function resetHistory(markSaved) {
        historyTimer.stop()
        var state = currentStateJson()
        syncDocumentEditor()
        undoHistory = [{ state: state, active: activeIndex, cursor: documentEditor.sourceCursor, anchor: documentEditor.sourceAnchor }]
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
        next.push({ state: state, active: activeIndex, cursor: documentEditor.sourceCursor, anchor: documentEditor.sourceAnchor })
        if (next.length > 150) next.shift()
        undoHistory = next
        redoHistory = []
        lastHistoryState = state
    }

    function restoreHistoryEntry(entry) {
        if (!entry || !entry.state) return
        restoringHistory = true
        var data = JSON.parse(entry.state)
        data.recoveryId = recoveryId
        applyDocumentData(data, entry.active)
        documentEditor.selectSource(entry.anchor === undefined ? documentEditor.sourceCursor : entry.anchor,
                                    entry.cursor === undefined ? documentEditor.sourceCursor : entry.cursor)
        documentEditor.forceActiveFocus()
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
        snippetExitPosition = -1
        snippetTrackedText = ""
        snippetSelectionChange = false
    }

    function updateSnippetText(editor, rowIndex) {
        if (snippetSelectionChange || snippetStopRow !== rowIndex
                || snippetStopIndex < 0 || snippetStopIndex >= snippetStops.length)
            return
        var nextText = editor.text
        var priorText = snippetTrackedText
        if (nextText === priorText) return
        var prefix = 0
        var shared = Math.min(priorText.length, nextText.length)
        while (prefix < shared && priorText[prefix] === nextText[prefix]) prefix++
        var suffix = 0
        while (suffix < shared - prefix
               && priorText[priorText.length - 1 - suffix]
                  === nextText[nextText.length - 1 - suffix]) suffix++
        var priorEditEnd = priorText.length - suffix
        var activeStop = snippetStops[snippetStopIndex]
        if (prefix < activeStop.start || priorEditEnd > activeStop.end) {
            clearSnippetStops()
            return
        }
        var delta = nextText.length - priorText.length
        snippetExitPosition += delta
        var adjusted = []
        for (var i = 0; i < snippetStops.length; ++i) {
            var stop = snippetStops[i]
            if (i < snippetStopIndex)
                adjusted.push({ start: stop.start, end: stop.end })
            else if (i === snippetStopIndex)
                adjusted.push({ start: stop.start, end: stop.end + delta })
            else
                adjusted.push({ start: stop.start + delta, end: stop.end + delta })
        }
        snippetStops = adjusted
        snippetTrackedText = nextText
    }

    function updateSnippetCursor(editor, rowIndex) {
        if (snippetSelectionChange || snippetStopRow !== rowIndex
                || snippetStopIndex < 0 || snippetStopIndex >= snippetStops.length)
            return
        var stop = snippetStops[snippetStopIndex]
        if (editor.selectionStart < stop.start || editor.selectionEnd > stop.end)
            clearSnippetStops()
    }

    function insertSnippet(editor, rowIndex, from, to, template) {
        if (editor === documentEditor && !editor.inMath()
                && editor.text.slice(0, from).trim().length
                && !/^\\(?:section|subsection|subsubsection|text(?:bf|it|tt|rm|sf|normal|sc|up|md)|emph|underline)\b/.test(template))
            template = "$" + template + "$"
        clearSnippetStops()
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
        if (editor === documentEditor) {
            var firstStop = stops.length ? stops[0] : { start: from + clean.length, end: from + clean.length }
            editor.replaceRange(from, to, clean, firstStop.start, firstStop.end)
        } else {
            editor.remove(from, to)
            editor.insert(from, clean)
        }
        snippetStops = stops
        snippetStopIndex = -1
        snippetStopRow = rowIndex
        snippetExitPosition = from + clean.length
        snippetTrackedText = editor.text
        advanceSnippet(editor, rowIndex)
        changed()
    }

    function advanceSnippet(editor, rowIndex) {
        if (snippetStopRow !== rowIndex || !snippetStops.length) return false
        snippetStopIndex++
        if (snippetStopIndex >= snippetStops.length) {
            var exitPosition = snippetExitPosition
            clearSnippetStops()
            editor.cursorPosition = Math.min(editor.text.length, Math.max(0, exitPosition))
            return true
        }
        var stop = snippetStops[snippetStopIndex]
        snippetSelectionChange = true
        editor.select(stop.start, stop.end)
        snippetSelectionChange = false
        return true
    }

    function retreatSnippet(editor, rowIndex) {
        if (snippetStopRow !== rowIndex || !snippetStops.length) return false
        snippetStopIndex = Math.max(0, snippetStopIndex - 1)
        var stop = snippetStops[snippetStopIndex]
        snippetSelectionChange = true
        editor.select(stop.start, stop.end)
        snippetSelectionChange = false
        return true
    }

    function builtInSnippetTemplates() {
        return {
            textbf: "\\textbf{«text»}",
            textit: "\\textit{«text»}",
            emph: "\\emph{«text»}",
            texttt: "\\texttt{«code»}",
            text: "\\text{«text»}",
            sqrt: "\\sqrt{«x»}",
            root: "\\sqrt[«n»]{«x»}",
            frac: "\\frac{«a»}{«b»}",
            sum: "\\sum_{«i=1»}^{«n»} «x_i»",
            prod: "\\prod_{«i=1»}^{«n»} «x_i»",
            int: "\\int_{«a»}^{«b»} «f(x)»\\,d«x»",
            lim: "\\lim_{«x \\to 0»} «f(x)»",
            set: "\\left\\{ «x» \\middle| «condition» \\right\\}",
            union: "«A» \\cup «B»",
            inter: "«A» \\cap «B»",
            card: "\\left\\lvert «A» \\right\\rvert",
            cart: "«A» \\times «B»",
            bigunion: "\\bigcup_{«i=1»}^{«n»} «A_i»",
            biginter: "\\bigcap_{«i=1»}^{«n»} «A_i»",
            forall: "\\forall «x \\in A» \\colon «statement»",
            exists: "\\exists «x \\in A» \\colon «statement»",
            abs: "\\left| «x» \\right|",
            norm: "\\left\\lVert «x» \\right\\rVert",
            seq: "(«a_n»)_{«n=1»}^{\\infty}",
            series: "\\sum_{«n=1»}^{\\infty} «a_n»",
            deriv: "\\frac{d}{d«x»} «f(x)»",
            d2: "\\frac{d^2}{d«x»^2} «f(x)»",
            eval: "\\left[ «F(x)» \\right]_{«a»}^{«b»}",
            indef: "\\int «f(x)»\\,d«x»",
            func: "«f» \\colon «A» \\to «B»",
            comp: "(«f» \\circ «g»)(«x»)",
            sin: "\\sin(«x»)",
            cos: "\\cos(«x»)",
            tan: "\\tan(«x»)",
            ln: "\\ln(«x»)",
            exp: "\\exp(«x»)",
            cint: "\\left[ «a», «b» \\right]",
            oint: "\\left( «a», «b» \\right)",
            epsdel: "\\forall \\varepsilon > 0\\; \\exists \\delta > 0 \\colon «statement»",
            taylor: "\\sum_{«k=0»}^{«n»} \\frac{f^{(k)}(«a»)}{k!}(x-«a»)^k",
            nn: "\\mathbb{N}",
            zz: "\\mathbb{Z}",
            qq: "\\mathbb{Q}",
            rr: "\\mathbb{R}",
            cc: "\\mathbb{C}",
            cases: "\\begin{cases} «x», & «x > 0» \\\\ «0», & «x \\leq 0» \\end{cases}",
            mat2: "\\begin{bmatrix} «a» & «b» \\\\ «c» & «d» \\end{bmatrix}",
            mat3: "\\begin{bmatrix} «a» & «b» & «c» \\\\ «d» & «e» & «f» \\\\ «g» & «h» & «i» \\end{bmatrix}",
            and: "«P» \\land «Q»",
            or: "«P» \\lor «Q»",
            not: "\\neg «P»",
            choose: "\\binom{«n»}{«k»}",
            mod: "«a» \\equiv «b» \\pmod{«n»}",
            divides: "«a» \\mid «b»",
            recur: "«a_{n+1}» = «formula»",
            truth: "\\begin{array}{c|c|c} P & Q & «P \\land Q» \\\\ \\hline T&T&«T» \\\\ T&F&«F» \\\\ F&T&«F» \\\\ F&F&«F» \\end{array}",
            equivclass: "[«a»]_{«R»}",
            gcd: "\\gcd(«a»,«b»)",
            graph: "«G»=(«V»,«E»)",
            adjacency: "A_{«ij»}=«1»",
            induct: "\\begin{aligned} &\\text{Base case: } «P(0)» \\\\ &\\text{Assume: } «P(k)» \\\\ &\\text{Show: } «P(k+1)» \\end{aligned}",
            det: "\\det\\left( «A» \\right)",
            trans: "«A»^{\\mathsf T}",
            invmat: "«A»^{-1}",
            aug: "\\left[\\begin{array}{cc|c} «a» & «b» & «e» \\\\ «c» & «d» & «f» \\end{array}\\right]",
            span: "\\operatorname{span}\\{ «v_1», «v_2» \\}",
            basis: "\\mathcal{B}=\\{ «v_1», «v_2» \\}",
            dim: "\\dim(«V»)",
            rank: "\\operatorname{rank}(«A»)",
            ker: "\\ker(«T»)",
            image: "\\operatorname{im}(«T»)",
            dot: "\\left\\langle «u», «v» \\right\\rangle",
            proj: "\\operatorname{proj}_{«u»}(«v»)",
            eigen: "«A»«v» = «\\lambda»«v»",
            colvec: "\\begin{bmatrix} «a» \\\\ «b» \\end{bmatrix}",
            rowswap: "R_{«1»} \\leftrightarrow R_{«2»}",
            rowadd: "R_{«2»} \\leftarrow R_{«2»} + «c»R_{«1»}",
            linsys: "\\begin{cases} «a x+b y=c» \\\\ «d x+e y=f» \\end{cases}",
            charpoly: "\\det(«A»-\\lambda I)=«0»",
            diag: "«A»=«P»«D»«P»^{-1}",
            orthcomp: "«W»^{\\perp}",
            grad: "\\nabla «f»",
            dirder: "D_{«u»} «f» = \\nabla «f» \\cdot «u»",
            jac: "J_{«f»}(«x»)",
            hess: "H_{«f»}(«x»)",
            dint: "\\iint_{«D»} «f(x,y)»\\,d«A»",
            tint: "\\iiint_{«E»} «f(x,y,z)»\\,d«V»",
            lint: "\\int_{«C»} «f»\\,d«s»",
            sint: "\\iint_{«S»} «f»\\,d«S»",
            div: "\\nabla \\cdot «F»",
            curl: "\\nabla \\times «F»",
            laplace: "\\Delta «f»",
            changevar: "\\left|\\det \\frac{\\partial(«x,y»)}{\\partial(«u,v»)}\\right|",
            polar: "«x»=«r»\\cos «\\theta», \\quad «y»=«r»\\sin «\\theta»",
            flux: "\\iint_{«S»} «F»\\cdot «n»\\,dS",
            ode1: "«y»' + «p(x)»«y» = «q(x)»",
            ode2: "«y»'' + «a»«y»' + «b»«y» = «f(x)»"
        }
    }

    function handleSnippetTab(editor, rowIndex) {
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
            if (advanceSnippet(editor, rowIndex)) return true
            editor.insert(cursor, "    ")
            return true
        }
        var snippets = builtInSnippetTemplates()
        var customSnippet = backend.customSnippet(trigger[1], courseName)
        var template = customSnippet.found
                ? customSnippet.template : snippets[trigger[1].toLowerCase()]
        if (!template) {
            if (advanceSnippet(editor, rowIndex)) return true
            editor.insert(cursor, "    ")
            return true
        }
        insertSnippet(editor, rowIndex, cursor - trigger[1].length, cursor, template)
        return true
    }

    function changed() {
        if (loading) return
        if (!editingDocument) syncDocumentEditor()
        modified = true
        saveStatus = "Saving…"
        recoveryTimer.restart()
        historyTimer.restart()
    }

    function saveRecoveryNow() {
        var data = documentData()
        var primarySaved = false
        data.recoveryDirty = modified
        if (documentPath.length && modified) {
            data.recoveryDirty = false
            var checkedSave = backend.saveDocumentDataChecked(
                        documentPath, data, documentFileRevision)
            if (!checkedSave.saved) {
                saveStatus = checkedSave.conflict
                        ? "Changed outside — recovery saved" : "Save failed"
                data.recoveryDirty = true
                var recovered = backend.saveRecoveryData(data)
                if (!recovered) {
                    saveStatus = "Recovery failed"
                    messageDialog.message = "FoldTeX could not save this note or its Recovery copy. Keep the app open and use Save As to another folder."
                    messageDialog.open()
                    return false
                }
                if (checkedSave.conflict && !externalConflictShown) {
                    externalConflictShown = true
                    messageDialog.message = "This note changed outside FoldTeX. Your work is safe in Recovery. Reload the file or use Save As so neither copy is lost."
                    messageDialog.open()
                }
                return true
            }
            documentFileRevision = checkedSave.revision
            externalConflictShown = false
            primarySaved = true
            modified = false
            saveStatus = "Saved"
            savedHistoryState = currentStateJson()
            data = documentData()
            data.recoveryDirty = false
            backend.rememberNoteFolder(documentPath)
        }
        if (!backend.saveRecoveryData(data)) {
            saveStatus = "Recovery failed"
            return primarySaved || (documentPath.length && !modified)
        }
        if (!documentPath.length) saveStatus = "Recovery saved"
        return true
    }

    function quitApp() {
        saveRecoveryNow()
        Qt.quit()
    }

    function documentHasContent() {
        for (var i = 0; i < lines.count; ++i)
            if (lines.get(i).source.trim().length || lines.get(i).asset.length) return true
        return (documentTitle.trim().length && documentTitle !== "Untitled notes")
                || courseName.trim().length
                || lectureName.trim().length || problemSetName.trim().length
    }

    function startNewDocument() {
        recoveryTimer.stop()
        cancelQueuedRenders()
        loading = true
        lines.clear()
        lines.append(makeLine(""))
        activeIndex = 0
        documentPath = ""
        documentFileRevision = ""
        externalConflictShown = false
        recoveryId = backend.newRecoveryId()
        documentTitle = "Untitled notes"
        courseName = ""
        noteKind = "lecture"
        lectureName = ""
        problemSetName = ""
        lectureDate = ""
        sourcePdf = ""
        spellLanguage = "sv"
        spellcheckEnabled = true
        spellingIgnored = []
        pdfOpen = false
        displayMode = 0
        modified = false
        loading = false
        resetHistory(true)
        saveRecoveryNow()
        Qt.callLater(function() { win.editLine(0) })
    }

    function startNewDocumentWithDetails(kind, title, course, lecture, problemSet, date) {
        startNewDocument()
        documentTitle = title.trim().length ? title.trim()
                                             : kind === "problem-solving"
                                               && problemSet.trim().length ? problemSet.trim()
                                             : lecture.trim().length ? lecture.trim()
                                                                     : "Untitled notes"
        courseName = course.trim()
        noteKind = kind === "problem-solving" ? "problem-solving" : "lecture"
        lectureName = noteKind === "lecture" ? lecture.trim() : ""
        problemSetName = noteKind === "problem-solving" ? problemSet.trim() : ""
        lectureDate = date.trim()
        resetHistory(true)
        saveRecoveryNow()
    }

    function openNewDocumentSetup() {
        newDocumentSetupDialog.open()
    }

    function requestNewDocument() {
        if (!saveRecoveryNow()) return
        var course = courseName
        startNewDocument()
        courseName = course
        resetHistory(true)
    }

    onClosing: function(close) {
        if (!saveRecoveryNow()) {
            close.accepted = false
            messageDialog.message = "FoldTeX could not save this note or its recovery copy."
            messageDialog.open()
        }
    }

    Timer {
        id: recoveryTimer
        interval: 1000
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
        running: win.documentHasContent()
        onTriggered: backend.saveSnapshot(win.documentData())
    }

    Timer {
        id: documentScrollLinger
        objectName: "documentScrollLinger"
        interval: 2600
    }

    function renderLine(index) { syncDocumentEditor() }
    function queueRenders(includeActiveLine) { syncDocumentEditor(); documentEditor.refresh() }
    function cancelQueuedRenders() { documentEditor.cancelRenders() }
    function renderAll() { syncDocumentEditor() }
    function renderEveryLine() { queueRenders(true) }

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
        documentEditor.revealRow(selectionEnd)
    }

    function extendLineSelectionBy(amount) {
        var from = selectionEnd >= 0 ? selectionEnd : activeIndex
        extendLineSelection(from + amount)
    }

    function scrollDocumentBy(amount) {
        documentEditor.scrollBy(amount)
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
                                  current.asset, current.slide, current.mode))
        for (var i = 1; i < parts.length; ++i) {
            var source = parts[i] + (i === parts.length - 1 ? after : "")
            lines.insert(index + i, makeLine(source, current.kind, current.label,
                                             "", -1, current.mode))
        }
        activeIndex = index + parts.length - 1
        changed()
        Qt.callLater(function() {
            documentEditor.revealRow(activeIndex)
            var item = { currentContent: documentEditor }
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
                                      prior.asset, prior.slide, prior.mode))
        lines.remove(index)
        activeIndex = index - 1
        changed()
        Qt.callLater(function() {
            documentEditor.revealRow(activeIndex)
            var item = { currentContent: documentEditor }
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
                                  current.asset, current.slide, current.mode))
        lines.remove(index + 1)
        changed()
        Qt.callLater(function() { editor.cursorPosition = cursor })
        return true
    }

    function editLine(index) {
        if (index < 0 || index >= lines.count) return
        clearLineSelection()
        if (displayMode === 2) displayMode = 0
        if (index !== snippetStopRow) clearSnippetStops()
        syncDocumentEditor()
        documentEditor.editRow(index)
        activeIndex = index
    }

    function syncDocumentEditor() {
        if (syncingDocument || editingDocument || !documentEditor) return
        syncingDocument = true
        documentEditor.loadRows(serializedLines())
        syncingDocument = false
    }

    function beginDocumentEdit(separate) {
        if (separate || !historyTimer.running) {
            recordHistory()
            if (undoHistory.length) {
                var history = undoHistory.slice()
                var last = history[history.length - 1]
                history[history.length - 1] = { state: last.state, active: activeIndex,
                    cursor: documentEditor.sourceCursor, anchor: documentEditor.sourceAnchor }
                undoHistory = history
            }
        }
    }

    function focusBlankWritingArea() {
        clearLineSelection()
        var target = lines.count - 1
        if (lines.count === 0) {
            lines.append(makeLine(""))
            target = 0
            changed()
        } else if (lines.get(target).source.length !== 0) {
            lines.append(makeLineAfter(target))
            target = lines.count - 1
            changed()
        }
        activeIndex = target
        Qt.callLater(function() { win.editLine(target) })
    }

    function activeEditorItem() { return documentEditor }

    function rowContextMenu(index) { return documentRowMenu }

    function documentEditorHasFocus() {
        var editor = activeEditorItem()
        return editor && editor.activeFocus
    }

    function insertBlockBreak(editor, index) {
        if (!editor || index < 0 || index >= lines.count) return false
        var source = editor.text
        var isMath = editor.forceMath === true
        var start = Math.min(editor.selectionStart, editor.selectionEnd)
        var end = Math.max(editor.selectionStart, editor.selectionEnd)
        var prefix = ""
        var suffix = ""
        var breakText = "\n"
        if (isMath) {
            // Keep explicit math delimiters outside the alignment environment.
            var trimmed = source.trim()
            var pairs = [["\\[", "\\]"], ["\\(", "\\)"], ["$$", "$$"], ["$", "$"]]
            for (var p = 0; p < pairs.length; ++p) {
                var pair = pairs[p]
                if (trimmed.length >= pair[0].length + pair[1].length
                        && trimmed.startsWith(pair[0]) && trimmed.endsWith(pair[1])) {
                    var offset = source.indexOf(trimmed) + pair[0].length
                    var limit = offset + trimmed.length - pair[0].length - pair[1].length
                    prefix = source.slice(0, offset)
                    suffix = source.slice(limit)
                    source = source.slice(offset, limit)
                    start = Math.max(0, Math.min(source.length, start - offset))
                    end = Math.max(start, Math.min(source.length, end - offset))
                    break
                }
            }

            // Keep delimiter whitespace outside a newly inserted aligned block.
            // Blank source lines inside aligned become illegal TeX paragraphs.
            if (source.trim().length) {
                var leading = source.match(/^\s*/)[0]
                var trailing = source.match(/\s*$/)[0]
                prefix += leading
                suffix = trailing + suffix
                source = source.slice(leading.length, source.length - trailing.length)
                start = Math.max(0, Math.min(source.length, start - leading.length))
                end = Math.max(start, Math.min(source.length, end - leading.length))
            }

            // Inspect the cursor's context, not just whether an environment exists
            // somewhere in the block. Escaped braces and comments are not groups.
            var tokens = /\\(begin|end)\{([^}]+)\}|\\([A-Za-z]+)\s*\{|\\.|[{}]|%[^\n]*/g
            var groups = []
            var environments = []
            var token
            while ((token = tokens.exec(source)) && tokens.lastIndex <= start) {
                if (token[1] === "begin")
                    environments.push({ name: token[2], start: tokens.lastIndex })
                else if (token[1] === "end") environments.pop()
                else if (token[3] || token[0] === "{") groups.push(token[0])
                else if (token[0] === "}") groups.pop()
            }
            var environment = environments.length ? environments[environments.length - 1] : null
            var hasRowEnvironment = environment
                    && /^(align\*?|aligned|alignedat|gather\*?|gathered|array|cases|split|matrix|pmatrix|bmatrix|Bmatrix|vmatrix|Vmatrix)$/.test(environment.name)
            var leftAligned = !hasRowEnvironment
                    || (environment.name === "aligned"
                        && /^\s*&/.test(source.slice(environment.start)))
            breakText = " \\\\\n" + (leftAligned ? "&" : "")

            // A break inside prose must close and reopen its text groups so that
            // the LaTeX row separator belongs to aligned, not to \\text.
            var textGroups = groups.length > 0
                    && /^\\text(?:rm|sf|tt|normal|bf|md|it|up|sl|sc)?\s*\{$/.test(groups[0])
            for (var g = 1; textGroups && g < groups.length; ++g)
                textGroups = groups[g] === "{"
                        || /^\\(?:text(?:rm|sf|tt|normal|bf|md|it|up|sl|sc)?|emph)\s*\{$/.test(groups[g])
            if (textGroups)
                breakText = "}".repeat(groups.length) + breakText + groups.join("")
            if (!hasRowEnvironment) {
                prefix += "\\begin{aligned}\n&"
                suffix = "\n\\end{aligned}" + suffix
            }
        }
        var next = prefix + source.slice(0, start) + breakText + source.slice(end) + suffix
        var cursor = prefix.length + start + breakText.length

        recordHistory()
        editor.text = next
        editor.cursorPosition = cursor
        return true
    }

    function documentBlockBreak() {
        var editor = documentEditor
        var source = editor.text
        var span = editor.mathAtCursor()
        var start = span.start === undefined ? 0 : span.start
        var end = span.end === undefined ? source.length : span.end
        var fragment = {
            text: source.slice(start, end),
            selectionStart: Math.max(0, editor.selectionStart - start),
            selectionEnd: Math.max(0, editor.selectionEnd - start),
            cursorPosition: editor.cursorPosition - start,
            forceMath: span.start !== undefined,
            forceText: span.start === undefined
        }
        if (insertBlockBreak(fragment, activeIndex)) {
            editor.text = source.slice(0, start) + fragment.text + source.slice(end)
            editor.cursorPosition = start + fragment.cursorPosition
        }
    }

    function updateInlineCompletion() {
        var prefix = documentEditor.completionPrefix
        inlineCompletionResults.clear()
        if (!prefix.length) { inlineCompletion.close(); return }
        for (var i = 0; i < latexCommands.length; ++i) {
            var command = latexCommands[i]
            var match = /^\\([A-Za-z]+)/.exec(command.insertText)
            if (match && match[1].startsWith(prefix))
                inlineCompletionResults.append({ command: match[1], label: commandDisplayName(command.name),
                                                 insertion: command.insertText })
            if (inlineCompletionResults.count >= 6) break
        }
        if (inlineCompletionResults.count) {
            inlineSuggestions.currentIndex = 0
            inlineCompletion.open()
        } else inlineCompletion.close()
    }

    function acceptInlineCompletion() {
        if (!inlineCompletionResults.count) return
        var suggestion = inlineCompletionResults.get(Math.max(0, inlineSuggestions.currentIndex))
        var prefix = documentEditor.completionPrefix
        var cursor = documentEditor.cursorPosition
        var template = builtInSnippetTemplates()[suggestion.command]
                || suggestion.insertion.replace("|", "«»")
        inlineCompletion.close()
        insertSnippet(documentEditor, activeIndex, cursor - prefix.length - 1, cursor, template)
        inlineCompletion.close()
        documentEditor.forceActiveFocus()
    }

    ListModel { id: inlineCompletionResults }
    Popup {
        id: inlineCompletion
        parent: documentEditor
        x: Math.min(documentEditor.width - width - 16, documentEditor.cursorRectangle.x)
        y: documentEditor.cursorRectangle.y + documentEditor.cursorRectangle.height + height + 12 < documentEditor.height
           ? documentEditor.cursorRectangle.y + documentEditor.cursorRectangle.height + 6
           : Math.max(0, documentEditor.cursorRectangle.y - height - 6)
        width: Math.min(380, documentEditor.width - 32)
        height: Math.min(6, inlineCompletionResults.count) * 40 + 12
        padding: 6
        focus: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        contentItem: ListView {
            id: inlineSuggestions
            model: inlineCompletionResults
            clip: true
            delegate: ItemDelegate {
                width: inlineSuggestions.width
                height: 40
                text: "\\" + model.command + "   " + model.label
                highlighted: index === inlineSuggestions.currentIndex
                onClicked: { inlineSuggestions.currentIndex = index; win.acceptInlineCompletion() }
            }
        }
    }

    function keyboardScrollDocument(amount) {
        documentScrollLinger.restart()
        scrollDocumentBy(amount)
    }

    function commitLine(index) {
        if (documentEditor.activeRow !== index) editLine(index)
        documentEditor.pasteText("\n")
    }

    function insertLineAfter(index) {
        if (index < 0 || index >= lines.count) return
        recordHistory()
        var target = index + 1
        lines.insert(target, makeLineAfter(index))
        activeIndex = target
        changed()
        Qt.callLater(function() {
            documentEditor.revealRow(target)
            win.editLine(target)
        })
    }

    function saveTo(path) {
        if (!path) return false
        path = backend.normalizedPath(path)
        recordHistory()
        var oldAssets = []
        for (var i = 0; i < lines.count; ++i) oldAssets.push(lines.get(i).asset || "")
        var oldPdf = sourcePdf
        if (!prepareImageAssets(path)) {
            for (var j = 0; j < lines.count && j < oldAssets.length; ++j)
                lines.setProperty(j, "asset", oldAssets[j])
            sourcePdf = oldPdf
            return false
        }
        var checkedSave = backend.saveDocumentDataChecked(
                    path, documentData(), path === backend.normalizedPath(documentPath)
                    ? documentFileRevision : null)
        if (checkedSave.saved) {
            documentPath = path
            documentFileRevision = checkedSave.revision
            externalConflictShown = false
            modified = false
            saveStatus = "Saved"
            savedHistoryState = currentStateJson()
            backend.saveSnapshot(documentData())
            backend.rememberNoteFolder(path)
            saveRecoveryNow()
            for (var savedIndex = 0; savedIndex < lines.count
                 && savedIndex < oldAssets.length; ++savedIndex) {
                if (oldAssets[savedIndex].length
                        && oldAssets[savedIndex] !== lines.get(savedIndex).asset)
                    backend.removeTemporaryAsset(oldAssets[savedIndex])
            }
            return true
        }
        for (var k = 0; k < lines.count && k < oldAssets.length; ++k)
            lines.setProperty(k, "asset", oldAssets[k])
        sourcePdf = oldPdf
        var recoveryData = documentData()
        recoveryData.recoveryDirty = true
        var recoverySaved = backend.saveRecoveryData(recoveryData)
        messageDialog.message = recoverySaved
                ? (checkedSave.conflict
                   ? "This note changed outside FoldTeX. Use Save As or reload it. Your current work remains in Recovery."
                   : "Could not save the note. Your current work remains in Recovery.")
                : "FoldTeX could not save this note or its Recovery copy. Keep the app open and use Save As to another folder."
        messageDialog.open()
        return false
    }

    function prepareImageAssets(path) {
        if (documentPath === path) return true
        for (var i = 0; i < lines.count; ++i) {
            var line = lines.get(i)
            if (line.kind !== "image" || !line.asset.length) continue
            var result = backend.adoptAsset(path, line.asset)
            if (result.error) {
                messageDialog.message = result.error
                messageDialog.open()
                return false
            }
            lines.setProperty(i, "asset", result.path)
        }
        if (sourcePdf.length) {
            var pdfResult = backend.importPdf(path, sourcePdf)
            if (pdfResult.error) {
                messageDialog.message = pdfResult.error
                messageDialog.open()
                return false
            }
            sourcePdf = pdfResult.path
        }
        return true
    }

    function cycleDisplayMode() {
        cancelQueuedRenders()
        displayMode = (displayMode + 1) % 3
        clearLineSelection()
        documentEditor.forceActiveFocus()
    }

    function displayModeName() {
        return displayMode === 1 ? "Source" : displayMode === 2 ? "Rendered" : "Auto"
    }

    function openSearch() {
        searchPopup.open()
    }

    function setActiveRowKind(kind) {
        if (activeIndex < 0 || activeIndex >= lines.count) return
        lines.setProperty(activeIndex, "kind", kind)
        lines.setProperty(activeIndex, "label",
                          kind === "normal" ? "" : kind.charAt(0).toUpperCase() + kind.slice(1))
        changed()
        rowTypePopup.close()
    }

    function moveActiveRow(amount) {
        if (activeIndex < 0 || activeIndex >= lines.count) return
        var target = Math.max(0, Math.min(lines.count - 1, activeIndex + amount))
        if (target === activeIndex) return
        recordHistory()
        var row = lines.get(activeIndex)
        lines.remove(activeIndex)
        lines.insert(target, makeLine(row.source, row.kind, row.label,
                                      row.asset, row.slide, row.mode))
        activeIndex = target
        changed()
        Qt.callLater(function() { documentEditor.revealRow(activeIndex) })
    }

    function duplicateActiveRow() {
        if (activeIndex < 0 || activeIndex >= lines.count) return
        recordHistory()
        var row = lines.get(activeIndex)
        var target = activeIndex + 1
        lines.insert(target, makeLine(row.source, row.kind, row.label,
                                      row.asset, row.slide, row.mode))
        activeIndex = target
        changed()
        Qt.callLater(function() { editLine(target) })
    }

    function deleteActiveRow() {
        if (activeIndex < 0 || activeIndex >= lines.count) return
        selectionAnchor = activeIndex
        selectionEnd = activeIndex
        deleteSelectedLines()
    }

    function listRowNumber(index) {
        if (index < 0 || index >= lines.count || lines.get(index).kind !== "numbered") return 0
        var number = 1
        for (var i = index - 1; i >= 0 && lines.get(i).kind === "numbered"; --i) number++
        return number
    }

    function editImageCaption(index) {
        if (index < 0 || index >= lines.count || lines.get(index).kind !== "image") return
        imageCaptionDialog.rowIndex = index
        imageCaptionField.text = lines.get(index).source || ""
        imageCaptionDialog.open()
    }

    function replaceImage(index) {
        if (index < 0 || index >= lines.count || lines.get(index).kind !== "image") return
        replaceImageDialog.rowIndex = index
        replaceImageDialog.open()
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
        var writingTarget = target + 1
        if (writingTarget >= lines.count
                || lines.get(writingTarget).kind !== "normal"
                || lines.get(writingTarget).source.length !== 0)
            lines.insert(writingTarget, makeLine(""))
        activeIndex = writingTarget
        changed()
        Qt.callLater(function() { editLine(writingTarget) })
    }

    function pasteClipboardImage() {
        addImportedImage(backend.importClipboardImage(documentPath))
    }

    function openFigureEditor() {
        figureEditor.open()
    }

    function attachPdf(result) {
        if (result.error) {
            messageDialog.message = result.error
            messageDialog.open()
            return
        }
        recordHistory()
        sourcePdf = result.path
        pdfOpen = true
        changed()
    }

    function togglePdf() {
        if (!sourcePdf.length) {
            slidesDialog.open()
            return
        }
        pdfOpen = !pdfOpen
    }

    function linkActiveRowToSlide() {
        if (!sourcePdf.length || activeIndex < 0 || activeIndex >= lines.count
                || pdfView.currentPage < 0) return
        recordHistory()
        lines.setProperty(activeIndex, "slide", pdfView.currentPage)
        changed()
        saveStatus = "Linked to slide " + (pdfView.currentPage + 1)
    }

    function openRowSlide(index) {
        if (!sourcePdf.length || index < 0 || index >= lines.count
                || lines.get(index).slide < 0) return
        pdfOpen = true
        var page = lines.get(index).slide
        Qt.callLater(function() { pdfView.goToPage(page) })
    }

    function captureSlide() {
        if (!sourcePdf.length || pdfView.currentPage < 0) return
        var target = backend.newFigureAsset(documentPath)
        if (target.error) {
            messageDialog.message = target.error
            messageDialog.open()
            return
        }
        pdfView.grabToImage(function(result) {
            if (!result.saveToFile(target.path)) {
                messageDialog.message = "Could not capture this slide"
                messageDialog.open()
                return
            }
            win.addImportedImage({ path: target.path })
        })
    }

    function runCourseSearch() {
        courseResults.clear()
        var found = backend.searchCourse(documentPath, courseSearchInput.text)
        for (var i = 0; i < found.length; ++i)
            courseResults.append(found[i])
        courseResultList.currentIndex = courseResults.count ? 0 : -1
    }

    function noteLibrarySortKey() {
        return noteLibrarySort.currentIndex === 1 ? "date"
                : noteLibrarySort.currentIndex === 2 ? "title"
                : noteLibrarySort.currentIndex === 3 ? "course" : "updated"
    }

    function highlightLibraryPreview(source, query) {
        var escaped = source.replace(/&/g, "&amp;").replace(/</g, "&lt;")
                            .replace(/>/g, "&gt;")
        var terms = query.trim().split(/\s+/)
        for (var i = 0; i < terms.length; ++i) {
            if (!terms[i].length) continue
            var htmlTerm = terms[i].replace(/&/g, "&amp;").replace(/</g, "&lt;")
                                   .replace(/>/g, "&gt;")
            var pattern = htmlTerm.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")
            escaped = escaped.replace(new RegExp("(" + pattern + ")", "gi"), "<b>$1</b>")
        }
        return escaped
    }

    function refreshNoteLibrary() {
        noteLibraryResults.clear()
        var course = noteLibraryCourse.currentIndex > 0
                ? noteLibraryCourse.currentText : ""
        var kind = noteLibraryKind.currentIndex === 1 ? "lecture"
                : noteLibraryKind.currentIndex === 2 ? "problem-solving" : ""
        var found = backend.noteLibrary(noteLibrarySearch.text, noteLibrarySortKey(),
                                        course, kind)
        for (var i = 0; i < found.length; ++i)
            noteLibraryResults.append(found[i])
        noteLibraryList.currentIndex = noteLibraryResults.count ? 0 : -1
    }

    function refreshNoteLibraryState() {
        var state = backend.noteLibraryState()
        noteLibraryCourses.clear()
        noteLibraryCourses.append({ label: "All courses" })
        var courses = state.courses || []
        for (var i = 0; i < courses.length; ++i)
            noteLibraryCourses.append({ label: courses[i] })
        noteLibraryFolders.clear()
        var folders = state.folders || []
        for (var j = 0; j < folders.length; ++j)
            noteLibraryFolders.append(folders[j])
        noteLibraryLastNote = state.lastNote || ({})
    }

    function openNoteLibrary() {
        backend.rememberNoteFolder(documentPath)
        refreshNoteLibraryState()
        noteLibraryPopup.open()
    }

    function refreshRecoveries() {
        recoveryResults.clear()
        var found = backend.recoveryEntries()
        for (var i = 0; i < found.length; ++i) recoveryResults.append(found[i])
    }

    function openRecoveryCenter() {
        refreshRecoveries()
        recoveryPopup.open()
    }

    function restoreRecovery(item) {
        if (!item || !item.path) return
        if (!saveRecoveryNow()) return
        var data = backend.loadDocument(item.path)
        if (data.error) {
            messageDialog.message = data.error
            messageDialog.open()
            return
        }
        data.recoveryDirty = true
        loadData(data, data.documentPath || "")
        recoveryPopup.close()
        noteLibraryPopup.close()
        Qt.callLater(function() { editLine(activeIndex) })
    }

    function openLibraryNote(item) {
        if (!item) return
        if (!saveRecoveryNow()) return
        var data = backend.loadDocument(item.path)
        if (data.error) {
            messageDialog.message = data.error
            messageDialog.open()
            return
        }
        loadData(data, item.path)
        backend.rememberOpenedNote(item.path)
        noteLibraryPopup.close()
        Qt.callLater(function() { editLine(activeIndex) })
    }

    function openDocumentPath(path) {
        if (!path || !saveRecoveryNow()) return
        var data = backend.loadDocument(path)
        if (data.error) {
            messageDialog.message = data.error
            messageDialog.open()
            return
        }
        loadData(data, path)
        Qt.callLater(function() { editLine(activeIndex) })
    }

    function toggleLibraryPin(item) {
        backend.setNotePinned(item.path, !item.pinned)
        refreshNoteLibrary()
        refreshNoteLibraryState()
    }

    function newNoteForLibraryCourse() {
        if (documentHasContent() && (modified || !documentPath.length)) {
            messageDialog.message = "Save or close the current note before starting another note"
            messageDialog.open()
            return
        }
        newDocumentCoursePreset = noteLibraryCourse.currentIndex > 0
                ? noteLibraryCourse.currentText : ""
        noteLibraryPopup.close()
        newDocumentSetupDialog.open()
    }

    function openCourseResult(item) {
        if (!item) return
        if (!saveRecoveryNow()) return
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
            documentEditor.editRow(lineIndex, position)
            documentEditor.select(position, position + length)
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
        cancelQueuedRenders()
        loading = true
        lines.clear()
        documentTitle = data.title || "Untitled notes"
        courseName = data.course || ""
        noteKind = data.noteKind === "problem-solving" ? "problem-solving" : "lecture"
        lectureName = data.lecture || ""
        problemSetName = data.problemSet || ""
        lectureDate = data.lectureDate || ""
        sourcePdf = data.sourcePdf || ""
        spellLanguage = data.spellLanguage === "en" ? "en" : "sv"
        spellcheckEnabled = data.spellcheckEnabled !== false
        spellingIgnored = Array.isArray(data.spellingIgnored) ? data.spellingIgnored.filter(function(word) { return typeof word === "string" }) : []
        recoveryId = data.recoveryId || backend.newRecoveryId()
        pdfOpen = false
        var loaded = backend.migrateRowModes(data.lines || [])
        for (var i = 0; i < loaded.length; ++i) {
            var item = loaded[i]
            lines.append(makeLine(item.source || "", item.kind || "normal",
                                  item.label || "", item.asset || "",
                                  item.slide === undefined ? -1 : item.slide,
                                  item.mode || "latex"))
        }
        if (lines.count === 0 || lines.get(lines.count - 1).source.length !== 0)
            lines.append(makeLine(""))
        activeIndex = preferredActive === undefined
                ? Math.max(0, lines.count - 1)
                : Math.max(0, Math.min(lines.count - 1, preferredActive))
        loading = false
        var target = activeIndex
        syncDocumentEditor()
        documentEditor.editRow(target)
        activeIndex = target
        Qt.callLater(renderAll)
    }

    function loadData(data, path) {
        if (data.error) return
        applyDocumentData(data)
        documentPath = backend.normalizedPath(path || data.documentPath || "")
        documentFileRevision = data.recoveryDirty === true
                ? (data.baseRevision || "")
                : (documentPath.length ? backend.fileRevision(documentPath) : "")
        externalConflictShown = false
        modified = data.recoveryDirty === true
        saveStatus = modified ? "Recovered changes"
                              : documentPath.length ? "Saved" : "Recovery saved"
        backend.rememberNoteFolder(documentPath)
        backend.rememberOpenedNote(documentPath)
        resetHistory(!modified)
    }

    function loadStartupRecovery() {
        var recovered = backend.loadRecovery()
        if (recovered.error || !recovered.lines || !recovered.lines.length) return false
        if (recovered.recoveryDirty !== true && recovered.documentPath) {
            var live = backend.loadDocument(recovered.documentPath)
            if (!live.error) {
                loadData(live, recovered.documentPath)
                return true
            }
        }
        loadData(recovered, recovered.documentPath || "")
        return true
    }

    function updateCommandResults(query) {
        commandResults.clear()
        var q = query.trim().toLowerCase()
        var matches = []
        for (var i = 0; i < latexCommands.length; ++i) {
            var command = latexCommands[i]
            var name = command.name.toLowerCase()
            var swedishName = (latexCommandSwedishNames[command.name] || "").toLowerCase()
            var haystack = name + " " + swedishName + " " + command.keywords
                    + " " + command.example.toLowerCase()
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
                if (name === q || swedishName === q) score += 100
                else if (name.indexOf(q) === 0 || swedishName.indexOf(q) === 0) score += 60
                else if (name.indexOf(q) >= 0 || swedishName.indexOf(q) >= 0) score += 30
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
                commandName: commandDisplayName(item.name),
                insertText: item.insertText,
                example: item.example
            })
        }
        commandList.currentIndex = commandResults.count ? 0 : -1
    }

    function firstCommandResultName() {
        return commandResults.count ? commandResults.get(0).commandName : ""
    }

    function commandsMissingSwedishNames() {
        var missing = []
        for (var i = 0; i < latexCommands.length; ++i)
            if (!(latexCommandSwedishNames[latexCommands[i].name] || "").length)
                missing.push(latexCommands[i].name)
        return missing
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
        var editor = activeEditorItem()
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
    }

    ListModel { id: lines }
    ListModel { id: commandResults }
    ListModel { id: courseResults }
    ListModel { id: noteLibraryResults }
    ListModel { id: noteLibraryCourses }
    ListModel { id: noteLibraryFolders }
    ListModel { id: recoveryResults }

    PdfDocument {
        id: lecturePdf
        source: win.sourcePdf.length ? backend.fileUrl(win.sourcePdf) : ""
    }

    Shortcut { sequence: StandardKey.Save; onActivated: documentPath ? saveTo(documentPath) : saveDialog.open() }
    Shortcut {
        sequence: StandardKey.Undo
        context: Qt.ApplicationShortcut
        enabled: !figureEditor.visible
        onActivated: undoDocument()
    }
    Shortcut { sequence: StandardKey.Redo; context: Qt.ApplicationShortcut; onActivated: redoDocument() }
    Shortcut { sequence: StandardKey.SaveAs; onActivated: saveDialog.open() }
    Shortcut { sequence: "Ctrl+O"; context: Qt.ApplicationShortcut; onActivated: openNoteLibrary() }
    Shortcut { sequence: "Ctrl+N"; context: Qt.ApplicationShortcut; onActivated: requestNewDocument() }
    Shortcut { sequence: StandardKey.Quit; onActivated: quitApp() }
    Shortcut { sequence: "Ctrl+,"; onActivated: fontDialog.open() }
    Shortcut { sequence: "Ctrl+'"; onActivated: fontDialog.open() }
    Shortcut { sequence: "Ctrl+Shift+F"; onActivated: fontDialog.open() }
    Shortcut { sequence: "Ctrl+K"; context: Qt.ApplicationShortcut; onActivated: openCommandFinder() }
    Shortcut { sequence: "Ctrl+."; context: Qt.ApplicationShortcut; onActivated: rowTypePopup.open() }
    Shortcut { sequence: "Alt+Up"; context: Qt.ApplicationShortcut; onActivated: moveActiveRow(-1) }
    Shortcut { sequence: "Alt+Down"; context: Qt.ApplicationShortcut; onActivated: moveActiveRow(1) }
    Shortcut { sequence: "Ctrl+D"; context: Qt.ApplicationShortcut; onActivated: duplicateActiveRow() }
    Shortcut { sequence: "Ctrl+M"; context: Qt.ApplicationShortcut; onActivated: addCatchupMarker() }
    Shortcut { sequence: "Ctrl+Alt+L"; context: Qt.ApplicationShortcut; onActivated: lectureDialog.open() }
    Shortcut { sequence: "Ctrl+Alt+F"; context: Qt.ApplicationShortcut; onActivated: courseSearchPopup.open() }
    Shortcut { sequence: "Ctrl+Shift+V"; context: Qt.ApplicationShortcut; onActivated: pasteClipboardImage() }
    Shortcut { sequence: "Ctrl+Alt+I"; context: Qt.ApplicationShortcut; onActivated: openFigureEditor() }
    Shortcut { sequence: "Ctrl+Alt+P"; context: Qt.ApplicationShortcut; onActivated: togglePdf() }
    Shortcut { sequence: "Ctrl+Alt+S"; context: Qt.ApplicationShortcut; onActivated: snippetManager.openManager() }
    Shortcut { sequence: "Ctrl+G"; context: Qt.WindowShortcut; onActivated: openGuide() }
    Shortcut { sequence: StandardKey.Find; context: Qt.ApplicationShortcut; onActivated: openSearch() }
    Shortcut { sequence: "Ctrl+Shift+R"; context: Qt.ApplicationShortcut; onActivated: cycleDisplayMode() }
    Shortcut { sequence: "Ctrl+Shift+E"; context: Qt.ApplicationShortcut; onActivated: exportMenu.open() }
    Shortcut { sequence: "F1"; context: Qt.ApplicationShortcut; onActivated: helpDialog.open() }
    Shortcut { sequence: "Ctrl+Shift+A"; context: Qt.ApplicationShortcut; onActivated: {
        documentEditor.selectAll()
    } }
    Shortcut { sequence: StandardKey.Copy; enabled: win.hasLineSelection; onActivated: copySelectedLines() }
    Shortcut { sequence: StandardKey.Cut; enabled: win.hasLineSelection; onActivated: cutSelectedLines() }
    Shortcut { sequence: "Delete"; enabled: win.hasLineSelection; onActivated: deleteSelectedLines() }
    Shortcut {
        objectName: "documentScrollUpShortcut"
        sequence: "Up"
        context: Qt.WindowShortcut
        enabled: !win.documentEditorHasFocus() && !titleField.activeFocus
                 && !searchPopup.visible && !commandPopup.visible
                 && !rowTypePopup.visible && !courseSearchPopup.visible
                 && !fontDialog.visible && !lectureDialog.visible
                 && !helpDialog.visible && !messageDialog.visible
                 && !newDocumentDialog.visible && !figureEditor.visible
                 && !snippetManager.visible
        onActivated: win.keyboardScrollDocument(-Math.max(48, win.editorSize * 3))
    }
    Shortcut {
        objectName: "documentScrollDownShortcut"
        sequence: "Down"
        context: Qt.WindowShortcut
        enabled: !win.documentEditorHasFocus() && !titleField.activeFocus
                 && !searchPopup.visible && !commandPopup.visible
                 && !rowTypePopup.visible && !courseSearchPopup.visible
                 && !fontDialog.visible && !lectureDialog.visible
                 && !helpDialog.visible && !messageDialog.visible
                 && !newDocumentDialog.visible && !figureEditor.visible
        onActivated: win.keyboardScrollDocument(Math.max(48, win.editorSize * 3))
    }

    Popup {
        id: searchPopup
        parent: Overlay.overlay
        x: Math.round((win.width - width) / 2)
        y: 12
        width: Math.min(620, win.width - 40)
        modal: false
        focus: true
        padding: 12
        closePolicy: Popup.CloseOnEscape

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
        objectName: "rowTypePopup"
        readonly property var kinds: ["normal", "heading", "subheading", "bullet", "numbered",
                                      "definition", "theorem", "proof", "example", "remark",
                                      "exercise", "solution"]
        readonly property var labels: ["Normal", "Heading", "Subheading", "Bullet", "Numbered",
                                       "Definition", "Theorem", "Proof", "Example", "Remark",
                                       "Exercise", "Solution"]
        function applyCurrentType() {
            if (rowTypeList.currentIndex >= 0)
                win.setActiveRowKind(kinds[rowTypeList.currentIndex])
        }
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(380, win.width - 40)
        height: Math.min(430, win.height - 64)
        modal: true
        focus: true
        padding: 8
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onOpened: {
            rowTypeList.currentIndex = Math.max(0,
                kinds.indexOf(lines.get(activeIndex).kind))
            Qt.callLater(function() { rowTypeList.forceActiveFocus() })
        }
        Shortcut {
            sequence: "Up"
            context: Qt.WindowShortcut
            enabled: rowTypePopup.visible
            onActivated: {
                rowTypeList.currentIndex = rowTypeList.currentIndex <= 0
                        ? rowTypeList.count - 1 : rowTypeList.currentIndex - 1
                rowTypeList.positionViewAtIndex(rowTypeList.currentIndex, ListView.Contain)
            }
        }
        Shortcut {
            sequence: "Down"
            context: Qt.WindowShortcut
            enabled: rowTypePopup.visible
            onActivated: {
                rowTypeList.currentIndex = rowTypeList.currentIndex >= rowTypeList.count - 1
                        ? 0 : rowTypeList.currentIndex + 1
                rowTypeList.positionViewAtIndex(rowTypeList.currentIndex, ListView.Contain)
            }
        }
        Shortcut {
            sequences: ["Return", "Enter"]
            context: Qt.WindowShortcut
            enabled: rowTypePopup.visible
            onActivated: rowTypePopup.applyCurrentType()
        }
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
                objectName: "rowTypeList"
                property color optionTextColor: win.textColor
                property color selectionFillColor: Qt.rgba(
                    win.textColor.r, win.textColor.g, win.textColor.b, 0.14)
                property color selectionBorderColor: Qt.rgba(
                    win.textColor.r, win.textColor.g, win.textColor.b, 0.68)
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: rowTypePopup.labels
                focus: true
                keyNavigationWraps: true
                delegate: ItemDelegate {
                    id: rowTypeOption
                    required property string modelData
                    required property int index
                    objectName: "rowTypeOption" + index
                    property color labelColor: rowTypeList.optionTextColor
                    width: rowTypeList.width
                    height: 58
                    text: modelData
                    highlighted: ListView.isCurrentItem
                    font.family: win.editorFont
                    onClicked: win.setActiveRowKind(modelData.toLowerCase())

                    contentItem: Text {
                        text: rowTypeOption.modelData
                        color: rowTypeOption.labelColor
                        font.family: win.editorFont
                        font.pixelSize: win.editorSize
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 12
                    }

                    background: Rectangle {
                        radius: 6
                        color: rowTypeOption.highlighted
                               ? rowTypeList.selectionFillColor
                               : rowTypeOption.hovered
                                 ? Qt.rgba(win.textColor.r, win.textColor.g,
                                           win.textColor.b, 0.08)
                                 : "transparent"
                        border.width: rowTypeOption.highlighted ? 2 : 0
                        border.color: rowTypeList.selectionBorderColor
                    }
                }
            }

        }
    }

    Dialog {
        id: lectureDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(520, win.width - 48)
        height: Math.min(650, win.height - 32)
        modal: true
        title: "Note details"
        standardButtons: Dialog.Cancel | Dialog.Ok
        Material.accent: fontDialog.dialogText
        onOpened: {
            detailsKindChoice.currentIndex = win.noteKind === "problem-solving" ? 1 : 0
            courseField.text = win.courseName
            lectureField.text = win.lectureName
            problemSetField.text = win.problemSetName
            lectureDateField.text = win.lectureDate
            Qt.callLater(function() { courseField.forceActiveFocus(); courseField.selectAll() })
        }
        onAccepted: {
            win.recordHistory()
            win.noteKind = detailsKindChoice.currentIndex === 1
                    ? "problem-solving" : "lecture"
            win.courseName = courseField.text.trim()
            win.lectureName = win.noteKind === "lecture" ? lectureField.text.trim() : ""
            win.problemSetName = win.noteKind === "problem-solving"
                    ? problemSetField.text.trim() : ""
            win.lectureDate = lectureDateField.text.trim()
            win.changed()
        }
        contentItem: ScrollView {
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: 8
            Label { text: "Note type"; color: fontDialog.dialogText; font.family: win.editorFont }
            ComboBox {
                id: detailsKindChoice
                Layout.fillWidth: true
                model: ["Lecture", "Problem-solving"]
                font.family: win.editorFont
            }
            Label { text: "Course"; color: fontDialog.dialogText; font.family: win.editorFont }
            TextField {
                id: courseField
                Layout.fillWidth: true
                color: fontDialog.dialogText
                font.family: win.editorFont
                placeholderText: "Calculus I"
            }
            Label {
                text: "Lecture"
                visible: detailsKindChoice.currentIndex === 0
                color: fontDialog.dialogText
                font.family: win.editorFont
            }
            TextField {
                id: lectureField
                visible: detailsKindChoice.currentIndex === 0
                Layout.fillWidth: true
                color: fontDialog.dialogText
                font.family: win.editorFont
                placeholderText: "Lecture 4 — Limits"
            }
            Label {
                text: "Problem set or topic"
                visible: detailsKindChoice.currentIndex === 1
                color: fontDialog.dialogText
                font.family: win.editorFont
            }
            TextField {
                id: problemSetField
                visible: detailsKindChoice.currentIndex === 1
                Layout.fillWidth: true
                color: fontDialog.dialogText
                font.family: win.editorFont
                placeholderText: "Problem set 3 — derivatives"
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
    }

    Timer {
        id: noteLibrarySearchDelay
        interval: 200
        repeat: false
        onTriggered: win.refreshNoteLibrary()
    }

    Popup {
        id: noteLibraryPopup
        objectName: "noteLibraryPopup"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(760, win.width - 40)
        height: Math.min(620, win.height - 48)
        modal: true
        focus: true
        padding: 1
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onOpened: {
            noteLibrarySearch.text = ""
            noteLibraryCourse.currentIndex = 0
            noteLibraryKind.currentIndex = 0
            win.refreshNoteLibrary()
            Qt.callLater(function() { noteLibrarySearch.forceActiveFocus() })
        }
        background: Rectangle {
            color: backend.themeBackground
            radius: 10
            border.width: 1
            border.color: Qt.rgba(win.textColor.r, win.textColor.g, win.textColor.b, 0.18)
        }
        contentItem: ColumnLayout {
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 62
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                spacing: 10

                TextField {
                    id: noteLibrarySearch
                    objectName: "noteLibrarySearch"
                    Layout.fillWidth: true
                    placeholderText: "Search notes, courses, dates, or content"
                    color: win.textColor
                    font.family: win.editorFont
                    font.pixelSize: win.editorSize
                    onTextEdited: noteLibrarySearchDelay.restart()
                    Keys.onDownPressed: function(event) {
                        if (noteLibraryResults.count) noteLibraryList.forceActiveFocus()
                        event.accepted = true
                    }
                    Keys.onReturnPressed: function(event) {
                        if (noteLibraryList.currentIndex >= 0)
                            win.openLibraryNote(noteLibraryResults.get(noteLibraryList.currentIndex))
                        event.accepted = true
                    }
                }

                ComboBox {
                    id: noteLibrarySort
                    objectName: "noteLibrarySort"
                    Layout.preferredWidth: 178
                    model: ["Recently updated", "Lecture date", "Title", "Course"]
                    font.family: win.editorFont
                    onActivated: win.refreshNoteLibrary()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Layout.bottomMargin: 8
                spacing: 10

                ComboBox {
                    id: noteLibraryCourse
                    objectName: "noteLibraryCourse"
                    Layout.fillWidth: true
                    textRole: "label"
                    model: noteLibraryCourses
                    font.family: win.editorFont
                    onActivated: win.refreshNoteLibrary()
                }
                ComboBox {
                    id: noteLibraryKind
                    objectName: "noteLibraryKind"
                    Layout.preferredWidth: 190
                    model: ["All note types", "Lectures", "Problem-solving"]
                    font.family: win.editorFont
                    onActivated: win.refreshNoteLibrary()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Qt.rgba(win.textColor.r, win.textColor.g, win.textColor.b, 0.1)
            }

            ListView {
                id: noteLibraryList
                objectName: "noteLibraryList"
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: noteLibraryResults
                clip: true
                focus: true
                boundsBehavior: Flickable.StopAtBounds
                currentIndex: -1
                section.property: noteLibrarySort.currentIndex === 3 ? "course" : ""
                section.criteria: ViewSection.FullString
                section.delegate: Rectangle {
                    required property string section
                    width: noteLibraryList.width
                    height: 34
                    color: Qt.rgba(backend.themeAccent.r, backend.themeAccent.g,
                                   backend.themeAccent.b, 0.12)
                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        text: parent.section.length ? parent.section : "No course"
                        color: win.textColor
                        font.family: win.editorFont
                        font.pixelSize: Math.max(11, win.editorSize - 2)
                        font.weight: Font.DemiBold
                    }
                }
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                WheelHandler {
                    target: null
                    onWheel: function(event) {
                        var distance = win.wheelDistance(event.pixelDelta.y,
                                                         event.angleDelta.y)
                        var minimum = noteLibraryList.originY
                        var maximum = Math.max(minimum, minimum
                                              + noteLibraryList.contentHeight
                                              - noteLibraryList.height)
                        noteLibraryList.contentY = Math.max(minimum, Math.min(
                            maximum, noteLibraryList.contentY - distance))
                        event.accepted = true
                    }
                }

                delegate: ItemDelegate {
                    required property int index
                    required property string path
                    required property string title
                    required property string course
                    required property string noteKind
                    required property string lecture
                    required property string problemSet
                    required property string lectureDate
                    required property string modifiedLabel
                    required property string fileName
                    required property string matchPreview
                    required property bool pinned
                    width: noteLibraryList.width
                    height: matchPreview.length ? 106 : 84
                    highlighted: ListView.isCurrentItem
                    contentItem: Column {
                        spacing: 4
                        Text {
                            width: parent.width - 48
                            text: (pinned ? "★  " : "")
                                  + (noteKind === "problem-solving" && problemSet.length
                                     ? problemSet : lecture.length ? lecture : title)
                            color: win.textColor
                            elide: Text.ElideRight
                            font.family: win.editorFont
                            font.pixelSize: win.editorSize
                            font.weight: Font.DemiBold
                        }
                        Text {
                            width: parent.width - 48
                            text: (noteKind === "problem-solving"
                                   ? "Problem-solving" : "Lecture")
                                  + " · " + (course.length ? course : "No course")
                                  + (lectureDate.length ? " · " + lectureDate : "")
                                  + " · updated " + modifiedLabel
                            color: win.mutedColor
                            elide: Text.ElideRight
                            font.family: win.editorFont
                            font.pixelSize: Math.max(11, win.editorSize - 3)
                        }
                        Text {
                            width: parent.width - 48
                            text: matchPreview.length
                                  ? win.highlightLibraryPreview(matchPreview,
                                                                noteLibrarySearch.text)
                                  : fileName
                            textFormat: matchPreview.length ? Text.StyledText : Text.PlainText
                            color: win.mutedColor
                            opacity: 0.72
                            elide: Text.ElideMiddle
                            font.family: win.editorFont
                            font.pixelSize: Math.max(10, win.editorSize - 4)
                        }
                    }
                    ToolButton {
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: pinned ? "★" : "☆"
                        onClicked: win.toggleLibraryPin(noteLibraryResults.get(index))
                        ToolTip.visible: hovered
                        ToolTip.text: pinned ? "Unpin note" : "Pin note"
                    }
                    onClicked: win.openLibraryNote(noteLibraryResults.get(index))
                }

                Keys.onReturnPressed: function(event) {
                    if (currentIndex >= 0)
                        win.openLibraryNote(noteLibraryResults.get(currentIndex))
                    event.accepted = true
                }
                Keys.onUpPressed: function(event) {
                    if (currentIndex <= 0) noteLibrarySearch.forceActiveFocus()
                    else decrementCurrentIndex()
                    event.accepted = true
                }

                Label {
                    anchors.centerIn: parent
                    visible: noteLibraryResults.count === 0
                    text: noteLibrarySearch.text.length
                          ? "No matching notes"
                          : "No notes yet — add a folder or save a note"
                    color: win.mutedColor
                    font.family: win.editorFont
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Qt.rgba(win.textColor.r, win.textColor.g, win.textColor.b, 0.1)
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 54
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Label {
                    Layout.fillWidth: true
                    text: noteLibraryResults.count + (noteLibraryResults.count === 1
                          ? " note" : " notes")
                    color: win.mutedColor
                    font.family: win.editorFont
                }
                Button {
                    text: "Continue last"
                    enabled: win.noteLibraryLastNote.path !== undefined
                             && win.noteLibraryLastNote.path.length > 0
                    onClicked: win.openLibraryNote(win.noteLibraryLastNote)
                }
                Button { text: "New note"; onClicked: win.newNoteForLibraryCourse() }
                Button { text: "Recovery…"; onClicked: win.openRecoveryCenter() }
                Button { text: "Folders…"; onClicked: noteLibraryFoldersPopup.open() }
                Button {
                    text: "Browse files…"
                    onClicked: {
                        noteLibraryPopup.close()
                        openDialog.open()
                    }
                }
            }
        }
    }

    Popup {
        id: recoveryPopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(650, win.width - 48)
        height: Math.min(500, win.height - 64)
        modal: true
        focus: true
        padding: 1
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            color: backend.themeBackground
            radius: 10
            border.width: 1
            border.color: Qt.rgba(win.textColor.r, win.textColor.g, win.textColor.b, 0.18)
        }
        contentItem: ColumnLayout {
            spacing: 0
            Label {
                text: "Recovery copies and snapshots"
                color: win.textColor
                font.family: win.editorFont
                font.pixelSize: win.editorSize + 1
                font.weight: Font.DemiBold
                Layout.leftMargin: 14
                Layout.topMargin: 12
                Layout.bottomMargin: 10
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Qt.rgba(win.textColor.r, win.textColor.g, win.textColor.b, 0.1)
            }
            ListView {
                id: recoveryList
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: recoveryResults
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                delegate: ItemDelegate {
                    required property int index
                    required property string path
                    required property string entryKind
                    required property string title
                    required property string course
                    required property string lectureDate
                    required property string modifiedLabel
                    width: recoveryList.width
                    height: 76
                    contentItem: Column {
                        spacing: 4
                        Text {
                            width: parent.width - 82
                            text: title.length ? title : "Untitled notes"
                            color: win.textColor
                            elide: Text.ElideRight
                            font.family: win.editorFont
                            font.pixelSize: win.editorSize
                            font.weight: Font.DemiBold
                        }
                        Text {
                            width: parent.width - 82
                            text: entryKind + " · " + modifiedLabel
                                  + (course.length ? " · " + course : "")
                                  + (lectureDate.length ? " · " + lectureDate : "")
                            color: win.mutedColor
                            elide: Text.ElideRight
                            font.family: win.editorFont
                            font.pixelSize: Math.max(10, win.editorSize - 3)
                        }
                    }
                    Button {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Delete"
                        onClicked: {
                            backend.removeRecovery(path)
                            win.refreshRecoveries()
                        }
                    }
                    onClicked: win.restoreRecovery(recoveryResults.get(index))
                }
                Label {
                    anchors.centerIn: parent
                    visible: recoveryResults.count === 0
                    text: "No recovery copies yet"
                    color: win.mutedColor
                    font.family: win.editorFont
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                Layout.topMargin: 8
                Layout.bottomMargin: 8
                Label {
                    Layout.fillWidth: true
                    text: "Open an item to restore it as unsaved work."
                    color: win.mutedColor
                    font.family: win.editorFont
                }
                Button { text: "Done"; onClicked: recoveryPopup.close() }
            }
        }
    }

    Popup {
        id: noteLibraryFoldersPopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(620, win.width - 48)
        height: Math.min(440, win.height - 64)
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            color: backend.themeBackground
            radius: 10
            border.width: 1
            border.color: Qt.rgba(win.textColor.r, win.textColor.g, win.textColor.b, 0.18)
        }
        contentItem: ColumnLayout {
            spacing: 8
            Label {
                text: "Note folders"
                color: win.textColor
                font.family: win.editorFont
                font.pixelSize: win.editorSize + 1
                font.weight: Font.DemiBold
                Layout.leftMargin: 12
                Layout.topMargin: 10
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: noteLibraryFolders
                delegate: ItemDelegate {
                    required property int index
                    required property string path
                    required property string name
                    required property bool exists
                    width: ListView.view.width
                    height: 62
                    contentItem: Column {
                        Text {
                            width: parent.width - 90
                            text: (exists ? "" : "Missing · ") + name
                            color: exists ? win.textColor : "#ff8c85"
                            elide: Text.ElideRight
                            font.family: win.editorFont
                        }
                        Text {
                            width: parent.width - 90
                            text: path
                            color: win.mutedColor
                            elide: Text.ElideMiddle
                            font.family: win.editorFont
                            font.pixelSize: Math.max(10, win.editorSize - 4)
                        }
                    }
                    Button {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Remove"
                        onClicked: {
                            backend.removeNoteFolder(path)
                            win.refreshNoteLibraryState()
                            win.refreshNoteLibrary()
                        }
                    }
                }
                Label {
                    anchors.centerIn: parent
                    visible: noteLibraryFolders.count === 0
                    text: "No folders added"
                    color: win.mutedColor
                    font.family: win.editorFont
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                Layout.bottomMargin: 10
                Item { Layout.fillWidth: true }
                Button {
                    text: "Rescan"
                    onClicked: {
                        backend.rescanNoteLibrary()
                        win.refreshNoteLibraryState()
                        win.refreshNoteLibrary()
                    }
                }
                Button { text: "Add folder…"; onClicked: noteLibraryFolderDialog.open() }
                Button { text: "Done"; onClicked: noteLibraryFoldersPopup.close() }
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
                    text: "Sök LaTeX / Search LaTeX — prova “delmängd”"
                    color: win.mutedColor
                    verticalAlignment: Text.AlignVCenter
                    font.family: win.editorFont
                    font.pixelSize: win.editorSize
                }

                TextInput {
                    id: commandSearch
                    objectName: "commandSearch"
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
                objectName: "commandList"
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
                            width: parent.width
                            elide: Text.ElideRight
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
        Material.theme: win.luminance(win.color) < 0.5 ? Material.Dark : Material.Light
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
        backendApi: backend
        builtInSnippetTriggers: win.builtInSnippetTriggers
        builtInSnippetTemplates: win.builtInSnippetTemplates()
        backgroundColor: backend.themeBackground
        foregroundColor: backend.themeForeground
        accentColor: backend.themeAccent
        selectionColor: backend.themeSelection
        writingFont: win.editorFont
        writingSize: win.editorSize
        preferredSideMargin: backend.editorSideMargin
    }

    SnippetManager {
        id: snippetManager
        backendApi: backend
        builtInTriggers: win.builtInSnippetTriggers
        currentCourse: win.courseName
        onSnippetsChanged: guideWindow.reloadSnippets()
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
            text: "Enter                 Newline at cursor\n"
                  + "Ctrl+Enter            Insert row below\n"
                  + "Shift+Enter           Line break within block\n"
                  + "Up / Down             Scroll when not editing\n"
                  + "Tab / Shift+Tab       Next / prior snippet field\n"
                  + "Shift+click           Select text\n"
                  + "Shift+Up / Down       Extend text selection\n"
                  + "Ctrl+A                Select the document\n"
                  + "Ctrl+F                Find and replace\n"
                  + "Ctrl+G                Open the LaTeX guide\n"
                  + "Ctrl+K                Find LaTeX syntax\n"
                  + "Ctrl+.                Choose row type\n"
                  + "Alt+Up / Alt+Down     Move row\n"
                  + "Ctrl+D                Copy row\n"
                  + "Ctrl+Shift+R          Auto / source / rendered view\n"
                  + "Ctrl+Shift+E          Export TeX or PDF\n"
                  + "Ctrl+Shift+V          Paste an image\n"
                  + "Ctrl+Alt+I            Draw a figure\n"
                  + "Ctrl+Alt+P            Show or add lecture slides\n"
                  + "Ctrl+Alt+S            Custom Tab snippets\n"
                  + "Ctrl+N                Start an empty note\n"
                  + "Ctrl+S                Save\n"
                  + "Ctrl+O                Open notes library\n"
                  + "Ctrl+Q                Quit\n"
                  + "F1                    Show this help"
        }
    }

    Dialog {
        id: messageDialog
        objectName: "messageDialog"
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
        id: imageCaptionDialog
        property int rowIndex: -1
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(460, win.width - 48)
        modal: true
        title: "Figure caption"
        standardButtons: Dialog.Cancel | Dialog.Ok
        Material.accent: fontDialog.dialogText
        onAccepted: {
            if (rowIndex >= 0 && rowIndex < lines.count) {
                recordHistory()
                lines.setProperty(rowIndex, "source", imageCaptionField.text)
                changed()
            }
        }
        contentItem: TextField {
            id: imageCaptionField
            color: fontDialog.dialogText
            font.family: win.editorFont
            placeholderText: "Caption or alt text"
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
        onAccepted: Qt.callLater(win.openNewDocumentSetup)
        contentItem: Label {
            text: "Set up a new note? Changes not saved to a .foldtex file will be lost."
            color: fontDialog.dialogText
            wrapMode: Text.Wrap
            font.family: win.editorFont
        }
    }

    Dialog {
        id: newDocumentSetupDialog
        objectName: "newDocumentSetupDialog"
        property string selectedKind: "lecture"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(520, win.width - 48)
        height: Math.min(650, win.height - 32)
        modal: true
        title: "New note"
        standardButtons: Dialog.Cancel | Dialog.Ok
        Material.accent: fontDialog.dialogText
        onAboutToShow: {
            selectedKind = "lecture"
            newTitleField.text = ""
            newCourseField.text = win.newDocumentCoursePreset.length
                    ? win.newDocumentCoursePreset : win.courseName
            win.newDocumentCoursePreset = ""
            newLectureField.text = ""
            newProblemSetField.text = ""
            newLectureDateField.text = Qt.formatDate(new Date(), "yyyy-MM-dd")
            Qt.callLater(function() { newTitleField.forceActiveFocus() })
        }
        onAccepted: win.startNewDocumentWithDetails(selectedKind,
                                                    newTitleField.text,
                                                    newCourseField.text,
                                                    newLectureField.text,
                                                    newProblemSetField.text,
                                                    newLectureDateField.text)

        Shortcut {
            sequences: ["Return", "Enter"]
            context: Qt.WindowShortcut
            enabled: newDocumentSetupDialog.visible
            onActivated: newDocumentSetupDialog.accept()
        }

        contentItem: ScrollView {
            objectName: "newDocumentScroll"
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width
                spacing: 8

            Label { text: "Note type"; color: fontDialog.dialogText; font.family: win.editorFont }
            RowLayout {
                Layout.fillWidth: true
                RadioButton {
                    id: newLectureKindButton
                    objectName: "newLectureKindButton"
                    text: "Lecture"
                    checked: newDocumentSetupDialog.selectedKind === "lecture"
                    onClicked: newDocumentSetupDialog.selectedKind = "lecture"
                }
                RadioButton {
                    id: newProblemSolvingKindButton
                    objectName: "newProblemSolvingKindButton"
                    text: "Problem-solving"
                    checked: newDocumentSetupDialog.selectedKind === "problem-solving"
                    onClicked: newDocumentSetupDialog.selectedKind = "problem-solving"
                }
                Item { Layout.fillWidth: true }
            }

            Label { text: "Note title"; color: fontDialog.dialogText; font.family: win.editorFont }
            TextField {
                id: newTitleField
                objectName: "newTitleField"
                Layout.fillWidth: true
                color: fontDialog.dialogText
                font.family: win.editorFont
                placeholderText: "Lecture 4 — Limits"
            }

            Label { text: "Course or subject"; color: fontDialog.dialogText; font.family: win.editorFont }
            TextField {
                id: newCourseField
                objectName: "newCourseField"
                Layout.fillWidth: true
                color: fontDialog.dialogText
                font.family: win.editorFont
                placeholderText: "Calculus I"
            }

            Label {
                text: "Lecture"
                visible: newDocumentSetupDialog.selectedKind === "lecture"
                color: fontDialog.dialogText
                font.family: win.editorFont
            }
            TextField {
                id: newLectureField
                objectName: "newLectureField"
                visible: newDocumentSetupDialog.selectedKind === "lecture"
                Layout.fillWidth: true
                color: fontDialog.dialogText
                font.family: win.editorFont
                placeholderText: "Limits and continuity"
            }

            Label {
                text: "Problem set or topic"
                visible: newDocumentSetupDialog.selectedKind === "problem-solving"
                color: fontDialog.dialogText
                font.family: win.editorFont
            }
            TextField {
                id: newProblemSetField
                objectName: "newProblemSetField"
                visible: newDocumentSetupDialog.selectedKind === "problem-solving"
                Layout.fillWidth: true
                color: fontDialog.dialogText
                font.family: win.editorFont
                placeholderText: "Problem set 3 — derivatives"
            }

            Label { text: "Date"; color: fontDialog.dialogText; font.family: win.editorFont }
            TextField {
                id: newLectureDateField
                objectName: "newLectureDateField"
                Layout.fillWidth: true
                color: fontDialog.dialogText
                font.family: win.editorFont
                placeholderText: "2026-08-29"
            }

            Label {
                    Layout.fillWidth: true
                    text: newDocumentSetupDialog.selectedKind === "lecture"
                          ? "Leave the title blank to use the lecture name. The current course carries over."
                          : "Leave the title blank to use the problem set or topic. The current course carries over."
                    color: fontDialog.dialogMuted
                    wrapMode: Text.Wrap
                    font.family: win.editorFont
                    font.pixelSize: Math.max(11, win.editorSize - 2)
            }
        }
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
        onAccepted: win.openDocumentPath(selectedFile.toString())
    }

    Dialogs.FolderDialog {
        id: noteLibraryFolderDialog
        title: "Add a notes folder"
        onAccepted: {
            backend.addNoteFolder(selectedFolder.toString())
            win.refreshNoteLibraryState()
            win.refreshNoteLibrary()
        }
    }

    Dialogs.FileDialog {
        id: texExportDialog
        title: "Export TeX"
        fileMode: Dialogs.FileDialog.SaveFile
        nameFilters: ["TeX document (*.tex)"]
        defaultSuffix: "tex"
        onAccepted: {
            var result = backend.exportDocumentTex(selectedFile.toString(), documentData())
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
            var result = backend.exportDocumentPdf(selectedFile.toString(), documentData())
            messageDialog.message = result.error || "PDF exported"
            messageDialog.open()
        }
    }

    Dialogs.FileDialog {
        id: slidesDialog
        title: "Add lecture slides"
        fileMode: Dialogs.FileDialog.OpenFile
        nameFilters: ["PDF documents (*.pdf)"]
        onAccepted: win.attachPdf(backend.importPdf(win.documentPath,
                                                    selectedFile.toString()))
    }

    Dialogs.FileDialog {
        id: replaceImageDialog
        property int rowIndex: -1
        title: "Replace figure"
        fileMode: Dialogs.FileDialog.OpenFile
        nameFilters: ["Images (*.png *.jpg *.jpeg *.webp *.svg)"]
        onAccepted: {
            var result = backend.importAsset(win.documentPath, selectedFile.toString())
            if (result.error) {
                messageDialog.message = result.error
                messageDialog.open()
            } else if (rowIndex >= 0 && rowIndex < lines.count) {
                recordHistory()
                lines.setProperty(rowIndex, "asset", result.path)
                changed()
            }
        }
    }

    FigureEditor {
        id: figureEditor
        objectName: "figureEditorPopup"
        parent: Overlay.overlay
        backendApi: backend
        documentPath: win.documentPath
        backgroundColor: win.color
        foregroundColor: win.textColor
        mutedColor: win.mutedColor
        accentColor: backend.themeAccent
        writingFont: win.editorFont
        onFigureSaved: function(path) { win.addImportedImage({ path: path }) }
        onSaveFailed: function(message) {
            messageDialog.message = message
            messageDialog.open()
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
                readonly property real sideClearance: Math.max(
                    56, saveLabel.x + saveLabel.width + 8)
                width: Math.min(520, Math.max(120,
                                              parent.width - sideClearance * 2))
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
                id: exportButton
                objectName: "documentToolsButton"
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: "⋯"
                font.pixelSize: 24
                Accessible.name: "Document tools"
                onClicked: documentTools.open()
                ToolTip.visible: hovered
                ToolTip.text: "Document tools"
                Menu {
                    id: documentTools
                    width: Math.min(280, win.width - 32)
                    x: parent.width - width
                    y: parent.height
                    MenuItem { text: "New note"; onTriggered: win.requestNewDocument() }
                    MenuItem { text: "Open notes"; onTriggered: win.openNoteLibrary() }
                    MenuItem { text: "Note details…"; onTriggered: lectureDialog.open() }
                    MenuSeparator { }
                    MenuItem { text: "Find LaTeX"; onTriggered: win.openCommandFinder() }
                    MenuItem { text: "Custom snippets…"; onTriggered: snippetManager.openManager() }
                    MenuItem { text: "Draw a figure…"; onTriggered: win.openFigureEditor() }
                    MenuItem { text: "Lecture slides…"; onTriggered: win.togglePdf() }
                    MenuSeparator { }
                    MenuItem { text: "View: " + win.displayModeName(); onTriggered: win.cycleDisplayMode() }
                    Menu {
                        title: "Stavningskontroll: " + (win.spellLanguage === "sv" ? "Svenska" : "English")
                        MenuItem {
                            text: "Kontrollera stavning"; checkable: true; checked: win.spellcheckEnabled
                            onTriggered: { win.recordHistory(); win.spellcheckEnabled = !win.spellcheckEnabled; win.changed() }
                        }
                        MenuSeparator { }
                        MenuItem { text: "Svenska"; checkable: true; checked: win.spellLanguage === "sv"; onTriggered: win.setDocumentSpellingLanguage("sv") }
                        MenuItem { text: "English"; checkable: true; checked: win.spellLanguage === "en"; onTriggered: win.setDocumentSpellingLanguage("en") }
                        MenuSeparator { }
                        MenuItem {
                            text: "Återställ ignorerade ord"; enabled: win.spellingIgnored.length > 0
                            onTriggered: { win.recordHistory(); win.spellingIgnored = []; win.changed() }
                        }
                        MenuItem { text: documentEditor.spellingError; visible: text.length > 0; enabled: false }
                    }
                    MenuItem { text: "Font and spacing…"; onTriggered: fontDialog.open() }
                    MenuItem { text: "Export…"; onTriggered: exportMenu.open() }
                    MenuSeparator { }
                    MenuItem { text: "LaTeX guide"; onTriggered: win.openGuide() }
                    MenuItem { text: "Keyboard help"; onTriggered: helpDialog.open() }
                }
            }

        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Qt.rgba(win.textColor.r, win.textColor.g, win.textColor.b, 0.08)
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Item {
                id: notePane
                objectName: "notePane"
                visible: !win.pdfOpen || win.widePdfSplit
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: win.widePdfSplit ? win.width * 0.54 : win.width

                DocumentEditor {
                    id: documentEditor
                    objectName: "documentEditor"
                    anchors.fill: parent
                    backendApi: backend
                    spellLanguage: win.spellLanguage
                    spellcheckEnabled: win.spellcheckEnabled
                    spellingIgnored: win.spellingIgnored
                    writingWidth: win.pageWidth
                    fontFamily: win.editorFont
                    fontSize: win.editorSize
                    textColor: win.textColor
                    selectionColor: backend.themeSelection
                    displayMode: win.displayMode
                    completionVisible: inlineCompletion.visible && inlineCompletionResults.count > 0 && completionPrefix.length > 0
                    Keys.priority: Keys.BeforeItem
                    Keys.onTabPressed: function(event) {
                        if (documentEditor.completionVisible) win.acceptInlineCompletion()
                        else win.handleSnippetTab(documentEditor, activeRow)
                        event.accepted = true
                    }
                    Keys.onBacktabPressed: function(event) {
                        win.retreatSnippet(documentEditor, activeRow)
                        event.accepted = true
                    }
                    onEditStarted: function(separateUndo) { win.beginDocumentEdit(separateUndo) }
                    onRowsEdited: function(rows) {
                        win.editingDocument = true
                        win.syncingDocument = true
                        for (var i = 0; i < rows.length; ++i) {
                            var row = rows[i]
                            if (i >= lines.count) lines.append(win.makeLine(row.source, row.kind, row.label, row.asset, row.slide, row.mode))
                            else {
                                var old = lines.get(i)
                                if (old.source !== row.source || old.kind !== row.kind || old.label !== row.label
                                        || old.asset !== row.asset || old.slide !== row.slide || old.mode !== row.mode)
                                    lines.set(i, win.makeLine(row.source, row.kind, row.label, row.asset, row.slide, row.mode))
                            }
                        }
                        if (lines.count > rows.length) lines.remove(rows.length, lines.count - rows.length)
                        win.activeIndex = activeRow
                        win.syncingDocument = false
                        win.updateSnippetText(documentEditor, activeRow)
                        win.changed()
                        win.editingDocument = false
                    }
                    onCursorPositionChanged: {
                        if (!win.syncingDocument) {
                            win.activeIndex = activeRow
                            win.updateSnippetCursor(documentEditor, activeRow)
                            win.updateInlineCompletion()
                        }
                    }
                    onTabPressed: function(backward) {
                        if (!backward && documentEditor.completionVisible) win.acceptInlineCompletion()
                        else if (backward) win.retreatSnippet(documentEditor, activeRow)
                        else win.handleSnippetTab(documentEditor, activeRow)
                    }
                    onBlockBreakRequested: win.documentBlockBreak()
                    onInsertRowRequested: win.insertLineAfter(activeRow)
                    onUndoRequested: win.undoDocument()
                    onRedoRequested: win.redoDocument()
                    onEditModeRequested: win.displayMode = 0
                    onCompletionMove: function(direction) {
                        inlineSuggestions.currentIndex = Math.max(0, Math.min(inlineSuggestions.count - 1,
                                                                   inlineSuggestions.currentIndex + direction))
                    }
                    onCompletionDismissed: inlineCompletion.close()
                    onSpellingMenuRequested: function(word, suggestions, loading, x, y) {
                        if (!loading && !spellingMenu.visible) return
                        spellingMenu.word = word
                        spellingMenu.suggestions = suggestions
                        spellingMenu.loadingSuggestions = loading
                        if (loading) spellingMenu.popup(x, y + 12)
                    }
                    onSpellingDismissed: spellingMenu.close()
                    onSpellingIgnoreRequested: function(word) { win.ignoreSpellingWord(word) }
                    onContextMenuRequested: function(row, x, y) {
                        win.activeIndex = row
                        documentRowMenu.popup(x, y)
                    }
                }
                Menu {
                    id: spellingMenu
                    objectName: "spellingMenu"
                    parent: documentEditor
                    width: 260
                    property string word: ""
                    property var suggestions: []
                    property bool loadingSuggestions: false
                    MenuItem { text: spellingMenu.word; enabled: false }
                    MenuItem {
                        text: spellingMenu.loadingSuggestions ? "Söker förslag…" : "Inga förslag"
                        visible: spellingMenu.suggestions.length === 0
                        height: visible ? implicitHeight : 0
                        enabled: false
                    }
                    Instantiator {
                        model: spellingMenu.suggestions
                        delegate: MenuItem {
                            required property string modelData
                            objectName: "spellingSuggestion"
                            text: modelData
                            onTriggered: documentEditor.correctSpelling(text)
                        }
                        onObjectAdded: function(index, object) { spellingMenu.insertItem(index + 2, object) }
                        onObjectRemoved: function(index, object) { spellingMenu.removeItem(object) }
                    }
                    MenuSeparator { }
                    MenuItem { text: "Ignorera ordet i dokumentet"; onTriggered: documentEditor.ignoreSpelling() }
                    onClosed: documentEditor.forceActiveFocus()
                }
                DropArea {
                    anchors.fill: parent
                    onDropped: function(drop) {
                        if (drop.urls && drop.urls.length)
                            win.addImportedImage(backend.importAsset(win.documentPath, drop.urls[0]))
                    }
                }
                ScrollBar {
                    objectName: "documentScrollBar"
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    orientation: Qt.Vertical
                    size: Math.min(1, documentEditor.height / documentEditor.contentHeight)
                    position: documentEditor.scrollY / documentEditor.contentHeight
                    active: hovered || pressed || documentScrollLinger.running
                    onPositionChanged: if (pressed) documentEditor.scrollY = position * documentEditor.contentHeight
                }
                Text {
                    visible: documentEditor.errorHint.length > 0 && !historyTimer.running
                    anchors.left: parent.left
                    anchors.leftMargin: win.pageMargin
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 8
                    width: win.pageWidth
                    text: documentEditor.errorHint
                    color: "#b79057"
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }
                Menu {
                    id: documentRowMenu
                    MenuItem { text: "Row type…"; onTriggered: rowTypePopup.open() }
                    MenuItem { objectName: "editFigureCaptionMenuItem"; height: visible ? implicitHeight : 0; text: "Edit figure caption…"; visible: win.activeIndex >= 0 && win.activeIndex < lines.count && lines.get(win.activeIndex).kind === "image"; onTriggered: win.editImageCaption(win.activeIndex) }
                    MenuItem { objectName: "replaceFigureMenuItem"; height: visible ? implicitHeight : 0; text: "Replace figure…"; visible: win.activeIndex >= 0 && win.activeIndex < lines.count && lines.get(win.activeIndex).kind === "image"; onTriggered: win.replaceImage(win.activeIndex) }
                    MenuSeparator { }
                    MenuItem { text: "Move up"; onTriggered: win.moveActiveRow(-1) }
                    MenuItem { text: "Move down"; onTriggered: win.moveActiveRow(1) }
                    MenuItem { text: "Duplicate"; onTriggered: win.duplicateActiveRow() }
                    MenuItem { text: "Delete"; onTriggered: win.deleteActiveRow() }
                }
            }

            Rectangle {
                visible: win.widePdfSplit
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                color: Qt.rgba(win.textColor.r, win.textColor.g, win.textColor.b, 0.12)
            }

            Item {
                id: pdfPane
                objectName: "pdfPane"
                visible: win.pdfOpen
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: win.widePdfSplit ? win.width * 0.46 : win.width

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        Layout.leftMargin: 8
                        Layout.rightMargin: 8
                        spacing: 2

                        ToolButton {
                            text: "‹"
                            enabled: pdfView.currentPage > 0
                            onClicked: pdfView.goToPage(pdfView.currentPage - 1)
                        }
                        Text {
                            text: lecturePdf.pageCount > 0
                                  ? (pdfView.currentPage + 1) + " / " + lecturePdf.pageCount
                                  : "Loading…"
                            color: win.mutedColor
                            font.family: win.editorFont
                            font.pixelSize: 12
                        }
                        ToolButton {
                            text: "›"
                            enabled: pdfView.currentPage + 1 < lecturePdf.pageCount
                            onClicked: pdfView.goToPage(pdfView.currentPage + 1)
                        }
                        Item { Layout.fillWidth: true }
                        ToolButton {
                            text: "↳"
                            onClicked: win.linkActiveRowToSlide()
                            ToolTip.visible: hovered
                            ToolTip.text: "Link active row to this slide"
                        }
                        ToolButton {
                            text: "▣"
                            onClicked: win.captureSlide()
                            ToolTip.visible: hovered
                            ToolTip.text: "Capture visible slide area into the note"
                        }
                        ToolButton {
                            text: "…"
                            onClicked: slidesDialog.open()
                            ToolTip.visible: hovered
                            ToolTip.text: "Choose another PDF"
                        }
                        ToolButton {
                            text: "×"
                            onClicked: win.pdfOpen = false
                            ToolTip.visible: hovered
                            ToolTip.text: "Hide slides"
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Qt.rgba(win.textColor.r, win.textColor.g,
                                       win.textColor.b, 0.1)
                    }

                    PdfMultiPageView {
                        id: pdfView
                        objectName: "pdfView"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        document: lecturePdf
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        recoveryId = backend.newRecoveryId()
        if (startupPath.length) {
            var startupData = backend.loadDocument(startupPath)
            if (!startupData.error) loadData(startupData, startupPath)
            else {
                startNewDocument()
                messageDialog.message = startupData.error
                messageDialog.open()
            }
        } else {
            if (!loadStartupRecovery()) {
                loading = true
                lines.append(makeLine(""))
                activeIndex = 0
                loading = false
                resetHistory(true)
                Qt.callLater(renderAll)
            }
        }
        var environment = backend.environmentStatus()
        if (!environment.ready) {
            messageDialog.message = "Math tools missing: " + environment.missing.join(", ")
                    + ". Install them before taking notes."
            messageDialog.open()
        }
    }
}
