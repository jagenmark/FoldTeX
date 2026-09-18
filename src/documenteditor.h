#pragma once

#include <QQuickPaintedItem>
#include <QImage>
#include <QTextDocument>
#include <QTimer>
#include <QVariantList>
#include <QSet>
#include "latexsyntax.h"
#include "spellchecker.h"

class Backend;

// A single text cursor over the complete source. The QTextDocument is a view:
// rendered objects never replace the canonical source or its row metadata.
class DocumentEditor : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QObject *backendApi READ backend WRITE setBackend)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(int activeRow READ activeRow NOTIFY cursorPositionChanged)
    Q_PROPERTY(int cursorPosition READ cursorPosition WRITE setCursorPosition NOTIFY cursorPositionChanged)
    Q_PROPERTY(int selectionStart READ selectionStart NOTIFY cursorPositionChanged)
    Q_PROPERTY(int selectionEnd READ selectionEnd NOTIFY cursorPositionChanged)
    Q_PROPERTY(QString selectedText READ selectedText NOTIFY cursorPositionChanged)
    Q_PROPERTY(int sourceCursor READ sourceCursor NOTIFY cursorPositionChanged)
    Q_PROPERTY(int sourceAnchor READ sourceAnchor NOTIFY cursorPositionChanged)
    Q_PROPERTY(QRectF cursorRectangle READ cursorRectangle NOTIFY layoutChanged)
    Q_PROPERTY(qreal contentHeight READ contentHeight NOTIFY layoutChanged)
    Q_PROPERTY(qreal scrollY READ scrollY WRITE setScrollY NOTIFY layoutChanged)
    Q_PROPERTY(qreal writingWidth READ writingWidth WRITE setWritingWidth NOTIFY layoutChanged)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor)
    Q_PROPERTY(QColor selectionColor READ selectionColor WRITE setSelectionColor)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily)
    Q_PROPERTY(int fontSize READ fontSize WRITE setFontSize)
    Q_PROPERTY(int displayMode READ displayMode WRITE setDisplayMode)
    Q_PROPERTY(QString completionPrefix READ completionPrefix NOTIFY cursorPositionChanged)
    Q_PROPERTY(bool completionVisible MEMBER m_completionVisible)
    Q_PROPERTY(int renderedCount READ renderedCount NOTIFY layoutChanged)
    Q_PROPERTY(int pendingRenderCount READ pendingRenderCount NOTIFY layoutChanged)
    Q_PROPERTY(QString errorHint READ errorHint NOTIFY layoutChanged)
    Q_PROPERTY(QString spellLanguage READ spellLanguage WRITE setSpellLanguage NOTIFY spellingChanged)
    Q_PROPERTY(bool spellcheckEnabled READ spellcheckEnabled WRITE setSpellcheckEnabled NOTIFY spellingChanged)
    Q_PROPERTY(QStringList spellingIgnored READ spellingIgnored WRITE setSpellingIgnored NOTIFY spellingChanged)
    Q_PROPERTY(QVariantList spellingIssues READ spellingIssues NOTIFY spellingChanged)
    Q_PROPERTY(QString spellingError READ spellingError NOTIFY spellingChanged)
