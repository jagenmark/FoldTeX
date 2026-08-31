#include "backend.h"

#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QPainter>
#include <QPdfWriter>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTest>
#include <QTemporaryDir>

#include <cstdio>

namespace {
bool expect(bool condition, const QString &message) {
    if (!condition) {
        qCritical().noquote() << "FAIL:" << message;
        std::fprintf(stderr, "FAIL: %s\n", qPrintable(message));
    }
    return condition;
}
}

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("foldtex-keyboard-test"));
    app.setOrganizationName(QStringLiteral("JagenmarkTests"));
    QQuickStyle::setStyle(QStringLiteral("Material"));
    if (argc != 2)
        return 2;

    QFile qmlFile(QString::fromLocal8Bit(argv[1]));
    if (!expect(qmlFile.open(QIODevice::ReadOnly), QStringLiteral("Could not read Main.qml")))
        return 1;
    const QByteArray qmlSource = qmlFile.readAll();
    if (!expect(!qmlSource.contains("ToolTip.text: row.error"),
                QStringLiteral("Raw LaTeX errors are still shown as warning tooltips"))
        || !expect(!qmlSource.contains("backend.render("),
                   QStringLiteral("QML still calls the blocking LaTeX renderer"))
        || !expect(qmlSource.contains("backend.renderAsync("),
                   QStringLiteral("QML does not use the background LaTeX renderer")))
        return 1;

    Backend backend;
    backend.setEditorFont(backend.editorFontFamily(), 17);
    backend.setEditorSideMargin(100);
    backend.saveRecovery(QStringLiteral("Arrow test"), {
        QVariantMap{{QStringLiteral("source"), QStringLiteral("first row")}},
        QVariantMap{{QStringLiteral("source"), QStringLiteral("second row")}}
    });
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
    engine.load(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])));
    if (!expect(!engine.rootObjects().isEmpty(), QStringLiteral("Main.qml did not load")))
        return 1;

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QObject *dialog = window->findChild<QObject *>(QStringLiteral("fontDialog"));
    QObject *font = window->findChild<QObject *>(QStringLiteral("fontChoice"));
    QObject *size = window->findChild<QObject *>(QStringLiteral("sizeChoice"));
    QObject *margin = window->findChild<QObject *>(QStringLiteral("marginChoice"));
    QObject *fontFrame = window->findChild<QObject *>(QStringLiteral("fontFocusFrame"));
    QObject *sizeFrame = window->findChild<QObject *>(QStringLiteral("sizeFocusFrame"));
    QObject *marginFrame = window->findChild<QObject *>(QStringLiteral("marginFocusFrame"));
    QObject *cancel = window->findChild<QObject *>(QStringLiteral("fontCancelButton"));
    QObject *ok = window->findChild<QObject *>(QStringLiteral("fontOkButton"));
    auto *guide = window->findChild<QQuickWindow *>(QStringLiteral("guideWindow"));
    QObject *titleField = window->findChild<QObject *>(QStringLiteral("titleField"));
    QObject *titleDisplay = window->findChild<QObject *>(QStringLiteral("titleDisplay"));
    QObject *titleSlot = window->findChild<QObject *>(QStringLiteral("titleSlot"));
    QObject *saveLabel = window->findChild<QObject *>(QStringLiteral("saveLabel"));
    QObject *guideHeaderButton = window->findChild<QObject *>(QStringLiteral("guideHeaderButton"));
    QObject *edgeMenuPanel = window->findChild<QObject *>(QStringLiteral("edgeMenuPanel"));
    const QStringList edgeButtonNames{
        QStringLiteral("settingsEdgeButton"), QStringLiteral("helpEdgeButton"),
        QStringLiteral("syntaxEdgeButton"), QStringLiteral("viewEdgeButton"),
        QStringLiteral("exportEdgeButton"), QStringLiteral("guideHeaderButton"),
        QStringLiteral("figureEdgeButton"), QStringLiteral("slidesEdgeButton")
    };
    QList<QObject *> edgeButtons;
    for (const QString &name : edgeButtonNames)
        edgeButtons.append(window->findChild<QObject *>(name));
    if (!expect(window && dialog && font && size && margin && fontFrame && sizeFrame
                    && marginFrame && cancel && ok && guide && titleField && titleDisplay
                    && titleSlot && saveLabel && guideHeaderButton && edgeMenuPanel
                    && std::all_of(edgeButtons.cbegin(), edgeButtons.cend(),
                                   [](QObject *button) { return button; }),
                QStringLiteral("Aa controls were not found")))
        return 1;
    if (!expect(titleDisplay->property("elide").toInt() == Qt::ElideRight,
                QStringLiteral("Idle title does not elide its end")))
        return 1;
    window->setProperty("documentTitle", QStringLiteral("xDDDDDDDDDDDDDDDDDDDDDDDDDDDD"));
    const QList<int> legalWidths{560, 600, 720, 980, 1280};
    for (int legalWidth : legalWidths) {
        window->setWidth(legalWidth);
        QTest::qWait(30);
        const qreal titleLeft = titleSlot->property("x").toReal();
        const qreal titleRight = titleLeft + titleSlot->property("width").toReal();
        const qreal saveRight = saveLabel->property("x").toReal()
                                + saveLabel->property("width").toReal();
        const qreal titleCenter = (titleLeft + titleRight) / 2;
        if (!expect(qAbs(titleCenter - legalWidth / 2.0) < 0.6,
                    QStringLiteral("Title is not centered at width %1")
                        .arg(legalWidth))
            || !expect(titleLeft >= saveRight && titleRight <= legalWidth - 56,
                    QStringLiteral("Centered title overlaps edge controls at width %1")
                        .arg(legalWidth)))
            return 1;
    }
    QTest::mouseMove(window, QPoint(window->width() - 2, 4));
    QTest::qWait(220);
    if (!expect(edgeMenuPanel->property("opacity").toReal() > 0.9,
                QStringLiteral("Right-edge menu did not appear on hover")))
        return 1;
    qreal priorButtonY = -1;
    const qreal edgeButtonX = edgeButtons.first()->property("x").toReal();
    for (QObject *button : edgeButtons) {
        if (!expect(qAbs(button->property("x").toReal() - edgeButtonX) < 0.6
                        && button->property("y").toReal() > priorButtonY,
                    QStringLiteral("Edge controls are not one per row")))
            return 1;
        priorButtonY = button->property("y").toReal();
    }
    QTest::mouseMove(window, QPoint(20, 200));
    QTest::qWait(220);
    if (!expect(edgeMenuPanel->property("opacity").toReal() < 0.1,
                QStringLiteral("Right-edge menu did not hide after hover")))
        return 1;
    QTest::mouseMove(window, QPoint(window->width() - 2, 200));
    QTest::qWait(220);
    if (!expect(edgeMenuPanel->property("opacity").toReal() < 0.1,
                QStringLiteral("The edge menu still opens over the document scrollbar")))
        return 1;
    window->setWidth(560);
    QTest::qWait(30);
    if (!expect(titleDisplay->property("visible").toBool()
                    && titleDisplay->property("truncated").toBool(),
                QStringLiteral("Idle long title does not show a right ellipsis")))
        return 1;
    QMetaObject::invokeMethod(titleField, "forceActiveFocus");
    QTest::qWait(30);
    if (!expect(titleField->property("activeFocus").toBool()
                    && !titleDisplay->property("visible").toBool()
                    && titleField->property("opacity").toReal() == 1,
                QStringLiteral("Focused title does not switch to the real editor")))
        return 1;
    window->setWidth(980);
    if (!expect(guide->transientParent() == nullptr,
                QStringLiteral("Guide is attached as an oversized child window")))
        return 1;
    const QVariantList guideSections = guide->property("sections").toList();
    QStringList guideTitles;
    for (const QVariant &section : guideSections)
        guideTitles.append(section.toMap().value(QStringLiteral("title")).toString());
    if (!expect(guideSections.size() >= 34
                    && guideTitles.contains(QStringLiteral("Fast LaTeX snippets"))
                    && guideTitles.contains(QStringLiteral("Save, open, and recover"))
                    && guideTitles.contains(QStringLiteral("Definitions, theorems, and proofs"))
                    && guideTitles.contains(QStringLiteral("Courses and lecture details"))
                    && guideTitles.contains(QStringLiteral("Figures"))
                    && guideTitles.contains(QStringLiteral("Lecture slides")),
                QStringLiteral("Guide is missing current FoldTeX features")))
        return 1;
    guide->setWidth(578);
    guide->setProperty("preferredSideMargin", 100);
    guide->show();
    QTest::qWait(100);
    QObject *guideScroll = guide->findChild<QObject *>(QStringLiteral("guideScroll"));
    QObject *guidePage = guide->findChild<QObject *>(QStringLiteral("guidePage"));
    QObject *guideScrollBar = guide->findChild<QObject *>(QStringLiteral("guideScrollBar"));
    if (!expect(guide->property("pageMargin").toInt() <= 21,
                QStringLiteral("Guide kept desktop-sized margins in a narrow window"))
        || !expect(guideScroll && guidePage && guideScrollBar,
                   QStringLiteral("Guide page layout was not found"))
        || !expect(guidePage->property("width").toReal()
                       <= guideScroll->property("availableWidth").toReal(),
                   QStringLiteral("Guide page extends beyond the scroll view")))
        return 1;
    QObject *guideFlick = guideScroll->property("contentItem").value<QObject *>();
    QObject *guideSearch = guide->findChild<QObject *>(QStringLiteral("guideSearch"));
    guideFlick->setProperty("contentY", 0);
    QMetaObject::invokeMethod(guideSearch, "forceActiveFocus");
    QTest::keyClick(guide, Qt::Key_Down);
    QTest::qWait(50);
    if (!expect(guideFlick->property("contentY").toReal() >= 40,
                QStringLiteral("Down did not scroll the guide")))
        return 1;
    guideFlick->setProperty("contentY", 0);
    QTest::wheelEvent(guide, QPointF(guide->width() / 2, guide->height() / 2),
                      QPoint(), QPoint(0, -20));
    QTest::qWait(50);
    if (!expect(guideFlick->property("contentY").toReal() >= 300,
                QStringLiteral("Trackpad step scrolls the guide too slowly"))
        || !expect(guideScrollBar->property("active").toBool()
                       && guideScrollBar->property("opacity").toReal() > 0,
                   QStringLiteral("Guide scroll bar did not appear while scrolling")))
        return 1;
    guideFlick->setProperty("contentY", 0);
    QTest::wheelEvent(guide, QPointF(guide->width() / 2, guide->height() / 2),
                      QPoint(0, -15));
    QTest::qWait(50);
    if (!expect(guideFlick->property("contentY").toReal() >= 240,
                QStringLiteral("Smooth angle step scrolls the guide too slowly")))
        return 1;
    guideFlick->setProperty("contentY", 0);
    const QPointF guideWheelPoint(guide->width() / 2, guide->height() / 2);
    QTest::wheelEvent(guide, guideWheelPoint, QPoint(), QPoint(0, -3),
                      Qt::NoModifier, Qt::NoScrollPhase);
    QTest::qWait(30);
    const qreal touchpadGuideDistance = guideFlick->property("contentY").toReal();
    if (!expect(touchpadGuideDistance >= 220,
                QStringLiteral("A small touchpad update scrolls the guide too slowly (%1 px)")
                    .arg(touchpadGuideDistance)))
        return 1;
    QTest::qWait(900);
    if (!expect(!guideScrollBar->property("active").toBool()
                    && guideScrollBar->property("opacity").toReal() < 0.1,
                QStringLiteral("Guide scroll bar did not fade after scrolling")))
        return 1;
    guide->hide();
    window->requestActivate();
    QTest::qWait(100);

    QMetaObject::invokeMethod(window, "editLine", Q_ARG(QVariant, QVariant(1)));
    QTest::qWait(400);
    QVariant editorValue;
    QMetaObject::invokeMethod(window, "activeEditorItem", Q_RETURN_ARG(QVariant, editorValue));
    QObject *editor = editorValue.value<QObject *>();
    if (!expect(editor, QStringLiteral("Active line editor was not found")))
        return 1;
    editor->setProperty("cursorPosition", 3);
    QMetaObject::invokeMethod(editor, "forceActiveFocus");
    QTest::keyClick(window, Qt::Key_Up);
    QTest::qWait(50);
    if (!expect(window->property("activeIndex").toInt() == 0,
                QStringLiteral("Up did not leave a row from the middle of its text")))
        return 1;

    QTest::keyClick(window, Qt::Key_Down, Qt::ShiftModifier);
    QTest::qWait(50);
    QTest::keyClick(window, Qt::Key_C, Qt::ControlModifier);
    QTest::qWait(50);
    if (!expect(backend.clipboardText() == QStringLiteral("first row\nsecond row"),
                QStringLiteral("Ctrl+C did not copy the selected rows")))
        return 1;

    QTest::keyClick(window, Qt::Key_Delete);
    QTest::qWait(100);
    if (!expect(!window->property("hasLineSelection").toBool(),
                QStringLiteral("Delete did not clear the row selection"))
        || !expect(window->property("activeIndex").toInt() == 0,
                   QStringLiteral("Delete did not leave an editable row")))
        return 1;

    backend.saveRecovery(QStringLiteral("Arrow test"), {
        QVariantMap{{QStringLiteral("source"), QStringLiteral("first row")}},
        QVariantMap{{QStringLiteral("source"), QStringLiteral("second row")}}
    });
    QMetaObject::invokeMethod(window, "loadData",
                              Q_ARG(QVariant, backend.loadRecovery()),
                              Q_ARG(QVariant, QString()));
    QTest::qWait(100);

    QTest::qWait(100);
    editorValue.clear();
    QMetaObject::invokeMethod(window, "activeEditorItem", Q_RETURN_ARG(QVariant, editorValue));
    editor = editorValue.value<QObject *>();
    const QString longLine = QStringLiteral(
        "This is a deliberately long FoldTeX row that must wrap inside the writing area "
        "instead of continuing past the right edge and becoming invisible. ").repeated(24);
    editor->setProperty("text", longLine);
    QTest::qWait(100);
    if (!expect(editor->property("contentWidth").toReal() <= editor->property("width").toReal(),
                QStringLiteral("Long active text extends past the editor width"))
        || !expect(editor->property("contentHeight").toReal() > 42,
                   QStringLiteral("Long active text did not grow to several visual lines")))
        return 1;

    const int wrappedRow = window->property("activeIndex").toInt();
    editor->setProperty("cursorPosition", longLine.size());
    QMetaObject::invokeMethod(editor, "forceActiveFocus");
    QTest::qWait(50);
    const int wrappedCursorBefore = editor->property("cursorPosition").toInt();
    QTest::keyClick(window, Qt::Key_Up);
    QTest::qWait(50);
    if (!expect(window->property("activeIndex").toInt() == wrappedRow,
                QStringLiteral("Up left a row from its lower wrapped screen line"))
        || !expect(editor->property("cursorPosition").toInt() < wrappedCursorBefore,
                   QStringLiteral("Up did not move within the wrapped row")))
        return 1;
    editor->setProperty("cursorPosition", 0);
    QTest::keyClick(window, Qt::Key_Up);
    QTest::qWait(50);
    if (!expect(window->property("activeIndex").toInt() == wrappedRow - 1,
                QStringLiteral("Up from the first wrapped screen line did not open the prior row")))
        return 1;
    QMetaObject::invokeMethod(window, "editLine", Q_ARG(QVariant, QVariant(wrappedRow)));
    QTest::qWait(50);

    QVariantList scrollLines;
    for (int i = 0; i < 40; ++i)
        scrollLines.append(QVariantMap{{QStringLiteral("source"),
                                       QStringLiteral("scroll row %1").arg(i)}});
    const QVariantMap scrollData{{QStringLiteral("title"), QStringLiteral("Scroll test")},
                                 {QStringLiteral("lines"), scrollLines}};
    QMetaObject::invokeMethod(
        window, "loadData",
        Q_ARG(QVariant, scrollData),
        Q_ARG(QVariant, QString()));
    QTest::qWait(100);

    QObject *documentList = window->findChild<QObject *>(QStringLiteral("documentList"));
    QObject *documentScrollBar = window->findChild<QObject *>(
        QStringLiteral("documentScrollBar"));
    QObject *documentScrollLinger = window->findChild<QObject *>(
        QStringLiteral("documentScrollLinger"));
    QObject *documentScrollHoverZone = window->findChild<QObject *>(
        QStringLiteral("documentScrollHoverZone"));
    QObject *documentScrollDownShortcut = window->findChild<QObject *>(
        QStringLiteral("documentScrollDownShortcut"));
    if (!expect(documentScrollBar && documentScrollLinger && documentScrollHoverZone,
                QStringLiteral("Document scroll controls were not found"))
        || !expect(documentScrollLinger->property("interval").toInt() >= 2000,
                   QStringLiteral("Document scrollbar still hides too quickly")))
        return 1;
    window->setHeight(420);
    QTest::qWait(100);
    window->setProperty("displayMode", 2);
    QTest::qWait(80);
    if (!expect(documentScrollDownShortcut
                    && documentScrollDownShortcut->property("enabled").toBool(),
                QStringLiteral("Document Down shortcut was not enabled outside the editor")))
        return 1;
    documentList->setProperty("contentY", documentList->property("originY"));
    const qreal arrowStart = documentList->property("contentY").toReal();
    QTest::keyClick(window, Qt::Key_Down);
    QTest::qWait(50);
    if (!expect(documentList->property("contentY").toReal() - arrowStart >= 45,
                QStringLiteral("Down arrow did not scroll a non-editing document (%1 -> %2)")
                    .arg(arrowStart).arg(documentList->property("contentY").toReal())))
        return 1;
    window->setProperty("displayMode", 0);
    QMetaObject::invokeMethod(window, "editLine", Q_ARG(QVariant, QVariant(wrappedRow)));
    QTest::qWait(50);
    documentList->setProperty("contentY", documentList->property("originY"));
    const qreal wheelStart = documentList->property("contentY").toReal();
    QTest::wheelEvent(window, QPointF(window->width() / 2, window->height() / 2),
                      QPoint(), QPoint(0, -20));
    QTest::qWait(50);
    const qreal wheelDistance = documentList->property("contentY").toReal() - wheelStart;
    if (!expect(wheelDistance >= 300,
                QStringLiteral("Trackpad step scrolls the document too slowly (%1 px)")
                    .arg(wheelDistance)
                    + QStringLiteral("; content %1, viewport %2")
                          .arg(documentList->property("contentHeight").toReal())
                          .arg(documentList->property("height").toReal())))
        return 1;
    if (!expect(documentScrollBar->property("active").toBool()
                    && documentScrollBar->property("opacity").toReal() > 0,
                QStringLiteral("Document scroll bar did not appear while scrolling")))
        return 1;
    documentList->setProperty("contentY", documentList->property("originY"));
    const qreal angleStart = documentList->property("contentY").toReal();
    QTest::wheelEvent(window, QPointF(window->width() / 2, window->height() / 2),
                      QPoint(0, -15));
    QTest::qWait(50);
    if (!expect(documentList->property("contentY").toReal() - angleStart >= 240,
                QStringLiteral("Smooth angle step scrolls the document too slowly")))
        return 1;
    QTest::qWait(900);
    if (!expect(documentScrollBar->property("active").toBool()
                    && documentScrollBar->property("opacity").toReal() > 0.9,
                QStringLiteral("Document scroll bar faded before it could be grabbed")))
        return 1;

    QTemporaryDir slideCourse;
    const QString slidePdf = slideCourse.path() + QStringLiteral("/slides.pdf");
    {
        QPdfWriter writer(slidePdf);
        QPainter painter(&writer);
        painter.drawText(QPoint(100, 100), QStringLiteral("Lecture slide"));
        painter.end();
    }
    QVariantMap slideData{
        {QStringLiteral("title"), QStringLiteral("Slide test")},
        {QStringLiteral("sourcePdf"), slidePdf},
        {QStringLiteral("lines"), QVariantList{
             QVariantMap{{QStringLiteral("source"), QStringLiteral("linked note")}}
         }}
    };
    QMetaObject::invokeMethod(window, "loadData", Q_ARG(QVariant, slideData),
                              Q_ARG(QVariant, slideCourse.path()
                                    + QStringLiteral("/notes.foldtex")));
    window->setProperty("pdfOpen", true);
    window->setWidth(980);
    QTest::qWait(250);
    QObject *notePane = window->findChild<QObject *>(QStringLiteral("notePane"));
    QObject *pdfPane = window->findChild<QObject *>(QStringLiteral("pdfPane"));
    QObject *pdfView = window->findChild<QObject *>(QStringLiteral("pdfView"));
    QObject *figurePopup = window->findChild<QObject *>(QStringLiteral("figureEditorPopup"));
    QObject *figureCanvas = window->findChild<QObject *>(QStringLiteral("figureCanvas"));
    if (!expect(notePane && pdfPane && pdfView && figurePopup && figureCanvas,
                QStringLiteral("Figure editor or slide split was not found"))
        || !expect(notePane->property("visible").toBool()
                       && pdfPane->property("visible").toBool(),
                   QStringLiteral("Wide slide view did not show note and PDF together")))
        return 1;
    window->setWidth(700);
    QTest::qWait(50);
    if (!expect(!notePane->property("visible").toBool()
                    && pdfPane->property("visible").toBool(),
                QStringLiteral("Narrow slide view did not switch to PDF-only mode")))
        return 1;
    QMetaObject::invokeMethod(window, "editLine", Q_ARG(QVariant, QVariant(0)));
    QTest::qWait(50);
    QMetaObject::invokeMethod(window, "linkActiveRowToSlide");
    QVariant linkedLines;
    QMetaObject::invokeMethod(window, "serializedLines", Q_RETURN_ARG(QVariant, linkedLines));
    if (!expect(linkedLines.toList().first().toMap().value(QStringLiteral("slide")).toInt() == 0,
                QStringLiteral("Active row did not link to the current slide")))
        return 1;
    QMetaObject::invokeMethod(figurePopup, "open");
    QTest::qWait(400);
    figurePopup->setProperty("tool", QStringLiteral("arrow"));
    QMetaObject::invokeMethod(figurePopup, "beginAction", Q_ARG(QVariant, 30), Q_ARG(QVariant, 30));
    QMetaObject::invokeMethod(figurePopup, "finishAction", Q_ARG(QVariant, 120), Q_ARG(QVariant, 90));
    if (!expect(figurePopup->property("actions").toList().size() == 1,
                QStringLiteral("Figure editor did not store an arrow")))
        return 1;
    QTest::keyClick(window, Qt::Key_Z, Qt::ControlModifier);
    QTest::qWait(50);
    if (!expect(figurePopup->property("actions").toList().isEmpty(),
                QStringLiteral("Ctrl+Z did not undo the last figure action")))
        return 1;
    QMetaObject::invokeMethod(figurePopup, "beginAction", Q_ARG(QVariant, 30), Q_ARG(QVariant, 30));
    QMetaObject::invokeMethod(figurePopup, "finishAction", Q_ARG(QVariant, 120), Q_ARG(QVariant, 90));
    QMetaObject::invokeMethod(figurePopup, "saveFigure");
    QTest::qWait(1000);
    QVariant figureLines;
    QMetaObject::invokeMethod(window, "serializedLines", Q_RETURN_ARG(QVariant, figureLines));
    const QVariantList savedFigureLines = figureLines.toList();
    const QString figurePath = savedFigureLines.size() > 1
        ? savedFigureLines.at(1).toMap().value(QStringLiteral("asset")).toString()
        : QString();
    if (!expect(!figurePath.isEmpty() && QFile::exists(figurePath),
                QStringLiteral("Figure editor did not save and insert its drawing"))) {
        std::fprintf(stderr, "figure rows=%lld path=%s visible=%d actions=%lld\n",
                     static_cast<long long>(savedFigureLines.size()), qPrintable(figurePath),
                     figurePopup->property("visible").toBool(),
                     static_cast<long long>(figurePopup->property("actions").toList().size()));
        std::fprintf(stderr, "canvas=%gx%g document=%s\n",
                     figureCanvas->property("width").toDouble(),
                     figureCanvas->property("height").toDouble(),
                     qPrintable(figurePopup->property("documentPath").toString()));
        return 1;
    }
    window->setWidth(980);

    QMetaObject::invokeMethod(dialog, "open");
    window->requestActivate();
    QTest::qWait(400);
    if (!expect(dialog->property("visible").toBool(), QStringLiteral("Aa dialog did not open"))
        || !expect(dialog->property("keyboardRow").toInt() == 0,
                   QStringLiteral("Aa dialog did not start on Font"))
        || !expect(fontFrame->property("keyboardSelected").toBool(),
                   QStringLiteral("Font did not show the keyboard selection")))
        return 1;

    const int fontBefore = font->property("currentIndex").toInt();
    QTest::keyClick(window, Qt::Key_Right);
    if (!expect(font->property("currentIndex").toInt() == fontBefore + 1,
                QStringLiteral("Right did not select the next font")))
        return 1;

    QTest::keyClick(window, Qt::Key_Down);
    const int sizeBefore = size->property("value").toInt();
    QTest::keyClick(window, Qt::Key_Right);
    if (!expect(dialog->property("keyboardRow").toInt() == 1,
                QStringLiteral("Down did not move to Size"))
        || !expect(sizeFrame->property("keyboardSelected").toBool(),
                   QStringLiteral("Size did not show the keyboard selection"))
        || !expect(size->property("value").toInt() == sizeBefore + 1,
                   QStringLiteral("Right did not raise Size")))
        return 1;

    QTest::keyClick(window, Qt::Key_Down);
    const int marginBefore = margin->property("value").toInt();
    QTest::keyClick(window, Qt::Key_Left);
    if (!expect(dialog->property("keyboardRow").toInt() == 2,
                QStringLiteral("Down did not move to Side margin"))
        || !expect(marginFrame->property("keyboardSelected").toBool(),
                   QStringLiteral("Side margin did not show the keyboard selection"))
        || !expect(margin->property("value").toInt() == marginBefore - 4,
                   QStringLiteral("Left did not lower Side margin (%1 -> %2)")
                       .arg(marginBefore).arg(margin->property("value").toInt())))
        return 1;

    QTest::keyClick(window, Qt::Key_Down);
    QTest::keyClick(window, Qt::Key_Right);
    if (!expect(dialog->property("keyboardRow").toInt() == 3,
                QStringLiteral("Down did not move to the buttons"))
        || !expect(dialog->property("keyboardButton").toInt() == 1,
                   QStringLiteral("Right did not move from Cancel to OK"))
        || !expect(ok->property("keyboardSelected").toBool()
                       && !cancel->property("keyboardSelected").toBool(),
                   QStringLiteral("OK did not show the keyboard selection")))
        return 1;

    QTest::keyClick(window, Qt::Key_Return);
    QTest::qWait(400);
    if (!expect(!dialog->property("visible").toBool(),
                QStringLiteral("Enter on OK did not close Aa (row %1, button %2)")
                    .arg(dialog->property("keyboardRow").toInt())
                    .arg(dialog->property("keyboardButton").toInt()))
        || !expect(backend.editorFontSize() == size->property("value").toInt(),
                   QStringLiteral("Enter on OK did not save the changed size")))
        return 1;

    QMetaObject::invokeMethod(dialog, "open");
    QTest::qWait(400);
    QTest::keyClick(window, Qt::Key_Return);
    QTest::qWait(400);
    if (!expect(!dialog->property("visible").toBool(),
                QStringLiteral("Enter from Font did not apply and close Aa")))
        return 1;

    const int savedSize = backend.editorFontSize();
    QMetaObject::invokeMethod(dialog, "open");
    QTest::qWait(400);
    QTest::keyClick(window, Qt::Key_Down);
    QTest::keyClick(window, Qt::Key_Right);
    QTest::keyClick(window, Qt::Key_Down);
    QTest::keyClick(window, Qt::Key_Down);
    QTest::keyClick(window, Qt::Key_Return);
    QTest::qWait(400);
    if (!expect(!dialog->property("visible").toBool(),
                QStringLiteral("Enter on Cancel did not close Aa"))
        || !expect(backend.editorFontSize() == savedSize,
                   QStringLiteral("Enter on Cancel applied a changed value")))
        return 1;

    QVariantMap searchData{
        {QStringLiteral("title"), QStringLiteral("Search test")},
        {QStringLiteral("lines"), QVariantList{
             QVariantMap{{QStringLiteral("source"), QStringLiteral("\\sqrt{4d}")}},
             QVariantMap{{QStringLiteral("source"), QStringLiteral("plain 4d")}},
             QVariantMap{{QStringLiteral("source"), QStringLiteral("other")}}
         }}
    };
    QMetaObject::invokeMethod(window, "loadData", Q_ARG(QVariant, searchData),
                              Q_ARG(QVariant, QString()));
    QMetaObject::invokeMethod(window, "openSearch");
    QTest::qWait(100);
    QObject *searchInput = window->findChild<QObject *>(QStringLiteral("searchInput"));
    QMetaObject::invokeMethod(searchInput, "forceActiveFocus");
    QTest::keyClick(window, Qt::Key_4);
    QTest::keyClick(window, Qt::Key_D);
    QTest::qWait(100);
    if (!expect(window->property("searchLine").toInt() == 0,
                QStringLiteral("Typing 4d did not find \\sqrt{4d}")))
        return 1;
    QVariant matchCount;
    QVariant selectedLevel;
    QVariant otherLevel;
    QMetaObject::invokeMethod(window, "searchMatchCount", Q_RETURN_ARG(QVariant, matchCount));
    QMetaObject::invokeMethod(window, "searchHighlightLevel", Q_RETURN_ARG(QVariant, selectedLevel),
                              Q_ARG(QVariant, 0));
    QMetaObject::invokeMethod(window, "searchHighlightLevel", Q_RETURN_ARG(QVariant, otherLevel),
                              Q_ARG(QVariant, 1));
    if (!expect(matchCount.toInt() == 2,
                QStringLiteral("Search did not mark every 4d row"))
        || !expect(selectedLevel.toInt() == 2 && otherLevel.toInt() == 1,
                   QStringLiteral("Search did not distinguish current and other matches")))
        return 1;
    QObject *searchNext = window->findChild<QObject *>(QStringLiteral("searchNextButton"));
    QMetaObject::invokeMethod(searchNext, "click");
    QTest::qWait(50);
    selectedLevel.clear();
    otherLevel.clear();
    QMetaObject::invokeMethod(window, "searchHighlightLevel", Q_RETURN_ARG(QVariant, otherLevel),
                              Q_ARG(QVariant, 0));
    QMetaObject::invokeMethod(window, "searchHighlightLevel", Q_RETURN_ARG(QVariant, selectedLevel),
                              Q_ARG(QVariant, 1));
    if (!expect(window->property("searchLine").toInt() == 1
                    && selectedLevel.toInt() == 2 && otherLevel.toInt() == 1,
                QStringLiteral("Next search result did not move the strong highlight "
                               "(line %1, levels %2/%3)")
                    .arg(window->property("searchLine").toInt())
                    .arg(otherLevel.toInt()).arg(selectedLevel.toInt())))
        return 1;

    QVariantList priorityLines;
    for (int i = 0; i < 40; ++i) {
        priorityLines.append(QVariantMap{{
            QStringLiteral("source"), QStringLiteral("plain row %1").arg(i)
        }});
    }
    QVariantMap priorityData{
        {QStringLiteral("title"), QStringLiteral("Visible row order test")},
        {QStringLiteral("lines"), priorityLines}
    };
    QMetaObject::invokeMethod(window, "loadData", Q_ARG(QVariant, priorityData),
                              Q_ARG(QVariant, QString()));
    QTest::qWait(150);
    QMetaObject::invokeMethod(window, "queueRenders", Q_ARG(QVariant, true));
    QVariant visiblePrefix;
    QMetaObject::invokeMethod(window, "renderQueueHasVisiblePrefix",
                              Q_RETURN_ARG(QVariant, visiblePrefix));
    const int visibleQueued = window->property("queuedVisibleCount").toInt();
    if (!expect(visibleQueued > 0 && visibleQueued < 41,
                QStringLiteral("Render queue did not split visible and off-screen rows"))
        || !expect(visiblePrefix.toBool(),
                   QStringLiteral("Off-screen rows were queued before visible rows")))
        return 1;
    QMetaObject::invokeMethod(window, "cancelQueuedRenders");

    QVariantList slowRenderLines;
    for (int i = 0; i < 5; ++i) {
        slowRenderLines.append(QVariantMap{{
            QStringLiteral("source"),
            QStringLiteral("\\foldtexMissingCommand%1{x}").arg(i)
        }});
    }
    QVariantMap slowRenderData{
        {QStringLiteral("title"), QStringLiteral("Render mode timing test")},
        {QStringLiteral("lines"), slowRenderLines}
    };
    QMetaObject::invokeMethod(window, "loadData", Q_ARG(QVariant, slowRenderData),
                              Q_ARG(QVariant, QString()));
    window->setProperty("displayMode", 1);
    QElapsedTimer modeTimer;
    modeTimer.start();
    QMetaObject::invokeMethod(window, "cycleDisplayMode");
    const qint64 modeChangeMs = modeTimer.elapsed();
    if (!expect(window->property("displayMode").toInt() == 2,
                QStringLiteral("Source button did not enter Rendered mode"))
        || !expect(modeChangeMs < 100,
                   QStringLiteral("Source button blocked for %1 ms while rendering")
                       .arg(modeChangeMs))
        || !expect(window->property("pendingRenderCount").toInt() == 6,
                   QStringLiteral("Rendered mode did not queue all six rows")))
        return 1;

    QVariantMap savedDocumentData{
        {QStringLiteral("title"), QStringLiteral("Saved note")},
        {QStringLiteral("course"), QStringLiteral("Calculus I")},
        {QStringLiteral("lines"), QVariantList{
             QVariantMap{{QStringLiteral("source"), QStringLiteral("keep this line")}}
         }}
    };
    QMetaObject::invokeMethod(window, "loadData", Q_ARG(QVariant, savedDocumentData),
                              Q_ARG(QVariant, QStringLiteral("/tmp/saved.foldtex")));
    QTest::keyClick(window, Qt::Key_N, Qt::ControlModifier);
    QTest::qWait(100);
    QObject *newNoteDialog = window->findChild<QObject *>(QStringLiteral("newDocumentSetupDialog"));
    QObject *newTitleField = window->findChild<QObject *>(QStringLiteral("newTitleField"));
    QObject *newCourseField = window->findChild<QObject *>(QStringLiteral("newCourseField"));
    QObject *newLectureField = window->findChild<QObject *>(QStringLiteral("newLectureField"));
    QObject *newLectureDateField = window->findChild<QObject *>(QStringLiteral("newLectureDateField"));
    if (!expect(newNoteDialog && newNoteDialog->property("visible").toBool(),
                QStringLiteral("Ctrl+N did not open the new-note details form"))
        || !expect(newCourseField
                       && newCourseField->property("text").toString()
                              == QStringLiteral("Calculus I"),
                   QStringLiteral("New-note form did not carry over the current course "
                                  "(document: '%1', field: '%2')")
                       .arg(window->property("courseName").toString(),
                            newCourseField
                                ? newCourseField->property("text").toString()
                                : QStringLiteral("missing")))
        || !expect(newTitleField && newLectureField && newLectureDateField,
                   QStringLiteral("New-note detail fields were not available")))
        return 1;
    newTitleField->setProperty("text", QStringLiteral("Lecture 5 notes"));
    newLectureField->setProperty("text", QStringLiteral("Derivatives"));
    newLectureDateField->setProperty("text", QStringLiteral("2026-08-30"));
    QTest::keyClick(window, Qt::Key_Return);
    QTest::qWait(100);
    QVariant newLines;
    QMetaObject::invokeMethod(window, "serializedLines", Q_RETURN_ARG(QVariant, newLines));
    const QVariantList blankLines = newLines.toList();
    if (!expect(window->property("documentTitle").toString()
                    == QStringLiteral("Lecture 5 notes")
                    && window->property("documentPath").toString().isEmpty(),
                QStringLiteral("New-note form did not set the document identity"))
        || !expect(window->property("courseName").toString() == QStringLiteral("Calculus I")
                       && window->property("lectureName").toString()
                              == QStringLiteral("Derivatives")
                       && window->property("lectureDate").toString()
                              == QStringLiteral("2026-08-30"),
                   QStringLiteral("New-note form did not apply lecture details"))
        || !expect(blankLines.size() == 1
                       && blankLines.first().toMap().value(QStringLiteral("source"))
                              .toString().isEmpty(),
                   QStringLiteral("Ctrl+N did not create one blank row")))
        return 1;

    editorValue.clear();
    QMetaObject::invokeMethod(window, "activeEditorItem", Q_RETURN_ARG(QVariant, editorValue));
    editor = editorValue.value<QObject *>();
    if (!expect(editor && editor->property("activeFocus").toBool()
                    && editor->property("placeholderText").toString()
                           == QStringLiteral("Start typing…"),
                QStringLiteral("A new blank note did not show and focus its typing row")))
        return 1;

    editor->setProperty("text", QStringLiteral("set"));
    editor->setProperty("cursorPosition", 3);
    QVariant expandedSet;
    QMetaObject::invokeMethod(window, "handleSnippetTab", Q_RETURN_ARG(QVariant, expandedSet),
                              Q_ARG(QVariant, QVariant::fromValue(editor)),
                              Q_ARG(QVariant, 0));
    QTest::qWait(50);
    newLines.clear();
    QMetaObject::invokeMethod(window, "serializedLines", Q_RETURN_ARG(QVariant, newLines));
    if (!expect(expandedSet.toBool()
                    && newLines.toList().first().toMap().value(QStringLiteral("source"))
                           .toString()
                           == QStringLiteral("\\left\\{ x \\middle| condition \\right\\}")
                    && editor->property("selectedText").toString() == QStringLiteral("x"),
                QStringLiteral("Set-builder Tab snippet or its first stop failed")))
        return 1;

    QObject *commandList = window->findChild<QObject *>(QStringLiteral("commandList"));
    QMetaObject::invokeMethod(window, "updateCommandResults",
                              Q_ARG(QVariant, QStringLiteral("mängd")));
    if (!expect(commandList && commandList->property("count").toInt() >= 8,
                QStringLiteral("Ctrl+K set-theory search did not find the Swedish set entries")))
        return 1;
    QMetaObject::invokeMethod(window, "updateCommandResults",
                              Q_ARG(QVariant, QStringLiteral("gränsvärde")));
    if (!expect(commandList->property("count").toInt() >= 3,
                QStringLiteral("Ctrl+K Analysis 1 search did not find limit entries")))
        return 1;
    QMetaObject::invokeMethod(window, "clearSnippetStops");
    editor->setProperty("text", QString());
    editor->setProperty("cursorPosition", 0);
    QTest::qWait(50);

    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                      QPoint(window->width() / 2, window->height() - 50));
    QTest::qWait(100);
    editorValue.clear();
    QMetaObject::invokeMethod(window, "activeEditorItem", Q_RETURN_ARG(QVariant, editorValue));
    editor = editorValue.value<QObject *>();
    newLines.clear();
    QMetaObject::invokeMethod(window, "serializedLines", Q_RETURN_ARG(QVariant, newLines));
    if (!expect(editor && editor->property("activeFocus").toBool()
                    && newLines.toList().size() == 1,
                QStringLiteral("Clicking blank writing space did not focus the empty row")))
        return 1;

    QMetaObject::invokeMethod(editor, "forceActiveFocus");
    editor->setProperty("text", QStringLiteral("frac"));
    editor->setProperty("cursorPosition", 4);
    QVariant expanded;
    QMetaObject::invokeMethod(window, "handleSnippetTab", Q_RETURN_ARG(QVariant, expanded),
                              Q_ARG(QVariant, QVariant::fromValue(editor)),
                              Q_ARG(QVariant, 0));
    QTest::qWait(50);
    newLines.clear();
    QMetaObject::invokeMethod(window, "serializedLines", Q_RETURN_ARG(QVariant, newLines));
    if (!expect(expanded.toBool()
                    && newLines.toList().first().toMap().value(QStringLiteral("source"))
                           .toString() == QStringLiteral("\\frac{a}{b}")
                    && editor->property("selectedText").toString() == QStringLiteral("a"),
                QStringLiteral("frac Tab snippet or its first stop failed")))
        return 1;
    QTest::keyClick(window, Qt::Key_Tab);
    if (!expect(editor->property("selectedText").toString() == QStringLiteral("b"),
                QStringLiteral("Tab did not move to the next snippet stop (selected '%1', stop %2, focus %3)")
                    .arg(editor->property("selectedText").toString())
                    .arg(window->property("snippetStopIndex").toInt())
                    .arg(editor->property("activeFocus").toBool())))
        return 1;
    QTest::keyClick(window, Qt::Key_Tab, Qt::ShiftModifier);
    if (!expect(editor->property("selectedText").toString() == QStringLiteral("a")
                    && window->property("snippetStopIndex").toInt() == 0
                    && !titleField->property("activeFocus").toBool(),
                QStringLiteral("Shift+Tab did not return to the prior snippet field")))
        return 1;
    QTest::keyClick(window, Qt::Key_Tab);
    if (!expect(editor->property("selectedText").toString() == QStringLiteral("b"),
                QStringLiteral("Tab did not move forward after Shift+Tab")))
        return 1;
    QTest::keyClick(window, Qt::Key_Z, Qt::ControlModifier);
    QTest::qWait(80);
    newLines.clear();
    QMetaObject::invokeMethod(window, "serializedLines", Q_RETURN_ARG(QVariant, newLines));
    const QString undoSource = newLines.toList().first().toMap()
        .value(QStringLiteral("source")).toString();
    if (!expect(undoSource == QStringLiteral("frac"),
                QStringLiteral("Whole-note undo did not undo a snippet as one change: '%1'")
                    .arg(undoSource)))
        return 1;

    QObject *rowTypePopup = window->findChild<QObject *>(QStringLiteral("rowTypePopup"));
    if (!expect(rowTypePopup, QStringLiteral("Row type popup was not found")))
        return 1;
    QMetaObject::invokeMethod(rowTypePopup, "open");
    QTest::qWait(350);
    QObject *rowTypeList = window->findChild<QObject *>(QStringLiteral("rowTypeList"));
    if (!expect(rowTypeList, QStringLiteral("Row type list was not found"))
        || !expect(rowTypeList->property("optionTextColor") == window->property("textColor"),
                   QStringLiteral("Row type text does not use the visible theme color"))
        || !expect(rowTypeList->property("currentIndex").toInt() == 0,
                   QStringLiteral("Current row type has no selection")))
        return 1;
    QTest::keyClick(window, Qt::Key_Down);
    QTest::qWait(50);
    if (!expect(rowTypeList->property("currentIndex").toInt() == 1,
                QStringLiteral("Row type keyboard highlight did not move")))
        return 1;
    QTest::keyClick(window, Qt::Key_Up);
    QTest::qWait(50);
    if (!expect(rowTypeList->property("currentIndex").toInt() == 0,
                QStringLiteral("Up did not move back in the row type menu")))
        return 1;
    QTest::keyClick(window, Qt::Key_Down);
    QTest::keyClick(window, Qt::Key_Return);
    QTest::qWait(400);
    QVariant selectedTypeLines;
    QMetaObject::invokeMethod(window, "serializedLines",
                              Q_RETURN_ARG(QVariant, selectedTypeLines));
    if (!expect(!rowTypePopup->property("visible").toBool()
                    && selectedTypeLines.toList().first().toMap()
                           .value(QStringLiteral("kind")).toString()
                           == QStringLiteral("definition"),
                QStringLiteral("Enter did not apply and close the row type menu "
                               "(visible %1, kind %2, index %3)")
                    .arg(rowTypePopup->property("visible").toBool())
                    .arg(selectedTypeLines.toList().first().toMap()
                             .value(QStringLiteral("kind")).toString())
                    .arg(rowTypeList->property("currentIndex").toInt())))
        return 1;

    const QVariantMap typedLayoutData{
        {QStringLiteral("title"), QStringLiteral("Typed layout")},
        {QStringLiteral("lines"), QVariantList{
             QVariantMap{{QStringLiteral("source"), QStringLiteral("\\frac{a}{b}")},
                         {QStringLiteral("kind"), QStringLiteral("theorem")}},
             QVariantMap{{QStringLiteral("source"), QString()}}
         }}
    };
    QMetaObject::invokeMethod(window, "loadData", Q_ARG(QVariant, typedLayoutData),
                              Q_ARG(QVariant, QString()));
    QMetaObject::invokeMethod(window, "editLine", Q_ARG(QVariant, QVariant(1)));
    QMetaObject::invokeMethod(window, "renderAll");
    for (int attempt = 0; attempt < 80
            && window->property("pendingRenderCount").toInt() > 0; ++attempt)
        QTest::qWait(50);
    QTest::qWait(100);
    QQuickItem *theoremRow = nullptr;
    QMetaObject::invokeMethod(documentList, "itemAtIndex",
                              Q_RETURN_ARG(QQuickItem *, theoremRow), Q_ARG(int, 0));
    if (!expect(theoremRow, QStringLiteral("Rendered theorem row was not found"))
        || !expect(theoremRow->property("contentLeft").toReal()
                       >= theoremRow->property("pageLeft").toReal() + 16,
                   QStringLiteral("Typed row content overlaps its rail: page %1, content %2")
                       .arg(theoremRow->property("pageLeft").toReal())
                       .arg(theoremRow->property("contentLeft").toReal()))
        || !expect(theoremRow->property("height").toReal()
                       <= theoremRow->property("displayedContentHeight").toReal() + 48,
                   QStringLiteral("Typed row leaves excess space: row %1, visual %2")
                       .arg(theoremRow->property("height").toReal())
                       .arg(theoremRow->property("displayedContentHeight").toReal())))
        return 1;
    QMetaObject::invokeMethod(window, "editLine", Q_ARG(QVariant, QVariant(0)));
    QMetaObject::invokeMethod(window, "addCatchupMarker");
    newLines.clear();
    QMetaObject::invokeMethod(window, "serializedLines", Q_RETURN_ARG(QVariant, newLines));
    const QVariantList typedLines = newLines.toList();
    if (!expect(typedLines.first().toMap().value(QStringLiteral("kind"))
                    .toString() == QStringLiteral("theorem")
                    && typedLines.at(1).toMap().value(QStringLiteral("kind"))
                           .toString() == QStringLiteral("catchup"),
                QStringLiteral("Typed rows or catch-up marker were not stored")))
        return 1;

    qInfo() << "PASS: Aa keyboard navigation and Enter apply";
    return 0;
}
