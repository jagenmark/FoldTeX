#include "backend.h"
#include "documenteditor.h"
#include "latexsyntax.h"
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QProcess>
#include <QSvgRenderer>
#include <QPainter>
#include <QSignalSpy>
#include <cstdio>

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    app.setApplicationName("foldtex-document-editor-test");
    app.setOrganizationName("JagenmarkTests");
    QQuickStyle::setStyle("Material");
    if (argc != 2) return 2;
    auto check = [](bool success, const QString &message) {
        if (!success) std::fprintf(stderr, "FAIL: %s\n", qPrintable(message));
        return success;
    };
    Backend backend;
    // Source instrumentation must leave the actual TeX appearance unchanged.
    // Compare the rasterized result with a fresh, uninstrumented TeX render.
    for (const QString &formula : {QString("x^2+\\frac{a+b}{c}"), QString("\\sqrt[3]{x}+\\sin x"),
            QString("\\sum\\limits_{k=0}^{n} k"), QString("f''(x)"),
            QString("\\left\\{x\\in\\mathbb{R}\\middle|x>0\\right\\}"),
            QString("\\begin{aligned}a&=b\\\\c&=d\\end{aligned}")}) {
        const auto rendered = backend.render(formula, "#eeeeee", 17);
        if (!check(rendered.value("error").toString().isEmpty(), "Mapping render failed: " + formula)) return 1;
        const QString svgPath = QUrl(rendered.value("url").toString()).toLocalFile();
        QFile generated(QFileInfo(svgPath).dir().filePath("line.tex"));
        if (!generated.open(QIODevice::ReadOnly)) return 1;
        QString reference = QString::fromUtf8(generated.readAll());
        const QString prefix = "\\color{foldtext}\\(\\displaystyle ";
        const int from = reference.indexOf(prefix) + prefix.size();
        const int to = reference.lastIndexOf("\\)");
        reference.replace(from, to - from, LatexSyntax::renderBody(formula));
        QTemporaryDir comparison;
        QFile plain(comparison.filePath("plain.tex"));
        if (!plain.open(QIODevice::WriteOnly)) return 1;
        plain.write(reference.toUtf8()); plain.close();
        auto run = [&](const QString &command, const QStringList &arguments) {
            QProcess process; process.setWorkingDirectory(comparison.path()); process.start(command, arguments);
            return process.waitForFinished(10000) && process.exitCode() == 0;
        };
        if (!check(run("latex", {"-interaction=nonstopmode", "-halt-on-error", "-no-shell-escape", "plain.tex"})
                   && run("dvisvgm", {"--no-fonts", "--exact-bbox", "--bbox=min", "--output=plain.svg", "plain.dvi"}),
                   "Uninstrumented reference failed")) return 1;
        QSvgRenderer actual(svgPath), expected(comparison.filePath("plain.svg"));
        if (!check(qAbs(actual.viewBoxF().width() - expected.viewBoxF().width()) < 0.02
                   && qAbs(actual.viewBoxF().height() - expected.viewBoxF().height()) < 0.02,
                   "Source mapping changed TeX geometry: " + formula)) return 1;
        const QSize size = (expected.viewBoxF().size() * 4).toSize();
        QImage actualImage(size, QImage::Format_ARGB32), expectedImage(size, QImage::Format_ARGB32);
        actualImage.fill(Qt::transparent); expectedImage.fill(Qt::transparent);
        { QPainter painter(&actualImage); actual.render(&painter); }
        { QPainter painter(&expectedImage); expected.render(&painter); }
        if (!check(actualImage == expectedImage, "Source mapping changed TeX glyphs or spacing: " + formula)) return 1;
    }
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings, [](const QList<QQmlError> &warnings) {
        for (const auto &warning : warnings) std::fprintf(stderr, "QML: %s\n", qPrintable(warning.toString()));
    });
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])));
    if (!check(!engine.rootObjects().isEmpty(), "Main.qml did not load")) return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    auto *editor = window->findChild<DocumentEditor *>("documentEditor");
    if (!check(editor, "Continuous editor missing")) return 1;
    auto load = [&](const QStringList &sources) {
        QVariantList rows;
        for (const auto &source : sources) rows.append(QVariantMap{{"source", source}, {"mode", "latex"}});
        QVariantMap data{{"format", "foldtex-3"}, {"lines", rows}};
        QMetaObject::invokeMethod(window, "loadData", Q_ARG(QVariant, data), Q_ARG(QVariant, QString()));
        QTest::qWait(20); editor->editRow(0, 0); editor->forceActiveFocus();
    };
    auto key = [&](Qt::Key k, Qt::KeyboardModifiers modifiers = Qt::NoModifier) { QTest::keyClick(window, k, modifiers); QTest::qWait(10); };
    auto type = [&](const QString &value) { for (QChar c : value) QTest::keyClick(window, c.toLatin1()); };
    auto undo = [&] { QMetaObject::invokeMethod(window, "undoDocument"); QTest::qWait(20); };
    auto redo = [&] { QMetaObject::invokeMethod(window, "redoDocument"); QTest::qWait(20); };
    const QString mixedLegacy = QString::fromUtf8("Om vi sätter in $n = 3k$ i ursprungsuttrycket får vi\n$(4n+3)^2=(4(3k)+3)^2=$\n$(12k+3)^2=144k^2+72k+9$.\nVi undersöker sedan");
    for (const QString &oldMode : {QString("math"), QString("text"), QString("auto")}) {
        const QVariantList oldRows{QVariantMap{{"source", mixedLegacy}, {"mode", oldMode}},
            QVariantMap{{"source", "Continue"}, {"mode", "text"}}};
        QVariantMap oldData{{"format", "foldtex-3"}, {"lines", oldRows}};
        QMetaObject::invokeMethod(window, "loadData", Q_ARG(QVariant, oldData), Q_ARG(QVariant, QString()));
        editor->editRow(1, 2);
        if (!check(QTest::qWaitFor([&] { return editor->renderedCount() == 3; }, 12000), "Old row mode still blocks mixed formulas: " + oldMode)) return 1;
        if (oldMode == "math") window->grabWindow().save("/tmp/foldtex-rowmode-fixed.png");
        if (!check(editor->rows()[0].toMap()["source"] == mixedLegacy && editor->rows()[0].toMap()["mode"] == "latex", "Migration changed mixed source")) return 1;
        QTemporaryDir migration;
        const QVariantMap migrated{{"format", "foldtex-3"}, {"lines", editor->rows()}};
        if (!check(backend.saveDocumentData(migration.filePath("note.foldtex"), migrated)
                   && backend.loadDocument(migration.filePath("note.foldtex")).value("lines") == migrated.value("lines"), "Migration is not stable after save/reopen")) return 1;
        if (!check(backend.exportDocumentPdf(migration.filePath("note.pdf"), migrated).value("error").toString().isEmpty(), "Migrated mixed note PDF failed")) return 1;
    }
    const QVariantList oldEquations{QVariantMap{{"source", "x+y"}, {"mode", "math"}},
        QVariantMap{{"source", "(4n+3)^2 = 16n^2+24n+9"}, {"mode", "auto"}},
        QVariantMap{{"source", "\\usepackage{amsmath}"}, {"mode", "auto"}}};
    const auto migratedEquations = backend.migrateRowModes(oldEquations);
    if (!check(migratedEquations[0].toMap()["source"] == "\\[\nx+y\n\\]"
               && migratedEquations[1].toMap()["source"].toString().startsWith("\\[")
               && backend.migrateRowModes(migratedEquations) == migratedEquations, "Bare equation migration lost formulas or added duplicate delimiters")) return 1;
    load({"Price = 100", "Continue"}); editor->editRow(1, 1); QTest::qWait(400);
    if (!check(editor->renderedCount() == 0 && editor->rows()[0].toMap()["mode"] == "latex", "New prose was guessed to be math")) return 1;
    load({""}); type("$");
    if (!check(editor->text() == "$$" && editor->cursorPosition() == 1, "Dollar did not create an inline pair")) return 1;
    type("$");
    if (!check(editor->text() == "$$$$" && editor->cursorPosition() == 2, "Second dollar did not upgrade to display delimiters")) return 1;
    type("x$$");
    if (!check(editor->text() == "$$x$$" && editor->cursorPosition() == 5, "Typing display closers duplicated them")) return 1;
    load({""}); type("$"); type("x$");
    if (!check(editor->text() == "$x$" && editor->cursorPosition() == 3, "Inline closing dollar was duplicated")) return 1;
    for (int at : {0, 1, 9}) {
        load({"$ 3 + 3 $"}); editor->editRow(0, at); type("$");
        if (!check(editor->text() == "$$ 3 + 3 $$" && editor->cursorPosition() == (at == 9 ? 11 : 2), "Existing inline dollars did not upgrade together")) return 1;
        undo();
        if (!check(editor->text() == "$ 3 + 3 $" && editor->cursorPosition() == at, "Upgrade undo lost source or cursor")) return 1;
        redo();
        if (!check(editor->text() == "$$ 3 + 3 $$", "Upgrade redo failed")) return 1;
    }
    load({"$ 3 + 3 $"}); editor->editRow(0, 8); type("$");
    if (!check(editor->text() == "$ 3 + 3 $" && editor->cursorPosition() == 9, "Typing the existing inline closer duplicated it")) return 1;
    for (int at : {0, 1, 2, 6}) {
        load({"$$x+1$$"}); editor->editRow(0, at); type("$");
        if (!check(editor->text() == "$$x+1$$", "Editing double delimiter created a third dollar")) return 1;
    }
    for (const QString &source : {QString("$x+1"), QString("$$x+1")}) {
        load({source}); editor->editRow(0); type(source.startsWith("$$") ? "$$" : "$");
        if (!check(editor->text() == (source.startsWith("$$") ? "$$x+1$$" : "$x+1$"), "Unfinished formula got an extra empty pair")) return 1;
    }
    load({"Before after"}); editor->editRow(0, 7); type("$$");
    if (!check(editor->text() == "Before $$$$after" && editor->cursorPosition() == 9, "Empty pair upgrade in surrounding text failed")) return 1;
    load({"$a+b$"}); editor->editRow(0, 3); type("$");
    if (!check(editor->text() == "$a+$b$", "Dollar inside math was doubled")) return 1;
    load({"$a$ and $b$"}); editor->editRow(0, 9); type("$");
    if (!check(editor->text() == "$a$ and $$b$$", "Upgrading a later formula changed its neighbor")) return 1;
    load({"$a$$b$"}); editor->editRow(0, 3); type("$");
    if (!check(editor->text() == "$a$$$b$$", "Adjacent formula opening matched the wrong closing delimiter")) return 1;
    load({"$a% dollar $ in comment\n+b$"}); editor->editRow(0, 1); type("$");
    if (!check(editor->text() == "$$a% dollar $ in comment\n+b$$", "Comment dollar confused the matching pair")) return 1;
    load({"$x\\$+y$"}); editor->editRow(0, 1); type("$");
    if (!check(editor->text() == "$$x\\$+y$$", "Escaped dollar confused the matching pair")) return 1;
    for (int at : {1, 2, 6, 7}) {
        load({"$$x+1$$"}); editor->editRow(0, at); key(Qt::Key_Backspace);
        if (!check(editor->text() == "$x+1$", QString("Backspace at %1 left mismatched double delimiters: %2").arg(at).arg(editor->text()))) return 1;
        undo(); if (!check(editor->text() == "$$x+1$$", "Delimiter downgrade undo failed")) return 1;
    }
    for (int at : {0, 1, 5, 6}) {
        load({"$$x+1$$"}); editor->editRow(0, at); key(Qt::Key_Delete);
        if (!check(editor->text() == "$x+1$", "Delete left mismatched double delimiters")) return 1;
    }
    for (const auto &pair : QList<QPair<QString, QString>>{{"\\(", "\\)"}, {"\\[", "\\]"}, {"{", "}"}, {"(", ")"}}) {
        load({""}); type(pair.first);
        if (!check(editor->text() == pair.first + pair.second && editor->cursorPosition() == pair.first.size(), "Delimiter pairing failed: " + pair.first)) return 1;
        type("x" + pair.second);
        if (!check(editor->text() == pair.first + "x" + pair.second, "Delimiter closer duplicated: " + editor->text())) return 1;
        load({""}); type(pair.first); key(Qt::Key_Backspace);
        if (!check(editor->text().isEmpty(), "Backspace did not remove an empty pair: " + editor->text())) return 1;
    }
    load({""}); type("$$"); key(Qt::Key_Backspace);
    if (!check(editor->text().isEmpty(), "Backspace did not remove an empty display pair")) return 1;
    load({"selected"}); editor->select(0, 8); type("$");
    if (!check(editor->text() == "$selected$" && editor->selectedText() == "selected", "Dollar did not wrap selected source")) return 1;
    undo();
    if (!check(editor->text() == "selected" && editor->selectedText() == "selected", "Undo did not restore wrapped selection")) return 1;
    load({""}); type("\\$");
    if (!check(editor->text() == "\\$", "Escaped dollar was paired")) return 1;
    load({"% comment "}); editor->editRow(0); type("$");
    if (!check(editor->text() == "% comment $", "Dollar in comment was paired")) return 1;
    load({""});
    QKeyEvent altGrDollar(QEvent::KeyPress, Qt::Key_4, Qt::ControlModifier | Qt::AltModifier | Qt::GroupSwitchModifier, "$");
    QCoreApplication::sendEvent(window, &altGrDollar);
    if (!check(editor->text() == "$$", "AltGr dollar was not accepted")) return 1;
    QVariantList unfinishedSetup{QVariantMap{{"source", "\\newcommand{\\RR}"}}};
    if (!check(!LatexSyntax::documentPreamble(unfinishedSetup).complete, "An unfinished macro definition was treated as a complete preamble")) return 1;
    for (const QString &incomplete : {QString("\\frac{a}{"), QString("$\\frac{a}{b$"),
            QString("\\begin{align}a=b"), QString("\\[x+1"), QString("x^")}) {
        const auto parsed = LatexSyntax::mathSpans(incomplete);
        if (!check(parsed.size() == 1 && !parsed.first().complete, "Incomplete math was considered ready: " + incomplete)) return 1;
    }
    for (const QString &complete : {QString("$\\{a\\}$"), QString("\\begin{align}a=b\\end{align}"),
            QString("$a% ignored }\n+b$"), QString("\\frac{a}{b}")}) {
        const auto parsed = LatexSyntax::mathSpans(complete);
        if (!check(parsed.size() == 1 && parsed.first().complete, "Complete math was blocked: " + complete)) return 1;
    }
    const QString legacyMatrix = "\\begin{bmatrix}1&2\\\\3&4\\end{bmatrix} + x";
    if (!check(LatexSyntax::mathSpans(legacyMatrix).first().body == legacyMatrix,
               "Legacy matrix expression lost its surrounding mathematics")) return 1;
    load({"For every x this holds", "Next paragraph"});
    editor->setCursorPosition(10);
    key(Qt::Key_Return);
    if (!check(editor->rows().size() == 4 && editor->rows()[0].toMap()["source"] == "For every "
               && editor->text() == "x this holds" && editor->cursorPosition() == 0,
               "Enter did not split at the cursor: " + editor->sourceText())) return 1;
    undo();
    if (!check(editor->text() == "For every x this holds" && editor->cursorPosition() == 10 && editor->hasActiveFocus(), "Undo lost source, cursor or focus")) return 1;
    redo();
    if (!check(editor->text() == "x this holds" && editor->cursorPosition() == 0 && editor->hasActiveFocus(), "Redo lost cursor/focus")) return 1;
    load({"Alpha beta", "Gamma delta"});
    editor->selectSource(6, 16);
    if (!check(editor->selectedText() == "beta\nGamma", "Cross-paragraph selection incorrect")) return 1;
    key(Qt::Key_C, Qt::ControlModifier);
    if (!check(QGuiApplication::clipboard()->text() == "beta\nGamma", "Copy did not preserve selected source across paragraphs")) return 1;
    key(Qt::Key_Backspace);
    if (!check(editor->sourceText().startsWith("Alpha  delta"), "Cross-paragraph delete did not join text")) return 1;
    undo();
    if (!check(editor->selectedText() == "beta\nGamma", QString("Undo did not restore selection: <%1> cursor=%2 anchor=%3 text=<%4>").arg(editor->selectedText()).arg(editor->sourceCursor()).arg(editor->sourceAnchor()).arg(editor->sourceText()))) return 1;
    editor->editRow(0, 2); key(Qt::Key_Down);
    if (!check(editor->activeRow() == 1 && editor->cursorPosition() == 2, "Vertical navigation lost column")) return 1;
    editor->editRow(1, 0); key(Qt::Key_Backspace);
    if (!check(editor->text() == "Alpha betaGamma delta" && editor->cursorPosition() == 10,
               "Backspace at paragraph start did not join paragraphs")) return 1;
    const QString aligned = "\\begin{align}\na &= b \\\\\nc &= d\n\\end{align}";
    load({""}); editor->pasteText("Before\n" + aligned + "\nAfter");
    if (!check(editor->rows().size() == 3 && editor->rows()[1].toMap()["source"] == aligned, "Pasted environment was split")) return 1;
    editor->editRow(1, 20); key(Qt::Key_Return);
    if (!check(editor->rows().size() == 3, "Enter inside environment split the environment")) return 1;
    load({"Let $x+1$ and $y^2$ be given.", "Continue here"}); editor->editRow(1, 3);
    for (int i = 0; i < 150 && editor->renderedCount() < 2; ++i) QTest::qWait(100);
    if (!check(editor->renderedCount() == 2 && editor->rows()[0].toMap()["source"] == "Let $x+1$ and $y^2$ be given.", "Mixed formulas did not render without source changes")) return 1;
    editor->editRow(0, 6);
    if (!check(editor->renderedCount() == 1, "Editing one formula exposed unrelated math")) return 1;
    key(Qt::Key_Return, Qt::ShiftModifier);
    if (!check(editor->text().startsWith("Let $") && editor->text().endsWith("and $y^2$ be given.")
               && editor->text().contains("\\begin{aligned}"), "Shift+Enter changed text outside active formula")) return 1;
    const auto spans = LatexSyntax::mathSpans(editor->text());
    auto render = backend.render(LatexSyntax::renderBody(spans.first().body), "#eeeeee", 17);
    if (!check(render.value("error").toString().isEmpty(), "Shift+Enter inline math does not compile: " + render.value("error").toString())) return 1;
    load({"The fraction "}); editor->editRow(0);
    type("\\fra"); QTest::qWait(50); key(Qt::Key_Tab);
    if (!check(editor->text().contains("$\\frac{") && !editor->selectedText().isEmpty(), "Completion did not insert math snippet and select its argument: " + editor->text())) return 1;
    type("a"); key(Qt::Key_Tab); type("b");
    if (!check(editor->text().contains("\\frac{a}{b}"), "Snippet argument navigation failed: " + editor->text())) return 1;
    key(Qt::Key_Backtab, Qt::ShiftModifier);
    if (!check(editor->selectedText() == "a", "Shift+Tab did not return to the previous snippet argument")) return 1;
    load({"A "}); editor->editRow(0); type("\\textb"); QTest::qWait(30); key(Qt::Key_Tab); type("bold");
    if (!check(editor->text() == "A \\textbf{bold}", "Text completion inserted math delimiters or lost its argument: " + editor->text())) return 1;
    key(Qt::Key_Tab);
    if (!check(editor->cursorPosition() == editor->text().size(), "Final snippet Tab did not leave the closing brace")) return 1;
    load({""}); type("\\frac{a}{b}");
    if (!check(editor->text() == "\\frac{a}{b}", "Automatic braces duplicated closing delimiters")) return 1;
    load({""}); type("\\begin{align}");
    if (!check(editor->text() == "\\begin{align}\n\n\\end{align}" && editor->cursorPosition() == 14,
               "Environment completion lost its closing tag or writing position: " + editor->text())) return 1;
    type("a=b"); key(Qt::Key_Return);
    if (!check(editor->rows().first().toMap()["source"].toString().contains("a=b\n\n\\end{align}"),
               "Typing inside completed environment split the closing tag into another paragraph")) return 1;
    load({"\\begin{align}\nx=y\n\\end{align}"}); editor->editRow(0, 12); type("}");
    if (!check(editor->text().count("\\end{align}") == 1, "Completion duplicated an existing environment ending")) return 1;
    load({"\\begin{align}\n\\end{align}"}); editor->editRow(0, 14); type("\\begin{aligned}");
    if (!check(editor->text().contains("\\end{aligned}") && editor->text().count("\\end{align}") == 1,
               "Nested environment completion damaged the outer ending")) return 1;
    load({"\\begin{align}\n% \\end{align}"}); editor->editRow(0, 12); type("}");
    if (!check(editor->text() == "\\begin{align}\n\n\\end{align}\n% \\end{align}",
               "Commented environment ending prevented completion")) return 1;
    load({"\\frac{a}{", "Continue"}); editor->editRow(1, 1); QTest::qWait(400);
    if (!check(editor->pendingRenderCount() == 0 && editor->renderedCount() == 0 && editor->errorHint().isEmpty(),
               "Incomplete expression was sent to the renderer")) return 1;
    for (const QString &display : {QString("\\[x^2\\]"), QString("\\begin{equation}x^2\\end{equation}")}) {
        const QString source = "Before " + display + " after.";
        load({source}); editor->editRow(0, 2);
        for (int i = 0; i < 100 && editor->renderedCount() < 1; ++i) QTest::qWait(50);
        const qreal beforeY = editor->cursorRectangle().y();
        editor->editRow(0, source.size() - 2);
        if (!check(editor->renderedCount() == 1 && editor->cursorRectangle().y() > beforeY + 25
                   && editor->text() == source, "Display math did not separate surrounding prose without changing source")) return 1;
        window->grabWindow().save("/tmp/foldtex-display-audit.png");
    }
    // Display equations center within the writing area and remain reachable
    // with arrow keys from either side, even at a column outside the image.
    for (const QString &formula : {QString("$$\\frac{144k^2+72k+9}{9}$$"),
            QString("\\[\\frac{a}{\\frac{b}{c}}\\]"),
            QString("\\begin{align}a&=b\\\\c&=d\\end{align}")}) {
        load({"Above", formula, "Below"}); editor->editRow(2, 0);
        if (!check(QTest::qWaitFor([&] { return editor->renderedCount() == 1; }, 12000), "Display did not render for navigation")) return 1;
        auto equationBounds = [&] {
            QRectF bounds;
            for (int i = 6; i < 6 + formula.size(); ++i) {
                const auto glyph = editor->renderedSourceRectangle(i);
                if (glyph.isValid()) bounds = bounds.isValid() ? bounds.united(glyph) : glyph;
            }
            return bounds;
        };
        if (!check(equationBounds().isValid() && qAbs(equationBounds().center().x() - editor->width()/2) < 4,
                   "Display formula is not centered: " + formula)) return 1;
        window->grabWindow().save("/tmp/foldtex-centered-equation.png");
        key(Qt::Key_Up);
        if (!check(editor->activeRow() == 1 && editor->renderedCount() == 0, "Up did not enter rendered formula")) return 1;
        editor->editRow(0, 0);
        if (!check(QTest::qWaitFor([&] { return editor->renderedCount() == 1; }), "Formula did not refold")) return 1;
        key(Qt::Key_Down);
        if (!check(editor->activeRow() == 1 && editor->renderedCount() == 0, "Down did not enter rendered formula")) return 1;
        editor->editRow(2, 0);
        key(Qt::Key_Up, Qt::ShiftModifier);
        if (!check(editor->activeRow() == 1 && !editor->selectedText().isEmpty(), "Shift+Up did not select into formula")) return 1;
        if (!check(editor->rows()[1].toMap()["source"] == formula, "Navigation or centering changed source")) return 1;
    }
    load({"Before $$x^2$$ after", "Continue"}); editor->editRow(1, 0);
    if (!check(QTest::qWaitFor([&] { return editor->renderedCount() == 1; }), "Mixed display did not fold")) return 1;
    const qreal priorWidth = window->width(); window->setWidth(620); QTest::qWait(100);
    const QRectF centeredGlyph = editor->renderedSourceRectangle(9);
    if (!check(centeredGlyph.isValid() && qAbs(centeredGlyph.center().x() - editor->width()/2) < 12,
               "Display centering did not follow resize")) return 1;
    editor->editRow(0, 0);
    const qreal proseLeft = editor->cursorRectangle().x();
    editor->editRow(0, 15);
    if (!check(editor->cursorRectangle().x() < editor->width()/3 && proseLeft < editor->width()/3,
               "Prose beside display became centered")) return 1;
    key(Qt::Key_Up);
    if (!check(editor->cursorPosition() >= 7 && editor->cursorPosition() <= 14 && editor->renderedCount() == 0,
               "Up from prose in same paragraph did not open display")) return 1;
    window->setWidth(priorWidth); QTest::qWait(50);
    load({"Before $x+1$ after.", "Next paragraph"});
    editor->editRow(0, 16);
    for (int i = 0; i < 100 && editor->renderedCount() < 1; ++i) QTest::qWait(50);
    const QPointF textClick = editor->mapToScene(editor->cursorRectangle().center());
    editor->editRow(1, 0);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, textClick.toPoint());
    if (!check(editor->activeRow() == 0 && qAbs(editor->cursorPosition() - 16) <= 1, "Click did not place cursor in prose")) return 1;
    load({QString::fromUtf8("a😀b")}); editor->editRow(0, 3); key(Qt::Key_Backspace);
    if (!check(editor->text() == "ab", "Backspace damaged a Unicode character")) return 1;
    load({"The fraction $\\frac{a}{b}$"});
    QTemporaryDir temp;
    const QString mapped = "Before $a+\\frac{b}{c}+d$ after.";
    load({mapped, "Continue"}); editor->editRow(1, 3);
    for (int i = 0; i < 120 && editor->renderedCount() < 1; ++i) QTest::qWait(100);
    const int numerator = mapped.indexOf("{b}") + 1, denominator = mapped.indexOf("{c}") + 1;
    const QRectF numeratorBox = editor->renderedSourceRectangle(numerator);
    const QRectF denominatorBox = editor->renderedSourceRectangle(denominator);
    if (!check(numeratorBox.isValid() && denominatorBox.isValid() && numeratorBox.bottom() < denominatorBox.top(),
               "Fraction source map did not distinguish numerator and denominator")) return 1;
    for (int position : {numerator, denominator, int(mapped.indexOf("+d")) + 1}) {
        editor->editRow(1, 3);
        const QRectF box = editor->renderedSourceRectangle(position);
        const QPointF click(box.left() + box.width() * 0.2, box.center().y());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, editor->mapToScene(click).toPoint());
        if (!check(editor->cursorPosition() == position, QString("Formula click mapped to %1 instead of %2").arg(editor->cursorPosition()).arg(position))) return 1;
    }
    // Explicit prose breaks remain real LaTeX in copy/export and fold into
    // one visual break, whether the author also put a newline in the source.
    for (const QString &prose : {QString("First\\\\Second"), QString("First\\\\\nSecond"),
                                QString("First\\\\  \nSecond")}) {
        load({prose, "Continue"});
        editor->setDisplayMode(2);
        editor->selectSource(0, 0);
        const QRectF first = editor->cursorRectangle();
        const int second = prose.indexOf("Second");
        editor->selectSource(second, second);
        const QRectF next = editor->cursorRectangle();
        if (!check(next.y() > first.y() && next.y() - first.y() < first.height() * 1.7,
                   "Explicit prose break did not produce exactly one visual line: " + prose)) return 1;
        editor->selectSource(0, prose.size()); editor->copy();
        if (!check(QGuiApplication::clipboard()->text() == prose, "Copy changed explicit LaTeX breaks")) return 1;
        editor->setDisplayMode(1);
        editor->selectSource(second, second);
        if (!prose.contains('\n') && !check(qAbs(editor->cursorRectangle().y() - first.y()) < 1,
                                           "Source view hid explicit break syntax")) return 1;
        editor->setDisplayMode(0);
        if (!check(LatexSyntax::toTex(prose, "latex") == prose, "Export escaped or duplicated an explicit break")) return 1;
    }
    load({"First\\\\", "Second"});
    editor->setDisplayMode(2); editor->editRow(0, 0);
    const QRectF trailingStart = editor->cursorRectangle();
    editor->editRow(1, 0);
    if (!check(editor->cursorRectangle().y() - trailingStart.y() < trailingStart.height() * 1.7,
               "Trailing prose break duplicated the next paragraph boundary")) return 1;
    editor->setDisplayMode(0);
    const QString breakExample = "Om $n$ är ett heltal.\\\\\nNästa rad med \\textbf{fet\\\\stil}.\\\\";
    load({breakExample, "", "Fortsättning", "\\begin{align}a&=b\\\\c&=d\\end{align}"});
    editor->setDisplayMode(2);
    if (!check(QTest::qWaitFor([&] { return editor->renderedCount() == 2; }, 12000), "Prose break disrupted math rendering")) return 1;
    window->grabWindow().save("/tmp/foldtex-prose-breaks.png");
    const auto breakPdf = backend.exportDocumentPdf(temp.filePath("prose-breaks.pdf"), QVariantMap{{"lines", editor->rows()}});
    if (!check(breakPdf.value("error").toString().isEmpty(), "Explicit prose break PDF failed: " + breakPdf.value("error").toString())) return 1;
    if (!check(LatexSyntax::textLineBreaks("% ignored \\\\\nNext").isEmpty()
               && LatexSyntax::textLineBreaks("\\textbackslash{}").isEmpty(), "Literal/commented backslashes became breaks")) return 1;
    editor->setDisplayMode(0);
    const QString styled = "A \\textbf{bold \\emph{and italic}} plus \\texttt{code} and $x^2$.";
    load({styled, "Continue"}); editor->editRow(1, 2);
    for (int i = 0; i < 120 && editor->renderedCount() < 1; ++i) QTest::qWait(100);
    window->grabWindow().save("/tmp/foldtex-qol-formatting.png");
    if (!check(LatexSyntax::toTex(styled).contains("\\textbf{bold \\emph{and italic}}")
               && !LatexSyntax::toTex(styled).contains("textbackslash"), "Text formatting was escaped during export")) return 1;
    QVariantMap styledDocument{{"lines", editor->rows()}};
    auto styledPdf = backend.exportDocumentPdf(temp.filePath("styled.pdf"), styledDocument);
    if (!check(styledPdf.value("error").toString().isEmpty(), "Styled text PDF failed: " + styledPdf.value("error").toString())) return 1;
    const QStringList configured{"\\documentclass{report}", "\\usepackage{bm}", "\\newcommand{\\RR}{\\mathbb{R}}",
        "\\newcommand{\\hello}[1]{\\textbf{Hello #1}}", "\\begin{document}",
        "Take $x\\in\\RR$ and $\\bm{v}$.", "Greeting: \\hello{world}.", "Continue", "\\end{document}"};
    load(configured); editor->editRow(7, 2);
    for (int i = 0; i < 180 && editor->renderedCount() < 3; ++i) QTest::qWait(100);
    if (!check(editor->renderedCount() == 3, "Document packages/macros did not render in math and text")) return 1;
    const auto preambleSetup = LatexSyntax::documentPreamble(editor->rows());
    if (!check(preambleSetup.complete && preambleSetup.rows.contains(0) && preambleSetup.rows.contains(8)
               && preambleSetup.source.contains("\\usepackage{bm}") && preambleSetup.documentClass.contains("report"), "Preamble extraction lost source settings")) return 1;
    QVariantMap configuredDocument{{"format", "foldtex-3"}, {"lines", editor->rows()}};
    auto configuredPdf = backend.exportDocumentPdf(temp.filePath("configured.pdf"), configuredDocument);
    if (!check(configuredPdf.value("error").toString().isEmpty(), "Preamble PDF export failed: " + configuredPdf.value("error").toString())) return 1;
    if (!check(backend.saveDocumentData(temp.filePath("configured.foldtex"), configuredDocument)
               && backend.loadDocument(temp.filePath("configured.foldtex")).value("lines").toList().at(3).toMap()["source"] == configured.at(3),
               "Save/reopen lost macro declarations")) return 1;
    const auto one = backend.render("\\hello{world}", "#eeeeee", 17, preambleSetup.source, true);
    const auto two = backend.render("\\hello{world}", "#eeeeee", 17, QString(preambleSetup.source).replace("Hello", "Goodbye"), true);
    if (!check(one.value("url") != two.value("url") && two.value("error").toString().isEmpty(), "Preamble edits reused stale rendering")) return 1;
    load({"The fraction $\\frac{a}{b}$"});
    QVariantMap document{{"format", "foldtex-3"}, {"title", "Continuous editing"}, {"lines", editor->rows()}};
    const auto exported = backend.exportDocumentPdf(temp.filePath("mixed.pdf"), document);
    if (!check(exported.value("error").toString().isEmpty() && QFile::exists(temp.filePath("mixed.pdf")), "Mixed document PDF failed: " + exported.value("error").toString())) return 1;
    if (!check(backend.saveDocumentData(temp.filePath("note.foldtex"), document), "Save failed")) return 1;
    auto reopened = backend.loadDocument(temp.filePath("note.foldtex"));
    if (!check(reopened.value("lines").toList().first().toMap()["source"] == editor->rows().first().toMap()["source"], "Save/reopen changed source")) return 1;
    load({"A $x$ expression", ""});
    window->setProperty("displayMode", 1); editor->editRow(0, 0); type("New ");
    if (!check(editor->text().startsWith("New A"), "Source mode is not editable")) return 1;
    window->setProperty("displayMode", 0);
    QImage figure(140, 80, QImage::Format_ARGB32); figure.fill(Qt::darkCyan);
    const QString figurePath = temp.filePath("figure.png"); figure.save(figurePath);
    QVariantMap imageDocument{{"format", "foldtex-3"}, {"lines", QVariantList{
        QVariantMap{{"source", "Caption here"}, {"kind", "image"}, {"asset", figurePath}},
        QVariantMap{{"source", "After the figure"}}}}};
    QMetaObject::invokeMethod(window, "loadData", Q_ARG(QVariant, imageDocument), Q_ARG(QVariant, QString()));
    editor->editRow(0, 8); key(Qt::Key_Return, Qt::ShiftModifier);
    if (!check(editor->rows()[0].toMap()["kind"] == "image" && editor->rows()[0].toMap()["asset"] == figurePath
               && editor->text().contains('\n'), "Caption edit removed or replaced its figure")) return 1;
    undo();
    if (!check(editor->text() == "Caption here" && editor->rows()[0].toMap()["asset"] == figurePath, "Undo lost the figure caption or asset")) return 1;
    QStringList snippetNote;
    for (int i = 0; i < 90; ++i) snippetNote.append("Paragraph " + QString::number(i));
    snippetNote[40] = "$frac$";
    load(snippetNote); editor->editRow(40, 5); QTest::qWait(300);
    editor->setScrollY(editor->scrollY() + editor->cursorRectangle().y() - 220);
    const qreal snippetY = editor->cursorRectangle().y(), snippetScroll = editor->scrollY();
    QSignalSpy snippetChanges(editor, &DocumentEditor::rowsEdited);
    key(Qt::Key_Tab);
    if (!check(editor->text() == "$\\frac{a}{b}$" && editor->selectedText() == "a", "Scrolled snippet expansion failed")) return 1;
    if (!check(qAbs(editor->cursorRectangle().y() - snippetY) < 3 && qAbs(editor->scrollY() - snippetScroll) < 3,
               QString("Tab moved viewport: caret %1 to %2, scroll %3 to %4").arg(snippetY).arg(editor->cursorRectangle().y()).arg(snippetScroll).arg(editor->scrollY()))) return 1;
    if (!check(snippetChanges.size() == 1, "Snippet expansion emitted intermediate source edits")) return 1;
    const qreal expandedY = editor->cursorRectangle().y();
    key(Qt::Key_Tab);
    if (!check(editor->selectedText() == "b" && qAbs(editor->cursorRectangle().y() - expandedY) < 3, "Tab between fields moved viewport")) return 1;
    key(Qt::Key_Backtab, Qt::ShiftModifier);
    if (!check(editor->selectedText() == "a" && qAbs(editor->cursorRectangle().y() - expandedY) < 3, "Backtab between fields moved viewport")) return 1;
    undo();
    if (!check(editor->text() == "$frac$" && qAbs(editor->cursorRectangle().y() - snippetY) < 3,
               "Undo of snippet moved viewport")) return 1;
    redo();
    if (!check(editor->text() == "$\\frac{a}{b}$" && qAbs(editor->cursorRectangle().y() - snippetY) < 3,
               "Redo of snippet moved viewport")) return 1;
    editor->editRow(40, 8);
    const qreal typingY = editor->cursorRectangle().y();
    type("\\alpha");
    if (!check(qAbs(editor->cursorRectangle().y() - typingY) < 3, "Typing a LaTeX command moved viewport")) return 1;
    QStringList longNote;
    for (int i = 0; i < 65; ++i) longNote.append("A paragraph before and after the expression.");
    longNote[20] = "\\[\\frac{1}{1+\\frac{1}{1+x}}\\]";
    load(longNote); editor->editRow(38, 8);
    const qreal anchorY = editor->cursorRectangle().y();
    for (int i = 0; i < 100 && editor->renderedCount() < 1; ++i) QTest::qWait(50);
    if (!check(editor->renderedCount() == 1 && qAbs(editor->cursorRectangle().y() - anchorY) < 2,
               "Background rendering moved the writing position")) return 1;
    load({"\\section{Calculus}", "Let $f(x)=x^2$. Then $f'(x)=2x$.", "\\[\\int_0^1 x^2\\,dx = \\frac{1}{3}\\]", "Every polynomial is differentiable.", "Write here"});
    editor->editRow(3);
    QMetaObject::invokeMethod(window, "setActiveRowKind", Q_ARG(QVariant, QString("theorem")));
    editor->editRow(4);
    for (int i = 0; i < 120 && editor->renderedCount() < 3; ++i) QTest::qWait(100);
    QTest::qWait(150);
    auto *title = window->findChild<QObject *>("titleDisplay");
    auto *toolsButton = window->findChild<QObject *>("documentToolsButton");
    if (!check(title && toolsButton && toolsButton->property("visible").toBool()
               && toolsButton->property("width").toReal() > 20 && title->property("width").toReal() > 100,
               "Document title or tools button missing")) return 1;
    window->grabWindow().save("/tmp/foldtex-continuous-editor.png");
    QMetaObject::invokeMethod(toolsButton, "click");
    QTest::qWait(200);
    window->grabWindow().save("/tmp/foldtex-continuous-menu.png");
    key(Qt::Key_Escape); editor->forceActiveFocus();
    window->setProperty("courseName", "Calculus");
    key(Qt::Key_N, Qt::ControlModifier); QTest::qWait(100);
    auto *setup = window->findChild<QObject *>("newDocumentSetupDialog");
    if (!check(editor->sourceText().trimmed().isEmpty() && editor->hasActiveFocus()
               && window->property("courseName") == "Calculus" && setup && !setup->property("visible").toBool(),
               "Ctrl+N did not start writing immediately with course context retained")) return 1;
    type("Start writing");
    if (!check(editor->text() == "Start writing", "New document did not accept typing immediately")) return 1;
    std::fprintf(stdout, "PASS: centered display math, Up/Down/Shift+Up navigation, row-mode migration, mixed legacy formulas/PDF, continuous editing, automatic pairs, AltGr, source-mapped clicks, unchanged TeX images, text formatting, document packages/macros, Shift+Enter, snippets, undo/redo, Ctrl+N, PDF and save/reopen\n");
    return 0;
}