public:
    explicit DocumentEditor(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;
    QObject *backend() const;
    void setBackend(QObject *backend);
    QString text() const;
    void setText(const QString &text);
    int activeRow() const;
    int cursorPosition() const;
    void setCursorPosition(int position);
    int selectionStart() const;
    int selectionEnd() const;
    QString selectedText() const;
    int sourceCursor() const { return m_cursor; }
    int sourceAnchor() const { return m_anchor; }
    QRectF cursorRectangle() const;
    qreal contentHeight() const;
    qreal scrollY() const { return m_scrollY; }
    void setScrollY(qreal value);
    qreal writingWidth() const { return m_writingWidth; }
    void setWritingWidth(qreal value);
    QColor textColor() const { return m_color; }
    void setTextColor(const QColor &value);
    QColor selectionColor() const { return m_selection; }
    void setSelectionColor(const QColor &value);
    QString fontFamily() const { return m_fontFamily; }
    void setFontFamily(const QString &value);
    int fontSize() const { return m_fontSize; }
    void setFontSize(int value);
    int displayMode() const { return m_mode; }
    void setDisplayMode(int value);
    QString completionPrefix() const;
    int renderedCount() const { return m_renderedCount; }
    int pendingRenderCount() const { return m_jobs.size() + (m_pendingKey.isEmpty() ? 0 : 1); }
    QString errorHint() const;
    QString spellLanguage() const { return m_spellLanguage; }
    void setSpellLanguage(const QString &language);
    bool spellcheckEnabled() const { return m_spellEnabled; }
    void setSpellcheckEnabled(bool enabled);
    QStringList spellingIgnored() const { return m_spellIgnored; }
    void setSpellingIgnored(const QStringList &words);
    QVariantList spellingIssues() const { return m_spellingIssues; }
    QString spellingError() const { return m_spellError; }
    Q_INVOKABLE void correctSpelling(const QString &replacement);
    Q_INVOKABLE void ignoreSpelling();
    Q_INVOKABLE QRectF spellingRectangle(int sourcePosition) const;
    Q_INVOKABLE void requestSpelling(int sourcePosition, qreal x, qreal y);
    Q_INVOKABLE void loadRows(const QVariantList &rows);
    Q_INVOKABLE QVariantList rows() const { return m_rows; }
    Q_INVOKABLE QString sourceText() const { return m_source; }
    Q_INVOKABLE void editRow(int row, int position = -1);
    Q_INVOKABLE void select(int start, int end);
    Q_INVOKABLE void selectSource(int anchor, int cursor);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void insert(int position, const QString &text);
    Q_INVOKABLE void remove(int start, int end);
    Q_INVOKABLE void replaceRange(int start, int end, const QString &text, int anchor, int cursor);
    Q_INVOKABLE void pasteText(const QString &text);
    Q_INVOKABLE void copy();
    Q_INVOKABLE void cut();
    Q_INVOKABLE void scrollBy(qreal amount);
    Q_INVOKABLE void revealRow(int row);
    Q_INVOKABLE bool inMath() const;
    Q_INVOKABLE QVariantMap mathAtCursor() const;
    Q_INVOKABLE void insertTemplate(const QString &value);
    Q_INVOKABLE void refresh() { rebuild(); }
    Q_INVOKABLE QRectF renderedSourceRectangle(int sourcePosition) const;
    Q_INVOKABLE void cancelRenders() { m_renderTimer.stop(); m_jobs.clear(); m_queued.clear(); emit layoutChanged(); }
signals:
    void rowsEdited(const QVariantList &rows);
    void editStarted(bool separateUndo);
    void textChanged();
    void cursorPositionChanged();
    void layoutChanged();
    void tabPressed(bool backward);
    void blockBreakRequested();
    void insertRowRequested();
    void undoRequested();
    void redoRequested();
    void contextMenuRequested(int row, qreal x, qreal y);
    void editModeRequested();
    void completionMove(int direction);
    void completionDismissed();
    void spellingChanged();
    void spellingMenuRequested(const QString &word, const QStringList &suggestions, bool loading, qreal x, qreal y);
    void spellingDismissed();
    void spellingIgnoreRequested(const QString &word);
protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
private:
    struct Render { QString url; QString error; QImage image; QSizeF size; QVariantList hits; };
    struct Job { QString key; QString body; bool textMode = false; };
    struct Folded { int visual, start, end; QString key; QVector<int> positions; QSizeF size; bool display; };
    QVector<Folded> m_folded;
    LatexSyntax::Preamble m_preamble;
    QString m_renderPreamble;
    QVariantList m_rows;
    QVector<int> m_offsets;
    QString m_source;
    QTextDocument m_document;
    QVector<int> m_positions;
    QVector<QPair<int, int>> m_readyRanges;
    Backend *m_backend = nullptr;
    QHash<QString, Render> m_renders;
    QList<Job> m_jobs;
    QSet<QString> m_queued;
    QString m_pendingKey;
    int m_requestId = -1000000;
    int m_cursor = 0;
    int m_anchor = 0;
    qreal m_scrollY = 0;
    qreal m_writingWidth = 760;
    QColor m_color = QColor("#eeeeee");
    QColor m_selection = QColor("#186a9a");
    QString m_fontFamily = "monospace";
    int m_fontSize = 17;
    int m_mode = 0;
    int m_renderedCount = 0;
    bool m_dragging = false;
    bool m_rebuilding = false;
    bool m_completionVisible = false;
    bool m_caretVisible = true;
    qreal m_preferredX = -1;
    QString m_preedit;
    QPointF m_dragPoint;
    QTimer m_renderTimer;
    QTimer m_refreshTimer;
    QTimer m_blinkTimer;
    QTimer m_dragTimer;
    SpellChecker m_spellChecker;
    QTimer m_spellTimer;
    QString m_spellLanguage = "sv";
    QStringList m_spellIgnored;
    bool m_spellEnabled = true;
    QVariantList m_spellingIssues;
    QString m_spellError;
    QVariantMap m_spellingTarget;
    QStringList m_spellingSuggestions;
    int m_suggestionRequest = 0;
    QPointF m_spellingPopupPoint, m_pressPoint;
    int m_pressedWord = -1;
    int m_emptyDollarAt = -1;
    bool handleDollarInput();
    bool shrinkDisplayDollars(bool backward);
    void scheduleSpelling();
    QVariantMap spellingAt(int sourcePosition) const;
    void rebuildSource();
    void rebuild(bool preserveViewport = true, int anchorPosition = -1, qreal anchorY = 0);
    void nextRender();
    void replaceSource(int from, int to, const QString &value, bool split, bool separateUndo, int anchor = -1, int cursor = -1);
    void moveCursor(int position, bool keepAnchor = false, bool keepX = false);
    int rowAt(int sourcePosition) const;
    int rowTextOffset(int row) const;
    int visualPosition(int sourcePosition) const;
    int sourcePosition(int visualPosition) const;
    int hitTest(QPointF point) const;
    qreal leftMargin() const;
    QRectF documentCursorRect() const;
    QRectF documentPositionRect(int sourcePosition) const;
    QRectF foldedRectangle(const Folded &folded) const;
    void ensureCursorVisible();
};
