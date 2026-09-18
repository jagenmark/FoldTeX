#include "backend.h"
#include "latexsyntax.h"

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTest>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <cstdio>

namespace {
bool expect(bool condition, const QString &message) {
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", qPrintable(message));
    return condition;
}

double renderedHeight(const QVariantMap &render) {
    QFile svg(QUrl(render.value("url").toString()).toLocalFile());
    if (!svg.open(QIODevice::ReadOnly)) return -1;
    QXmlStreamReader xml(&svg);
    while (xml.readNextStartElement()) {
        if (xml.name() == QStringLiteral("svg"))
            return xml.attributes().value("viewBox").toString().split(' ').value(3).toDouble();
        xml.skipCurrentElement();
    }
    return -1;
}
}

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("foldtex-block-break-test"));
    app.setOrganizationName(QStringLiteral("JagenmarkTests"));
    QQuickStyle::setStyle(QStringLiteral("Material"));
    if (argc != 2) return 2;
    Backend backend;
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings, [](const QList<QQmlError> &warnings) {
        for (const auto &warning : warnings)
            std::fprintf(stderr, "QML: %s\n", qPrintable(warning.toString()));
    });
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
    engine.load(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])));
    if (!expect(!engine.rootObjects().isEmpty(), "Main.qml did not load")) return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QVariant editorValue;
    QVariant newLines;
    QObject *editor = nullptr;
    // Exercise real key events, then compile the resulting LaTeX. In particular,
    // splitting prose inside math must not put a row separator inside \text{}.
    struct BreakCase { QString source; int cursor; QString mode; bool math; };
    const QString problem = QStringLiteral(
        "\\text{Om } n \\text{ är ett heltal sådant att } 3 \\mid n, "
        "\\text{då är } 9 \\text{en delare till } (4n+3)^2");
    const QList<BreakCase> breakCases{
        {QStringLiteral("\\text{first second}"), 12, "auto", true},
        {problem, int(problem.indexOf(QStringLiteral("då är")) + 3), "auto", true},
        {QStringLiteral("\\text{one \\textbf{two three}}"), 22, "math", true},
        {QStringLiteral("\\begin{aligned}x &= 1 + 2\\end{aligned}"), 23, "math", true},
        {QStringLiteral("\\begin{pmatrix}1 & 2\\end{pmatrix}"), 20, "math", true},
        {QStringLiteral("\\begin{pmatrix}1\\end{pmatrix} + x"), 29, "math", true},
        {QStringLiteral("\\[x + y = 10\\]"), 7, "math", true},
        {QStringLiteral("$$x + y = 10$$"), 7, "math", true},
        {QStringLiteral("$x + y = 10$"), 6, "math", true},
        {QStringLiteral("first second"), 6, "auto", false},
        {QStringLiteral("x = literal text"), 4, "text", false}
    };
    for (BreakCase test : breakCases) {
        // Compatibility migration adds explicit delimiters to old bare equations.
        const auto migrated = backend.migrateRowModes({QVariantMap{{"source", test.source}, {"mode", test.mode}}});
        const QString canonical = migrated.first().toMap()["source"].toString();
        if (canonical != test.source) test.cursor += canonical.indexOf(test.source);
        test.source = canonical;
        test.mode = "latex";
        const QVariantMap data{{QStringLiteral("lines"), QVariantList{
            QVariantMap{{QStringLiteral("source"), test.source},
                        {QStringLiteral("mode"), test.mode}}
        }}};
        QMetaObject::invokeMethod(window, "loadData", Q_ARG(QVariant, data),
                                  Q_ARG(QVariant, QString()));
        QMetaObject::invokeMethod(window, "editLine", Q_ARG(QVariant, 0));
        QTest::qWait(80);
        QMetaObject::invokeMethod(window, "activeEditorItem", Q_RETURN_ARG(QVariant, editorValue));
        editor = editorValue.value<QObject *>();
        editor->setProperty("cursorPosition", test.cursor);
        QMetaObject::invokeMethod(editor, "forceActiveFocus");
        QTest::keyClick(window, Qt::Key_Return, Qt::ShiftModifier);
        QTest::qWait(30);
        const QString result = editor->property("text").toString();
        const int cursor = editor->property("cursorPosition").toInt();
        QMetaObject::invokeMethod(window, "serializedLines", Q_RETURN_ARG(QVariant, newLines));
        if (!expect(result.contains('\n') && newLines.toList().size() == 2
                        && window->property("activeIndex").toInt() == 0
                        && editor->property("activeFocus").toBool()
                        && result.mid(cursor).startsWith(test.source.mid(test.cursor).left(2)),
                    QStringLiteral("Break/cursor/focus failed for %1: %2").arg(test.source, result)))
            return 1;
        if (test.math) {
            const QVariantMap rendered = backend.render(result, "#ffffff", 18);
            if (!expect(rendered.value("error").toString().isEmpty(),
                        QStringLiteral("Broken LaTeX after Shift+Enter: %1: %2")
                            .arg(result, rendered.value("error").toString())))
                return 1;
            if (LatexSyntax::renderBody(test.source) == QStringLiteral("\\text{first second}")) {
                const double originalHeight = renderedHeight(backend.render(test.source, "#ffffff", 18));
                if (!expect(originalHeight > 0 && renderedHeight(rendered) > originalHeight * 1.5,
                            QStringLiteral("The requested break is not visible in the rendered SVG: %1 -> %2")
                                .arg(originalHeight).arg(renderedHeight(rendered))))
                    return 1;
            }
        } else if (!expect(result == test.source.left(test.cursor) + '\n'
                                      + test.source.mid(test.cursor),
                           QStringLiteral("Text break altered source"))) return 1;

        QTest::keyClick(window, Qt::Key_Z, Qt::ControlModifier);
        QTest::qWait(30);
        QMetaObject::invokeMethod(window, "serializedLines", Q_RETURN_ARG(QVariant, newLines));
        if (!expect(newLines.toList().first().toMap().value("source").toString() == test.source,
                    QStringLiteral("Undo did not restore the whole block"))) return 1;
        QTest::keyClick(window, Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier);
        QTest::qWait(30);
        QMetaObject::invokeMethod(window, "activeEditorItem", Q_RETURN_ARG(QVariant, editorValue));
        editor = editorValue.value<QObject *>();
        if (!expect(editor->property("text").toString() == result,
                    QStringLiteral("Redo did not restore the multiline block"))) return 1;

        // The next break stays in the same environment, including keypad Enter.
        editor->setProperty("cursorPosition", cursor);
        QMetaObject::invokeMethod(editor, "forceActiveFocus");
        QTest::keyClick(window, Qt::Key_Enter, Qt::ShiftModifier);
        QTest::qWait(30);
        const QString repeated = editor->property("text").toString();
        if (!expect(repeated.count('\n') == result.count('\n') + 1,
                    QStringLiteral("Repeated break nested or left the block: before <%1>, after <%2>, active %3")
                        .arg(result, repeated).arg(window->property("activeIndex").toInt()))) return 1;
        if (test.math && !expect(backend.render(repeated, "#ffffff", 18).value("error")
                                .toString().isEmpty(),
                                QStringLiteral("Repeated break produced invalid math: %1").arg(repeated)))
            return 1;
        editor->setProperty("cursorPosition", 0);
        QTest::keyClick(window, Qt::Key_Down);
        if (!expect(window->property("activeIndex").toInt() == 0
                        && editor->property("cursorPosition").toInt() > 0,
                    QStringLiteral("Down left a multiline block prematurely"))) return 1;
        QTest::keyClick(window, Qt::Key_Return);
        QTest::qWait(30);
        if (!expect(window->property("activeIndex").toInt() == (test.math ? 0 : 1),
                    QStringLiteral("Enter did not preserve environment/split text for %1: active=%2 source=%3").arg(test.source).arg(window->property("activeIndex").toInt()).arg(editor->property("text").toString()))) return 1;
    }

    const QVariantMap selectionData{{QStringLiteral("lines"), QVariantList{
        QVariantMap{{QStringLiteral("source"), "first replace last"}, {QStringLiteral("mode"), "text"}}
    }}};
    QMetaObject::invokeMethod(window, "loadData", Q_ARG(QVariant, selectionData),
                              Q_ARG(QVariant, QString()));
    QMetaObject::invokeMethod(window, "editLine", Q_ARG(QVariant, 0));
    QTest::qWait(80);
    QMetaObject::invokeMethod(window, "activeEditorItem", Q_RETURN_ARG(QVariant, editorValue));
    editor = editorValue.value<QObject *>();
    QMetaObject::invokeMethod(editor, "select", Q_ARG(int, 6), Q_ARG(int, 14));
    QMetaObject::invokeMethod(editor, "forceActiveFocus");
    QTest::keyClick(window, Qt::Key_Return, Qt::ShiftModifier);
    if (!expect(editor->property("text").toString() == QStringLiteral("first \nlast")
                    && editor->property("cursorPosition").toInt() == 7,
                QStringLiteral("Shift+Enter did not replace the selection at the cursor"))) return 1;

    std::fprintf(stdout, "PASS: block breaks, LaTeX rendering, undo/redo, navigation and keypad Enter\n");
    return 0;
}
