#include "backend.h"
#include "documenteditor.h"
#include "spellchecker.h"
#include <QApplication>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QSignalSpy>
#include <QElapsedTimer>
#include <QFile>
#include <cstdio>

#define CHECK(condition, message) do { if (!(condition)) { std::fprintf(stderr, "FAIL: %s\n", message); return 1; } } while(false)
int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    app.setApplicationName("foldtex-spelling-test"); app.setOrganizationName("JagenmarkTests");
    QQuickStyle::setStyle("Material");
    if (argc != 2) return 2;
    Backend backend;
    QQmlApplicationEngine engine;
    bool warnings = false;
    QObject::connect(&engine, &QQmlEngine::warnings, [&](const QList<QQmlError> &errors) { for (const auto &error : errors) std::fprintf(stderr, "QML: %s\n", qPrintable(error.toString())); for (const auto &error : errors) if (!error.description().contains("Only binding to one of multiple key bindings")) warnings = true; });
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])));
    CHECK(!engine.rootObjects().isEmpty(), "QML load");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    auto *editor = window->findChild<DocumentEditor *>("documentEditor");
    auto *menu = window->findChild<QObject *>("spellingMenu");
    CHECK(editor && menu, "editor and menu");
    auto load = [&](QString text, QString language = QString()) {
        QVariantMap data{{"format", "foldtex-3"}, {"lines", QVariantList{QVariantMap{{"source", text}}}}};
        if (!language.isNull()) data.insert("spellLanguage", language);
        QMetaObject::invokeMethod(window, "loadData", Q_ARG(QVariant, data), Q_ARG(QVariant, QString()));
        QTest::qWait(25); editor->editRow(0, 0); editor->forceActiveFocus();
    };
    auto wait = [](auto condition) { return QTest::qWaitFor(condition, 8000); };
    auto word = [&] { return editor->spellingIssues().value(0).toMap().value("word").toString(); };
    load(QString::fromUtf8("Det häär är en text. Texten är svensk."));
    CHECK(editor->spellLanguage() == "sv" && editor->spellcheckEnabled(), "Swedish default for old documents");
    CHECK(wait([&] { return word() == QString::fromUtf8("häär"); }), "Swedish typo detected");
    CHECK(editor->spellingIssues().size() == 1 && editor->spellingError().isEmpty(), "correct Swedish including texten accepted");
    auto suggestions = [&] { const auto value = menu->property("suggestions"); return value.metaType() == QMetaType::fromType<QJSValue>() ? value.value<QJSValue>().toVariant().toStringList() : value.toStringList(); };
    auto clickWord = [&] {
        const auto issue = editor->spellingIssues().first().toMap();
        const auto rect = editor->spellingRectangle(issue.value("start").toInt());
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, editor->mapToScene(rect.center()).toPoint());
    };
    window->grabWindow().save("/tmp/foldtex-spelling.png");
    clickWord();
    bool opened = wait([&] { return menu->property("opened").toBool() && suggestions().contains(QString::fromUtf8("här")); });
    window->grabWindow().save("/tmp/foldtex-spelling-menu.png");
    CHECK(opened, "click opens Swedish suggestions");
    // Exercise the actual QML suggestion action, not just the C++ replacement API.
    QObject *suggestion = nullptr;
    for (auto *item : menu->findChildren<QObject *>("spellingSuggestion")) if (item->property("text").toString() == QString::fromUtf8("här")) suggestion = item;
    CHECK(suggestion, "suggestion menu item created");
    auto *suggestionItem = qobject_cast<QQuickItem *>(suggestion);
    CHECK(suggestionItem, "clickable suggestion item");
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
        suggestionItem->mapToScene(QPointF(suggestionItem->width()/2, suggestionItem->height()/2)).toPoint());
    CHECK(editor->text().startsWith(QString::fromUtf8("Det här är en text.")), "correction replaces only word");
    QMetaObject::invokeMethod(window, "undoDocument");
    CHECK(editor->text().contains(QString::fromUtf8("häär")), "correction undo");
    CHECK(wait([&] { return word() == QString::fromUtf8("häär"); }), "undo rechecks");
    clickWord(); editor->ignoreSpelling();
    CHECK(wait([&] { return editor->spellingIssues().isEmpty(); }), "ignore clears underline");
    CHECK(window->property("spellingIgnored").toStringList().contains(QString::fromUtf8("häär")), "ignore stored in document");
    QVariant ignoredDocument;
    QMetaObject::invokeMethod(window, "documentData", Q_RETURN_ARG(QVariant, ignoredDocument));
    QTemporaryDir ignoredDirectory;
    CHECK(backend.saveDocumentData(ignoredDirectory.filePath("ignored.foldtex"), ignoredDocument.toMap()), "save ignored words");
    CHECK(backend.loadDocument(ignoredDirectory.filePath("ignored.foldtex")).value("spellingIgnored").toStringList().contains(QString::fromUtf8("häär")), "ignored words persist through disk");
    load("This sentnce is correct.", "en");
    CHECK(wait([&] { return word() == "sentnce"; }), "English typo");
    CHECK(editor->spellingIssues().size() == 1, "English correct words accepted");
    clickWord();
    CHECK(wait([&] { return suggestions().contains("sentence"); }), "English suggestions");
    editor->insert(0, "A ");
    const auto changed = editor->text(); editor->correctSpelling("sentence");
    CHECK(editor->text() == changed && !menu->property("opened").toBool(), "stale suggestion invalidated after edit");
    QMetaObject::invokeMethod(window, "setDocumentSpellingLanguage", Q_ARG(QVariant, "sv"));
    CHECK(editor->spellLanguage() == "sv", "language setting reaches editor");
    QMetaObject::invokeMethod(window, "setDocumentSpellingLanguage", Q_ARG(QVariant, "en"));
    CHECK(editor->spellLanguage() == "en", "English selection");
    QVariant document;
    QMetaObject::invokeMethod(window, "documentData", Q_RETURN_ARG(QVariant, document));
    QTemporaryDir temporary;
    CHECK(backend.saveDocumentData(temporary.filePath("english.foldtex"), document.toMap()), "save English document");
    CHECK(backend.loadDocument(temporary.filePath("english.foldtex")).value("spellLanguage") == "en", "English language persists through disk");
    QMetaObject::invokeMethod(window, "startNewDocument");
    CHECK(editor->spellLanguage() == "sv" && editor->spellingIgnored().isEmpty(), "new document resets to Swedish");
    QFile old(temporary.filePath("old.foldtex")); CHECK(old.open(QIODevice::WriteOnly), "old document fixture");
    old.write("{\"format\":\"foldtex-3\",\"lines\":[{\"source\":\"texten\"}]}"); old.close();
    CHECK(backend.loadDocument(old.fileName()).value("spellLanguage") == "sv", "backend defaults old documents to Swedish");
    load(QString::fromUtf8("\\textbf{häär} och $\\unknown{mispelled}$ \\ref{mispelled} % mispelled"));
    CHECK(wait([&] { return word() == QString::fromUtf8("häär"); }), "formatted prose checked");
    CHECK(editor->spellingIssues().size() == 1, "commands math and comments excluded");
    clickWord(); CHECK(wait([&] { return suggestions().contains(QString::fromUtf8("här")); }), "formatted suggestions");
    editor->correctSpelling(QString::fromUtf8("här"));
    CHECK(editor->text().startsWith(QString::fromUtf8("\\textbf{här} och $\\unknown{mispelled}$")), "correction preserves LaTeX");
    load("sentnce", "en"); CHECK(wait([&] { return word() == "sentnce"; }), "enable fixture");
    window->setProperty("spellcheckEnabled", false);
    CHECK(editor->spellingIssues().isEmpty(), "disable clears marks");
    window->setProperty("spellcheckEnabled", true);
    CHECK(wait([&] { return word() == "sentnce"; }), "reenable rechecks");
    const QVariantList rows{QVariantMap{{"source", "\\usepackage{mispelled}"}}, QVariantMap{{"source", "\\newcommand{\\mispelled}{mispelled}"}},
        QVariantMap{{"source", "\\section{Heading} See https://mispelled.test and aa@mispelled.test $mispelled$"}},
        QVariantMap{{"kind", "image"}, {"source", "Caption"}}};
    QStringList checkedWords;
    for (const auto &candidate : SpellChecker::words(rows)) checkedWords.append(candidate.toMap().value("word").toString());
    CHECK(checkedWords == QStringList({"Heading", "See", "and", "Caption"}), "preamble URL formula excluded, heading and caption included");
    // Dragging from a misspelled word must still select text without a popup.
    load("sentnce and more text", "en");
    CHECK(wait([&] { return word() == "sentnce"; }), "drag fixture");
    const QPoint dragStart = editor->mapToScene(editor->spellingRectangle(0).center()).toPoint();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, dragStart);
    QTest::mouseMove(window, dragStart + QPoint(120, 0), 20);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, dragStart + QPoint(120, 0));
    CHECK(!editor->selectedText().isEmpty() && !menu->property("visible").toBool(), "drag selection does not open spelling menu");
    // Exercise a completed background scan, including offsets far down the note.
    SpellChecker checker;
    QSignalSpy complete(&checker, &SpellChecker::checked);
    QVariantList longRows;
    for (int i = 0; i < 1500; ++i) longRows.append(QVariantMap{{"source", "This sentnce is correct."}});
    QElapsedTimer scanTime; scanTime.start();
    checker.check(longRows, "en", {});
    int ticks = 0; QTimer responsiveness;
    QObject::connect(&responsiveness, &QTimer::timeout, [&] { ++ticks; }); responsiveness.start(1);
    CHECK(wait([&] { return !complete.isEmpty(); }), "long document spelling completes");
    CHECK(complete.first()[0].toList().size() == 1500 && ticks > 0, "long spelling scan keeps UI event loop responsive");
    std::fprintf(stderr, "Spelling scan 1500 paragraphs: %lld ms, %d UI timer ticks\n", scanTime.elapsed(), ticks);
    CHECK(!warnings, "no new QML warnings");
    std::fprintf(stderr, "PASS: spelling, click suggestions, correction/undo, language defaults/persistence, ignore and LaTeX exclusions\n");
    return 0;
}
