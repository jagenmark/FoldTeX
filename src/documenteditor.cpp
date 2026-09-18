#include "documenteditor.h"
#include "backend.h"
#include "latexsyntax.h"

#include <QAbstractTextDocumentLayout>
#include <QClipboard>
#include <QCursor>
#include <QGuiApplication>
#include <QImageReader>
#include <QFontMetricsF>
#include <QInputMethod>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextImageFormat>
#include <QTextLayout>
#include <QUrl>
#include <algorithm>
#include <limits>

namespace {
bool escapedAt(const QString &source, int position) {
    int slashes = 0;
    while (position > 0 && source[--position] == '\\') ++slashes;
    return slashes % 2;
}
bool inComment(const QString &source, int position) {
    for (int i = source.lastIndexOf('\n', qMax(0, position - 1)) + 1; i < position; ++i)
        if (source[i] == '%' && !escapedAt(source, i)) return true;
    return false;
}
QString rowSource(const QVariant &value) {
    const auto row = value.toMap();
    return (row.value("kind") == "image" ? QString(QChar::ObjectReplacementCharacter) : QString()) + row.value("source").toString();
}
QVariantMap emptyRow() {
    return {{"source", ""}, {"kind", "normal"}, {"mode", "latex"}, {"label", ""}, {"asset", ""}, {"slide", -1}};
}
QString renderKey(const QString &body, const QColor &color, int size, const QString &preamble, bool textMode = false) {
    return body + QChar(0x1f) + color.name() + QChar(0x1f) + QString::number(size) + QChar(0x1f) + preamble + (textMode ? "\ntext" : "");
}
}

DocumentEditor::DocumentEditor(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    setFlag(ItemAcceptsInputMethod, true);
    setActiveFocusOnTab(true);
    setClip(true);
    setCursor(QCursor(Qt::IBeamCursor));
    m_document.setDocumentMargin(0);
    m_document.setUndoRedoEnabled(false);
    m_renderTimer.setSingleShot(true);
    m_renderTimer.setInterval(250);
    connect(&m_renderTimer, &QTimer::timeout, this, &DocumentEditor::nextRender);
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(180);
    connect(&m_refreshTimer, &QTimer::timeout, this, [this] { rebuild(); });
    m_blinkTimer.setInterval(530);
    connect(&m_blinkTimer, &QTimer::timeout, this, [this] { m_caretVisible = !m_caretVisible; update(); });
    m_dragTimer.setInterval(30);
    m_spellTimer.setSingleShot(true);
    m_spellTimer.setInterval(450);
    connect(&m_spellTimer, &QTimer::timeout, this, [this] {
        if (m_spellEnabled) m_spellChecker.check(m_rows, m_spellLanguage, m_spellIgnored);
    });
    connect(&m_spellChecker, &SpellChecker::checked, this, [this](const QVariantList &issues, const QString &error) {
        m_spellingIssues = issues; m_spellError = error; update(); emit spellingChanged();
    });
    connect(&m_spellChecker, &SpellChecker::suggested, this, [this](int request, const QStringList &suggestions) {
        if (request != m_suggestionRequest || m_spellingTarget.isEmpty()) return;
        m_spellingSuggestions = suggestions;
        emit spellingMenuRequested(m_spellingTarget.value("word").toString(), suggestions, false, m_spellingPopupPoint.x(), m_spellingPopupPoint.y());
    });
    connect(&m_dragTimer, &QTimer::timeout, this, [this] {
        if (m_dragPoint.y() < 20) scrollBy(-18);
        else if (m_dragPoint.y() > height() - 20) scrollBy(18);
        else return;
        moveCursor(hitTest(m_dragPoint), true);
    });
    loadRows({emptyRow()});
}

QObject *DocumentEditor::backend() const { return m_backend; }
void DocumentEditor::setBackend(QObject *value) {
    if (m_backend) disconnect(m_backend, nullptr, this, nullptr);
    m_backend = qobject_cast<Backend *>(value);
    if (m_backend) connect(m_backend, &Backend::renderFinished, this, [this](int id, const QVariantMap &result) {
        if (id != m_requestId || m_pendingKey.isEmpty()) return;
        Render render;
        render.hits = result.value("hits").toList();
        render.url = result.value("url").toString();
        render.error = result.value("error").toString();
        if (!render.url.isEmpty()) {
            QImageReader reader(QUrl(render.url).toLocalFile());
            render.size = reader.size();
            if (render.size.isValid()) reader.setScaledSize((render.size * 2).toSize());
            render.image = reader.read();
        }
        m_renders.insert(m_pendingKey, render);
        m_queued.remove(m_pendingKey);
        m_pendingKey.clear();
        rebuild();
        m_renderTimer.start(0);
    });
    rebuild();
}

int DocumentEditor::rowAt(int position) const {
    auto it = std::upper_bound(m_offsets.begin(), m_offsets.end(), position);
    return qBound(0, int(it - m_offsets.begin()) - 1, qMax(0, int(m_rows.size()) - 1));
}
int DocumentEditor::activeRow() const { return rowAt(m_cursor); }
int DocumentEditor::rowTextOffset(int row) const {
    return m_offsets.value(row) + (m_rows.value(row).toMap().value("kind") == "image" ? 1 : 0);
}
QString DocumentEditor::text() const { return m_rows.value(activeRow()).toMap().value("source").toString(); }
int DocumentEditor::cursorPosition() const { return qBound(0, m_cursor - rowTextOffset(activeRow()), int(text().size())); }
int DocumentEditor::selectionStart() const { return qMax(0, qMin(m_cursor, m_anchor) - rowTextOffset(activeRow())); }
int DocumentEditor::selectionEnd() const { return qMin(int(text().size()), qMax(m_cursor, m_anchor) - rowTextOffset(activeRow())); }
QString DocumentEditor::selectedText() const { return m_source.mid(qMin(m_cursor, m_anchor), qAbs(m_cursor - m_anchor)); }

void DocumentEditor::rebuildSource() {
    scheduleSpelling();
    m_preamble = LatexSyntax::documentPreamble(m_rows);
    m_renderPreamble = m_preamble.documentClass + m_preamble.source;
    m_source.clear(); m_offsets.clear();
    for (const auto &row : m_rows) {
        if (!m_offsets.isEmpty()) m_source += '\n';
        m_offsets.append(m_source.size()); m_source += rowSource(row);
    }
    m_cursor = qBound(0, m_cursor, int(m_source.size()));
    m_anchor = qBound(0, m_anchor, int(m_source.size()));
}
void DocumentEditor::loadRows(const QVariantList &rows) {
    if (rows == m_rows) return;
    const int cursorRow = rowAt(m_cursor), anchorRow = rowAt(m_anchor);
    const int cursorColumn = m_cursor - m_offsets.value(cursorRow);
    const int anchorColumn = m_anchor - m_offsets.value(anchorRow);
    const qreal rowY = documentPositionRect(m_offsets.value(cursorRow)).y();
    m_emptyDollarAt = -1;
    m_spellingIssues.clear();
    m_rows = rows.isEmpty() ? QVariantList{emptyRow()} : rows;
    rebuildSource();
    const int nextCursorRow = qMin(cursorRow, int(m_rows.size()) - 1);
    const int nextAnchorRow = qMin(anchorRow, int(m_rows.size()) - 1);
    m_cursor = m_offsets[nextCursorRow] + qMin(cursorColumn, int(rowSource(m_rows[nextCursorRow]).size()));
    m_anchor = m_offsets[nextAnchorRow] + qMin(anchorColumn, int(rowSource(m_rows[nextAnchorRow]).size()));
    rebuild(true, m_offsets[nextCursorRow], rowY);
    emit textChanged(); emit cursorPositionChanged();
}
void DocumentEditor::setText(const QString &value) {
    if (value == text()) return;
    const int from = rowTextOffset(activeRow());
    replaceSource(from, from + text().size(), value, false, true);
}
void DocumentEditor::setCursorPosition(int position) { moveCursor(rowTextOffset(activeRow()) + qBound(0, position, int(text().size()))); }
void DocumentEditor::editRow(int row, int position) {
    row = qBound(0, row, int(m_rows.size()) - 1);
    const int length = m_rows[row].toMap().value("source").toString().size();
    moveCursor(rowTextOffset(row) + (position < 0 ? length : qBound(0, position, length)));
    forceActiveFocus();
}
void DocumentEditor::select(int start, int end) { selectSource(rowTextOffset(activeRow()) + start, rowTextOffset(activeRow()) + end); }
void DocumentEditor::selectSource(int anchor, int cursor) {
    m_anchor = qBound(0, anchor, int(m_source.size()));
    moveCursor(cursor, true);
}
void DocumentEditor::selectAll() { selectSource(0, m_source.size()); }
void DocumentEditor::insert(int position, const QString &value) {
    const int at = rowTextOffset(activeRow()) + qBound(0, position, int(text().size()));
    replaceSource(at, at, value, false, true);
}
void DocumentEditor::remove(int start, int end) {
    const int offset = rowTextOffset(activeRow());
    replaceSource(offset + start, offset + end, "", false, true);
}
void DocumentEditor::replaceRange(int start, int end, const QString &value, int anchor, int cursor) {
    const int offset = rowTextOffset(activeRow());
    replaceSource(offset + start, offset + end, value, false, true, offset + anchor, offset + cursor);
}
void DocumentEditor::pasteText(const QString &value) {
    QString normalized = value; normalized.replace("\r\n", "\n").replace('\r', '\n');
    replaceSource(qMin(m_cursor, m_anchor), qMax(m_cursor, m_anchor), normalized, normalized.contains('\n'), true);
}
void DocumentEditor::copy() { if (m_cursor != m_anchor) QGuiApplication::clipboard()->setText(selectedText()); }
void DocumentEditor::cut() {
    copy();
    if (m_cursor != m_anchor) replaceSource(qMin(m_cursor, m_anchor), qMax(m_cursor, m_anchor), "", false, true);
}
void DocumentEditor::insertTemplate(const QString &value) {
    QString insertion = value;
    if (!inMath() && !LatexSyntax::mathSpans(insertion, "latex").size()) insertion = "$" + insertion + "$";
    pasteText(insertion);
}
void DocumentEditor::replaceSource(int from, int to, const QString &value, bool split, bool separateUndo, int anchor, int cursor) {
    m_emptyDollarAt = -1;
    from = qBound(0, from, int(m_source.size())); to = qBound(from, to, int(m_source.size()));
    if (from == to && value.isEmpty()) return;
    // Capture a stable source location before changing either the text or its
    // cursor offsets. The old layout cannot resolve offsets in the new source.
    const qreal editAnchorY = documentPositionRect(from).y();
    QVariantList adjustedIssues;
    for (const auto &entry : m_spellingIssues) {
        auto issue = entry.toMap();
        int start = issue.value("start").toInt(), end = issue.value("end").toInt();
        if (start <= to && end >= from) continue;
        if (start > to) { start += value.size() - (to - from); end += value.size() - (to - from); }
        issue["start"] = start; issue["end"] = end; adjustedIssues.append(issue);
    }
    m_spellingIssues = adjustedIssues;
    emit editStarted(separateUndo);
    const int first = rowAt(from), last = rowAt(to);
    QVariantMap metadata = m_rows[first].toMap();
    const int visualFrom = visualPosition(from), visualTo = visualPosition(to);
    static const QRegularExpression structureCharacters(R"([\n\\{}$])");
    bool localEdit = !split && first == last && metadata.value("kind") != "image"
        && !structureCharacters.match(value).hasMatch()
        && !structureCharacters.match(m_source.mid(from, to - from)).hasMatch()
        && sourcePosition(visualFrom) == from && sourcePosition(visualTo) == to;
    for (int i = visualFrom; localEdit && i < visualTo; ++i)
        if (m_positions[i + 1] - m_positions[i] != 1) localEdit = false;
    QString combined = m_source.mid(m_offsets[first], from - m_offsets[first]) + value
        + m_source.mid(to, m_offsets[last] + rowSource(m_rows[last]).size() - to);
    if (metadata.value("kind") == "image" && !combined.startsWith(QChar::ObjectReplacementCharacter)) metadata = emptyRow();
    QStringList parts;
    int partStart = 0;
    if (split) {
        const auto spans = LatexSyntax::mathSpans(combined, "latex");
        int spanIndex = 0;
        const int insertedStart = from - m_offsets[first];
        // Only newly inserted newlines create paragraphs. Existing Shift+Enter
        // breaks remain in their block, including when text is pasted beside them.
        for (int i = insertedStart; i < insertedStart + value.size(); ++i) {
            while (spanIndex < spans.size() && spans[spanIndex].end <= i) ++spanIndex;
            const bool insideMath = spanIndex < spans.size() && spans[spanIndex].start <= i && i < spans[spanIndex].end;
            if (combined[i] == QLatin1Char('\n') && !insideMath) {
                parts.append(combined.mid(partStart, i - partStart)); partStart = i + 1;
            }
        }
    }
    parts.append(combined.mid(partStart));
    QVariantList next = m_rows.mid(0, first);
    for (int i = 0; i < parts.size(); ++i) {
        auto row = metadata;
        QString source = parts[i];
        if (i > 0) {
            row["asset"] = ""; row["slide"] = -1;
            if (row.value("kind") == "heading" || row.value("kind") == "subheading" || row.value("kind") == "image") row = emptyRow();
        }
        if (row.value("kind") == "image" && source.startsWith(QChar::ObjectReplacementCharacter)) source.remove(0, 1);
        row["source"] = source; next.append(row);
    }
    next.append(m_rows.mid(last + 1));
    m_rows = next;
    m_cursor = cursor < 0 ? from + value.size() : cursor;
    m_anchor = anchor < 0 ? m_cursor : anchor;
    rebuildSource();
    if (localEdit) {
        // Ordinary typing updates only the affected text range. TeX parsing and
        // preview replacement follow after idle time, not on every keystroke.
        QTextCursor edit(&m_document);
        edit.setPosition(visualFrom); edit.setPosition(visualTo, QTextCursor::KeepAnchor);
        edit.insertText(value);
        m_positions.remove(visualFrom + 1, visualTo - visualFrom);
        for (int i = 1; i <= value.size(); ++i) m_positions.insert(visualFrom + i, from + i);
        const int delta = value.size() - (to - from);
        for (int i = visualFrom + value.size() + 1; i < m_positions.size(); ++i) m_positions[i] += delta;
        for (int i = m_readyRanges.size() - 1; i >= 0; --i) {
            auto &range = m_readyRanges[i];
            if (range.first <= to && range.second >= from) m_readyRanges.remove(i);
            else if (range.first > to) { range.first += delta; range.second += delta; }
        }
        for (auto &folded : m_folded) if (folded.start > to) {
            folded.start += delta; folded.end += delta; folded.visual += delta;
        }
        m_jobs.clear(); m_renderTimer.stop();
        m_refreshTimer.start();
        update(); emit layoutChanged();
    } else rebuild(true, from, editAnchorY);
    ensureCursorVisible();
    emit rowsEdited(m_rows); emit textChanged(); emit cursorPositionChanged();
    m_caretVisible = true; m_renderTimer.start(250);
    QGuiApplication::inputMethod()->update(Qt::ImQueryAll);
}

int DocumentEditor::visualPosition(int source) const {
    auto it = std::lower_bound(m_positions.begin(), m_positions.end(), source);
    return qBound(0, int(it - m_positions.begin()), m_document.characterCount() - 1);
}
int DocumentEditor::sourcePosition(int visual) const { return m_positions.value(qBound(0, visual, int(m_positions.size()) - 1), 0); }
qreal DocumentEditor::leftMargin() const { return qMax(16.0, (width() - m_writingWidth) / 2); }
qreal DocumentEditor::contentHeight() const { return m_document.size().height() + 130; }
QRectF DocumentEditor::documentCursorRect() const { return documentPositionRect(m_cursor); }
QRectF DocumentEditor::documentPositionRect(int source) const {
    const int pos = visualPosition(source);
    const QTextBlock block = m_document.findBlock(pos);
    if (!block.isValid() || !block.layout()) return {};
    const QTextLine line = block.layout()->lineForTextPosition(pos - block.position());
    if (!line.isValid()) return {};
    const QPointF origin = m_document.documentLayout()->blockBoundingRect(block).topLeft();
    return QRectF(origin.x() + line.cursorToX(pos - block.position()), origin.y() + line.y(), 1.5, line.height());
}
QRectF DocumentEditor::cursorRectangle() const { return documentCursorRect().translated(leftMargin(), 32 - m_scrollY); }
void DocumentEditor::setScrollY(qreal value) {
    value = qBound(0.0, value, qMax(0.0, contentHeight() - height()));
    if (qFuzzyCompare(m_scrollY, value)) return;
    m_scrollY = value; update(); emit layoutChanged();
}
void DocumentEditor::scrollBy(qreal amount) { setScrollY(m_scrollY + amount); }
void DocumentEditor::revealRow(int row) {
    const auto block = m_document.findBlock(visualPosition(m_offsets.value(row)));
    const auto rect = m_document.documentLayout()->blockBoundingRect(block);
    if (rect.top() < m_scrollY || rect.bottom() > m_scrollY + height() - 60) setScrollY(rect.top() - 60);
}
void DocumentEditor::ensureCursorVisible() {
    const auto rect = cursorRectangle();
    if (rect.bottom() > height() - 36) setScrollY(m_scrollY + rect.bottom() - height() + 36);
    else if (rect.top() < 22) setScrollY(m_scrollY + rect.top() - 22);
}
void DocumentEditor::setWritingWidth(qreal value) { if (m_writingWidth != value) { m_writingWidth = value; rebuild(); } }
void DocumentEditor::setTextColor(const QColor &value) { if (m_color != value) { m_color = value; rebuild(); } }
void DocumentEditor::setSelectionColor(const QColor &value) { m_selection = value; update(); }
void DocumentEditor::setFontFamily(const QString &value) { if (m_fontFamily != value) { m_fontFamily = value; rebuild(); } }
void DocumentEditor::setFontSize(int value) { if (m_fontSize != value) { m_fontSize = value; rebuild(); } }
void DocumentEditor::setDisplayMode(int value) { if (m_mode != value) { m_mode = value; rebuild(); } }
void DocumentEditor::geometryChange(const QRectF &next, const QRectF &old) { QQuickPaintedItem::geometryChange(next, old); rebuild(); }

void DocumentEditor::rebuild(bool preserveViewport, int anchorPosition, qreal anchorY) {
    if (m_rebuilding) return;
    m_refreshTimer.stop();
    m_rebuilding = true;
    const qreal priorCaretY = anchorPosition < 0 ? documentCursorRect().y() : anchorY;
    const int stablePosition = anchorPosition < 0 ? m_cursor : anchorPosition;
    const bool anchorViewport = preserveViewport && m_scrollY > 0 && !m_dragging;
    QFont font(m_fontFamily); font.setPixelSize(m_fontSize);
    m_document.setLayoutEnabled(false);
    m_document.clear(); m_document.setDefaultFont(font);
    m_document.setTextWidth(qMax(100.0, qMin(m_writingWidth, width() - 32)));
    QTextOption option; option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_document.setDefaultTextOption(option);
    m_positions = {0}; m_renderedCount = 0; m_readyRanges.clear(); m_folded.clear();
    // Queue only the latest version of each expression. One TeX process is
    // active at a time, so typing cannot create an unbounded work backlog.
    m_jobs.clear(); m_queued.clear(); if (!m_pendingKey.isEmpty()) m_queued.insert(m_pendingKey);
    QTextCursor out(&m_document);
    auto appendText = [&](const QString &s, int start, const QTextCharFormat &format) {
        out.insertText(s, format);
        for (int i = 1; i <= s.size(); ++i) m_positions.append(start + i);
    };
    static const QRegularExpression sectionPattern(R"(^\\(section|subsection|subsubsection)\{(.*)\}$)");
    for (int r = 0; r < m_rows.size(); ++r) {
        const auto row = m_rows[r].toMap();
        const QString source = rowSource(row);
        const QString kind = row.value("kind").toString();
        const int offset = m_offsets[r];
        QTextCharFormat format; format.setForeground(m_color); format.setFont(font);
        const auto markup = row.value("mode", "auto") == "text" ? QVector<LatexSyntax::TextMarkup>{} : LatexSyntax::textMarkup(source);
        const auto breaks = row.value("mode", "auto") == "text" ? QMap<int, int>{} : LatexSyntax::textLineBreaks(source);
        QVector<bool> hidden(markup.isEmpty() ? 0 : source.size(), false);
        QVector<QTextCharFormat> styles(markup.isEmpty() ? 0 : source.size(), format);
        for (const auto &mark : markup) {
            const int start = offset + mark.start, end = offset + mark.end;
            m_readyRanges.append({start, end});
            const bool editing = m_mode != 2 && ((m_cursor >= start && m_cursor <= end)
                || (m_cursor != m_anchor && qMin(m_cursor, m_anchor) < end && qMax(m_cursor, m_anchor) > start));
            if (m_mode == 1 || editing) continue;
            for (int i = mark.start; i < mark.contentStart; ++i) hidden[i] = true;
            for (int i = mark.contentEnd; i < mark.end; ++i) hidden[i] = true;
            for (int i = mark.contentStart; i < mark.contentEnd; ++i) {
                auto &style = styles[i];
                if (mark.command == "textbf") style.setFontWeight(QFont::Bold);
                else if (mark.command == "textmd") style.setFontWeight(QFont::Normal);
                else if (mark.command == "textit") style.setFontItalic(true);
                else if (mark.command == "emph") style.setFontItalic(!style.fontItalic());
                else if (mark.command == "textup") style.setFontItalic(false);
                else if (mark.command == "underline") style.setFontUnderline(true);
                else if (mark.command == "textsc") style.setFontCapitalization(QFont::SmallCaps);
                else if (mark.command == "textnormal") { style = format; }
                else style.setFontFamilies({mark.command == "texttt" ? "monospace" : mark.command == "textsf" ? "sans-serif" : "serif"});
            }
        }
        auto appendProse = [&](int start, int end) {
            if (markup.isEmpty() && breaks.isEmpty()) { appendText(source.mid(start, end - start), offset + start, format); return; }
            for (int i = start; i < end;) {
                const auto lineBreak = breaks.constFind(i);
                if (lineBreak != breaks.cend() && lineBreak.value() <= end) {
                    const int from = offset + i, to = from + 2;
                    m_readyRanges.append({from, to});
                    const bool editing = m_mode != 2 && ((m_cursor >= from && m_cursor <= to)
                        || (m_cursor != m_anchor && qMin(m_cursor, m_anchor) < to && qMax(m_cursor, m_anchor) > from));
                    if (m_mode != 1 && !editing) {
                        i = lineBreak.value();
                        // At row end the existing paragraph boundary already
                        // supplies the break; do not create another empty line.
                        if (i < source.size()) {
                            out.insertText(QString(QChar::LineSeparator), styles.isEmpty() ? format : styles[lineBreak.key()]);
                            m_positions.append(offset + i);
                        } else m_positions.last() = offset + i;
                        continue;
                    }
                }
                if (!hidden.isEmpty() && hidden[i]) { m_positions.last() = offset + ++i; continue; }
                int next = i + 1;
                while (next < end && !breaks.contains(next) && (hidden.isEmpty() || (!hidden[next] && styles[next] == styles[i]))) ++next;
                appendText(source.mid(i, next - i), offset + i, styles.isEmpty() ? format : styles[i]); i = next;
            }
        };
        QTextBlockFormat block; block.setBottomMargin(3); block.setLineHeight(125, QTextBlockFormat::ProportionalHeight);
        const auto section = sectionPattern.match(source);
        if (kind == "heading" || kind == "subheading" || section.hasMatch()) {
            format.setFontWeight(QFont::DemiBold);
            format.setProperty(QTextFormat::FontPixelSize, m_fontSize + (kind == "subheading" || section.captured(1) == "subsection" ? 3 : 7));
            block.setTopMargin(16); block.setBottomMargin(8);
        }
        if (kind == "bullet" || kind == "numbered") block.setLeftMargin(24);
        else if (kind != "normal" && kind != "heading" && kind != "subheading" && kind != "image") {
            block.setLeftMargin(16);
            if (r == 0 || m_rows[r - 1].toMap().value("kind") != kind) block.setTopMargin(m_fontSize + 14);
        }
        if (r) { out.insertBlock(block, format); m_positions.append(offset); }
        else out.setBlockFormat(block);
        if (m_preamble.rows.contains(r)) {
            QTextCharFormat setupFormat = format; setupFormat.setForeground(m_color.darker(135));
            appendText(source, offset, setupFormat); continue;
        }
        if (kind == "image") {
            QImage image(row.value("asset").toString());
            if (!image.isNull()) {
                const QString name = "figure-" + QString::number(r);
                m_document.addResource(QTextDocument::ImageResource, QUrl(name), image);
                QSizeF size = image.size(); size.scale(m_document.textWidth(), 700, Qt::KeepAspectRatio);
                QTextImageFormat img; img.setName(name); img.setWidth(size.width()); img.setHeight(size.height());
                out.insertImage(img); m_positions.append(offset + 1);
            } else appendText(QString(QChar::ObjectReplacementCharacter), offset, format);
            if (source.size() > 1) { out.insertText(QString(QChar::LineSeparator), format); m_positions.append(offset + 1); }
            appendText(source.mid(1), offset + 1, format);
            continue;
        }
        if (section.hasMatch()) m_readyRanges.append({offset, offset + int(source.size())});
        if (section.hasMatch() && m_mode != 1 && (m_mode == 2 || activeRow() != r)) {
            m_positions.last() = offset + section.capturedStart(2);
            appendText(section.captured(2), offset + section.capturedStart(2), format);
            m_positions.last() = offset + source.size();
            continue;
        }
        const auto spans = LatexSyntax::previewSpans(source, (kind == "heading" || kind == "subheading") ? "text" : row.value("mode", "auto").toString(), m_preamble.source);
        int at = 0;
        for (const auto &span : spans) {
            appendProse(at, span.start);
            int resume = span.end;
            const int start = offset + span.start, end = offset + span.end;
            const bool selected = m_cursor != m_anchor && qMin(m_cursor, m_anchor) < end && qMax(m_cursor, m_anchor) > start;
            const bool editing = m_mode != 2 && ((m_cursor >= start && m_cursor <= end) || selected);
            QString body = span.textMode ? span.body : LatexSyntax::renderBody(span.body);
            if (!span.textMode && !span.display) body.prepend("\\textstyle ");
            const QString key = renderKey(body, m_color, m_fontSize, m_renderPreamble, span.textMode);
            if (m_preamble.complete && span.complete && !m_renders.contains(key) && !m_queued.contains(key)) {
                m_jobs.append({key, body, span.textMode}); m_queued.insert(key);
            }
            const auto render = m_renders.constFind(key);
            if (render != m_renders.cend() && !render->image.isNull()) m_readyRanges.append({start, end});
            if (span.complete && m_mode != 1 && !editing && render != m_renders.cend() && !render->image.isNull()) {
                // Each display formula gets its own centered visual paragraph.
                // Synthetic paragraph boundaries map back to the same source.
                if (span.display) {
                    if (!out.atBlockStart()) { out.insertBlock(block, format); m_positions.append(start); }
                    QTextBlockFormat centered = block;
                    centered.setAlignment(Qt::AlignHCenter);
                    out.setBlockFormat(centered);
                }
                const QString name = "math-" + QString::number(r) + "-" + QString::number(span.start);
                m_document.addResource(QTextDocument::ImageResource, QUrl(name), render->image);
                const qreal scale = qMin(1.0, (m_document.textWidth() - block.leftMargin()) / qMax(1.0, render->size.width()));
                QTextImageFormat img; img.setName(name);
                img.setWidth(render->size.width() * scale); img.setHeight(render->size.height() * scale);
                img.setVerticalAlignment(QTextCharFormat::AlignMiddle);
                QVector<int> bodyPositions;
                if (span.textMode) { for (int i = 0; i <= body.size(); ++i) bodyPositions.append(i); }
                else {
                    LatexSyntax::renderBody(source.mid(span.start, span.end - span.start), &bodyPositions);
                    if (!span.display) bodyPositions = QVector<int>(11, bodyPositions.value(0)) + bodyPositions;
                }
                m_folded.append({out.position(), start, end, key, bodyPositions, QSizeF(img.width(), img.height()), span.display});
                out.insertImage(img); m_positions.append(end); ++m_renderedCount;
                if (span.display && resume < source.size()) {
                    // Reuse a real source newline instead of adding a blank line.
                    if (source[resume] == '\n') ++resume;
                    out.insertBlock(block, format); m_positions.append(offset + resume);
                }
            } else {
                QTextCharFormat mathFormat = format;
                mathFormat.setForeground(m_color.lighter(115));
                if (span.complete && !editing && render != m_renders.cend() && !render->error.isEmpty()) {
                    mathFormat.setUnderlineStyle(QTextCharFormat::DotLine);
                    mathFormat.setUnderlineColor(QColor("#b79057"));
                }
                appendText(source.mid(span.start, span.end - span.start), start, mathFormat);
            }
            at = resume;
        }
        appendProse(at, source.size());
    }
    m_document.setLayoutEnabled(true);
    m_document.documentLayout()->documentSize();
    if (anchorViewport) m_scrollY += documentPositionRect(stablePosition).y() - priorCaretY;
    m_scrollY = qBound(0.0, m_scrollY, qMax(0.0, contentHeight() - height()));
    m_rebuilding = false;
    if (!m_jobs.isEmpty() && m_pendingKey.isEmpty() && !m_renderTimer.isActive()) m_renderTimer.start();
    update(); emit layoutChanged();
}

void DocumentEditor::nextRender() {
    if (!m_backend || !m_pendingKey.isEmpty() || m_jobs.isEmpty()) return;
    const Job job = m_jobs.takeFirst(); m_pendingKey = job.key;
    m_backend->renderAsync(--m_requestId, job.body, m_color.name(), m_fontSize, m_renderPreamble, job.textMode);
}

void DocumentEditor::paint(QPainter *painter) {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->translate(leftMargin(), 32 - m_scrollY);
    QAbstractTextDocumentLayout::PaintContext context;
    context.palette.setColor(QPalette::Text, m_color);
    context.clip = QRectF(0, m_scrollY - 32, m_document.textWidth(), height());
    if (m_spellEnabled) {
        const int first = sourcePosition(qMax(0, m_document.documentLayout()->hitTest(QPointF(0, context.clip.top()), Qt::FuzzyHit)));
        int lastHit = m_document.documentLayout()->hitTest(QPointF(m_document.textWidth(), context.clip.bottom()), Qt::FuzzyHit);
        const int last = lastHit < 0 ? m_source.size() : sourcePosition(lastHit);
        for (const auto &value : m_spellingIssues) {
            const auto issue = value.toMap();
            const int start = issue.value("start").toInt(), end = issue.value("end").toInt();
            if (end < first || start > last) continue;
            QAbstractTextDocumentLayout::Selection underline;
            underline.cursor = QTextCursor(&m_document);
            underline.cursor.setPosition(visualPosition(start));
            underline.cursor.setPosition(visualPosition(end), QTextCursor::KeepAnchor);
            underline.format.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
            underline.format.setUnderlineColor(QColor("#ed6666"));
            context.selections.append(underline);
        }
    }
    if (m_cursor != m_anchor) {
        QAbstractTextDocumentLayout::Selection selection;
        selection.cursor = QTextCursor(&m_document);
        selection.cursor.setPosition(visualPosition(qMin(m_cursor, m_anchor)));
        selection.cursor.setPosition(visualPosition(qMax(m_cursor, m_anchor)), QTextCursor::KeepAnchor);
        selection.format.setBackground(m_selection);
        context.selections.append(selection);
    }
    m_document.documentLayout()->draw(painter, context);
    // Row metadata remains visible, without turning every paragraph into a card.
    for (int r = 0; r < m_rows.size(); ++r) {
        const auto row = m_rows[r].toMap(); const QString kind = row.value("kind").toString();
        auto block = m_document.findBlock(visualPosition(m_offsets[r]));
        auto rect = m_document.documentLayout()->blockBoundingRect(block);
        if (!rect.intersects(context.clip)) continue;
        painter->setPen(m_color.darker(150));
        if (kind == "bullet") painter->drawText(QPointF(3, rect.top() + m_fontSize), QString::fromUtf8("•"));
        else if (kind == "numbered") {
            int n = 1; for (int j = r - 1; j >= 0 && m_rows[j].toMap().value("kind") == "numbered"; --j) ++n;
            painter->drawText(QPointF(0, rect.top() + m_fontSize), QString::number(n) + ".");
        } else if (kind != "normal" && kind != "heading" && kind != "subheading" && kind != "image" && !kind.isEmpty()) {
            painter->drawLine(QPointF(3, rect.top()), QPointF(3, rect.bottom()));
            if (r == 0 || m_rows[r - 1].toMap().value("kind") != kind) {
                QFont labelFont(m_fontFamily); labelFont.setPixelSize(qMax(10, m_fontSize - 3));
                painter->setFont(labelFont);
                painter->drawText(QPointF(16, rect.top() - 6), row.value("label", kind).toString());
            }
        }
    }
    if (hasActiveFocus() && m_caretVisible && m_mode != 2) painter->fillRect(documentCursorRect(), m_color);
    if (!m_preedit.isEmpty()) {
        painter->setPen(m_color); painter->drawText(documentCursorRect().bottomLeft(), m_preedit);
    }
    painter->restore();
}

void DocumentEditor::moveCursor(int position, bool keepAnchor, bool keepX) {
    if (position != m_emptyDollarAt) m_emptyDollarAt = -1;
    const int priorRow = activeRow();
    const int priorCursor = m_cursor, priorAnchor = m_anchor;
    m_cursor = qBound(0, position, int(m_source.size()));
    if (!keepAnchor) m_anchor = m_cursor;
    if (!keepX) m_preferredX = -1;
    m_caretVisible = true;
    bool needsLayout = false;
    if (m_mode == 0) {
        for (const auto &range : m_readyRanges) {
            const auto expanded = [&](int cursor, int anchor) {
                return (cursor >= range.first && cursor <= range.second)
                    || (cursor != anchor && qMin(cursor, anchor) < range.second && qMax(cursor, anchor) > range.first);
            };
            if (expanded(priorCursor, priorAnchor) != expanded(m_cursor, m_anchor)) { needsLayout = true; break; }
        }
    }
    if (needsLayout) rebuild();
    else { update(); emit layoutChanged(); }
    ensureCursorVisible();
    if (activeRow() != priorRow) emit textChanged();
    emit cursorPositionChanged();
    QGuiApplication::inputMethod()->update(Qt::ImQueryAll);
}
bool DocumentEditor::inMath() const {
    if (m_preamble.rows.contains(activeRow())) return false;
    const int at = cursorPosition();
    for (const auto &span : LatexSyntax::previewSpans(text(), m_rows.value(activeRow()).toMap().value("mode", "auto").toString(), m_preamble.source))
        if (!span.textMode && at > span.start && (at < span.end || !span.complete)) return true;
    return false;
}
QVariantMap DocumentEditor::mathAtCursor() const {
    if (m_preamble.rows.contains(activeRow())) return {};
    for (const auto &span : LatexSyntax::previewSpans(text(), m_rows.value(activeRow()).toMap().value("mode", "auto").toString(), m_preamble.source))
        if (!span.textMode && cursorPosition() >= span.start && cursorPosition() <= span.end)
            return {{"start", span.start}, {"end", span.end}};
    return {};
}
QString DocumentEditor::completionPrefix() const {
    if (m_cursor != m_anchor || m_mode == 2) return {};
    return QRegularExpression(R"(\\([A-Za-z]{2,})$)").match(text().left(cursorPosition())).captured(1);
}
QString DocumentEditor::errorHint() const {
    if (!m_backend) return {};
    if (!m_preamble.complete) return {};
    for (const auto &span : LatexSyntax::previewSpans(text(), m_rows.value(activeRow()).toMap().value("mode", "auto").toString(), m_preamble.source)) {
        if (!span.complete) continue;
        const QString body = span.textMode ? span.body : span.display ? LatexSyntax::renderBody(span.body) : "\\textstyle " + LatexSyntax::renderBody(span.body);
        const auto render = m_renders.constFind(renderKey(body, m_color, m_fontSize, m_renderPreamble, span.textMode));
        if (render != m_renders.cend() && !render->error.isEmpty()) return m_backend->latexHint(span.body, render->error);
    }
    return {};
}

void DocumentEditor::keyPressEvent(QKeyEvent *event) {
    const bool ctrl = event->modifiers() & Qt::ControlModifier;
    const bool shift = event->modifiers() & Qt::ShiftModifier;
    const int key = event->key();
    if (event->matches(QKeySequence::Copy)) { copy(); return; }
    if (event->matches(QKeySequence::SelectAll)) { selectAll(); return; }
    if (event->matches(QKeySequence::Undo)) { emit undoRequested(); return; }
    if (event->matches(QKeySequence::Redo)) { emit redoRequested(); return; }
    if (m_mode == 2) { emit editModeRequested(); }
    if (event->matches(QKeySequence::Cut)) { cut(); return; }
    if (event->matches(QKeySequence::Paste)) { pasteText(QGuiApplication::clipboard()->text()); return; }
    if (m_completionVisible && (key == Qt::Key_Up || key == Qt::Key_Down)) { emit completionMove(key == Qt::Key_Up ? -1 : 1); return; }
    if (key == Qt::Key_Escape) { emit completionDismissed(); return; }
    if (key == Qt::Key_Tab || key == Qt::Key_Backtab) { emit tabPressed(shift || key == Qt::Key_Backtab); return; }
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        if (shift) emit blockBreakRequested();
        else if (ctrl) emit insertRowRequested();
        else replaceSource(qMin(m_cursor, m_anchor), qMax(m_cursor, m_anchor), "\n", !inMath(), true);
        return;
    }
    QTextCursor navigation(&m_document);
    navigation.setPosition(visualPosition(m_cursor));
    QTextCursor::MoveOperation operation = QTextCursor::NoMove;
    if (key == Qt::Key_Left) operation = ctrl ? QTextCursor::PreviousWord : QTextCursor::PreviousCharacter;
    if (key == Qt::Key_Right) operation = ctrl ? QTextCursor::NextWord : QTextCursor::NextCharacter;
    if (key == Qt::Key_Home) operation = ctrl ? QTextCursor::Start : QTextCursor::StartOfLine;
    if (key == Qt::Key_End) operation = ctrl ? QTextCursor::End : QTextCursor::EndOfLine;
    if (key == Qt::Key_Up || key == Qt::Key_Down || key == Qt::Key_PageUp || key == Qt::Key_PageDown) {
        const auto rect = documentCursorRect();
        if (m_preferredX < 0) m_preferredX = rect.x();
        const int direction = key == Qt::Key_Up || key == Qt::Key_PageUp ? -1 : 1;
        if (key == Qt::Key_PageUp || key == Qt::Key_PageDown) {
            const QPointF target(m_preferredX, rect.center().y() + direction * height() * 0.8);
            moveCursor(hitTest(target + QPointF(leftMargin(), 32 - m_scrollY)), shift, true);
            return;
        }
        // Walk actual visual lines: a tall fraction or paragraph gap must not
        // trap Up/Down on the same line or make it skip a formula.
        const int visual = visualPosition(m_cursor);
        QTextBlock targetBlock = m_document.findBlock(visual);
        QTextLine currentLine = targetBlock.layout()->lineForTextPosition(visual - targetBlock.position());
        int index = currentLine.lineNumber() + direction;
        if (index < 0 || index >= targetBlock.layout()->lineCount()) {
            targetBlock = direction < 0 ? targetBlock.previous() : targetBlock.next();
            if (!targetBlock.isValid()) {
                moveCursor(direction < 0 ? 0 : int(m_source.size()), shift, true); return;
            }
            index = direction < 0 ? targetBlock.layout()->lineCount() - 1 : 0;
        }
        const QTextLine line = targetBlock.layout()->lineAt(index);
        const QPointF origin = m_document.documentLayout()->blockBoundingRect(targetBlock).topLeft();
        const int hit = targetBlock.position() + line.xToCursor(m_preferredX - origin.x());
        int position = sourcePosition(hit);
        for (const auto &folded : m_folded) {
            if (folded.visual < targetBlock.position() + line.textStart()
                || folded.visual >= targetBlock.position() + line.textStart() + line.textLength()) continue;
            const QRectF bounds = foldedRectangle(folded);
            if (!folded.display && (m_preferredX < bounds.left() || m_preferredX > bounds.right())) continue;
            const QPointF target(qBound(bounds.left(), m_preferredX, bounds.right()),
                direction < 0 ? bounds.bottom() - 0.5 : bounds.top() + 0.5);
            position = hitTest(target + QPointF(leftMargin(), 32 - m_scrollY));
            break;
        }
        moveCursor(position, shift, true);
        return;
    }
    if (operation != QTextCursor::NoMove) {
        if (!shift && m_cursor != m_anchor && (key == Qt::Key_Left || key == Qt::Key_Right) && !ctrl)
            moveCursor(key == Qt::Key_Left ? qMin(m_cursor, m_anchor) : qMax(m_cursor, m_anchor));
        else { navigation.movePosition(operation); moveCursor(sourcePosition(navigation.position()), shift); }
        return;
    }
    if (key == Qt::Key_Backspace || key == Qt::Key_Delete) {
        int start = qMin(m_cursor, m_anchor), end = qMax(m_cursor, m_anchor);
        if (start == end && !ctrl && key == Qt::Key_Backspace) {
            const QList<QPair<QString, QString>> pairs{{"$$", "$$"}, {"$", "$"}, {"\\(", "\\)"},
                {"\\[", "\\]"}, {"{", "}"}, {"(", ")"}, {"[", "]"}};
            for (const auto &pair : pairs) {
                const int opening = start - pair.first.size();
                if (opening >= 0 && !escapedAt(m_source, opening)
                    && m_source.mid(opening, pair.first.size()) == pair.first
                    && m_source.mid(end, pair.second.size()) == pair.second) {
                    if (pair.first == "$" && m_emptyDollarAt != m_cursor) {
                        bool partOfDisplay = false;
                        const int offset = rowTextOffset(activeRow());
                        for (const auto &span : LatexSyntax::mathSpans(text(), "latex"))
                            if (span.display && span.end - span.start > 4 && offset + span.start <= opening
                                && offset + span.end >= end + 1) partOfDisplay = true;
                        if (partOfDisplay) continue;
                    }
                    replaceSource(opening, end + pair.second.size(), "", false, false); return;
                }
            }
        }
        if (start == end && !ctrl && shrinkDisplayDollars(key == Qt::Key_Backspace)) return;
        if (start == end) {
            // Move through source characters, so deleting beside a folded formula
            // never deletes an entire expression as an opaque image.
            QTextDocument raw(m_source); QTextCursor c(&raw); c.setPosition(m_cursor);
            c.movePosition(key == Qt::Key_Backspace ? (ctrl ? QTextCursor::PreviousWord : QTextCursor::PreviousCharacter)
                                                     : (ctrl ? QTextCursor::NextWord : QTextCursor::NextCharacter));
            start = qMin(m_cursor, c.position()); end = qMax(m_cursor, c.position());
        }
        replaceSource(start, end, "", false, false); return;
    }
    const bool altGr = event->modifiers() & Qt::GroupSwitchModifier;
    if (!event->text().isEmpty() && (altGr || !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)))) {
        const QString value = event->text();
        const bool escapedInput = escapedAt(m_source, m_cursor);
        const int first = qMin(m_cursor, m_anchor), last = qMax(m_cursor, m_anchor);
        if (!inComment(m_source, first) && value.size() == 1) {
            QString opening, closing;
            if (value == "$" && !escapedInput) {
                if (first == last && handleDollarInput()) return;
                opening = closing = "$";
            } else if (escapedInput && (value == "(" || value == "[")) {
                opening = value; closing = value == "(" ? "\\)" : "\\]";
            } else if (!escapedInput && QString("{([").contains(value)) {
                opening = value; closing = QString("})]")[QString("{([").indexOf(value)];
            } else if (value == "\\" && first == last && !escapedInput
                       && (m_source.mid(first, 2) == "\\)" || m_source.mid(first, 2) == "\\]")) {
                moveCursor(first + 1); return;
            }
            if (!opening.isEmpty()) {
                const QString selected = m_source.mid(first, last - first);
                replaceSource(first, last, opening + selected + closing, false, first != last);
                if (first != last) selectSource(first + opening.size(), first + opening.size() + selected.size());
                else moveCursor(first + opening.size());
                if (value == "$" && first == last) m_emptyDollarAt = m_cursor;
                return;
            }
        }
        // A closing delimiter that we already inserted is traversed, including
        // the second half of a typed LaTeX \\) or \\].
        if (first == last && escapedInput && value.size() == 1 && QString("})]").contains(value)
            && m_source.mid(first, 1) == value) { moveCursor(first + 1); return; }
        if (m_cursor == m_anchor && !escapedInput && value.size() == 1) {
            if (value == "}") {
                static const QRegularExpression opening(R"(\\begin\{([A-Za-z]+\*?)$)");
                const auto match = opening.match(text().left(cursorPosition()));
                const QString before = text().left(match.capturedStart());
                int slashes = 0;
                for (int i = before.size() - 1; i >= 0 && before[i] == '\\'; --i) ++slashes;
                const QString line = before.mid(before.lastIndexOf('\n') + 1);
                bool commented = false;
                for (int i = 0; i < line.size(); ++i) {
                    if (line[i] == '\\') ++i;
                    else if (line[i] == '%') { commented = true; break; }
                }
                if (match.hasMatch() && slashes % 2 == 0 && !commented) {
                    const QString name = match.captured(1);
                    const QString suffix = text().mid(cursorPosition());
                    static const QRegularExpression environment(R"(%[^\n]*|\\(begin|end)\{([^{}]*)\}|\\[A-Za-z]+|\\.)");
                    auto endings = environment.globalMatch(suffix);
                    int depth = 1;
                    while (endings.hasNext() && depth > 0) {
                        const auto ending = endings.next();
                        if (ending.captured(2) == name) depth += ending.captured(1) == "begin" ? 1 : -1;
                    }
                    if (depth > 0) {
                        const int start = m_cursor;
                        const bool hasBrace = m_cursor < m_source.size() && m_source[m_cursor] == '}';
                        replaceSource(start, start + (hasBrace ? 1 : 0), "}\n\n\\end{" + name + "}", false, false);
                        moveCursor(start + 2);
                        return;
                    }
                }
            }
            if (QString("})]").contains(value) && m_cursor < m_source.size() && m_source[m_cursor] == value[0]) {
                moveCursor(m_cursor + 1); return;
            }
        }
        replaceSource(qMin(m_cursor, m_anchor), qMax(m_cursor, m_anchor), event->text(), false, false); return;
    }
    event->ignore();
}

QRectF DocumentEditor::foldedRectangle(const Folded &folded) const {
    const auto block = m_document.findBlock(folded.visual);
    if (!block.isValid() || !block.layout()) return {};
    const auto line = block.layout()->lineForTextPosition(folded.visual - block.position());
    if (!line.isValid()) return {};
    const auto origin = m_document.documentLayout()->blockBoundingRect(block).topLeft();
    QFont font(m_fontFamily); font.setPixelSize(m_fontSize);
    return QRectF(origin.x() + line.cursorToX(folded.visual - block.position()),
        origin.y() + line.y() + line.ascent() - (folded.size.height() + QFontMetricsF(font).xHeight()) / 2,
        folded.size.width(), folded.size.height());
}
QRectF DocumentEditor::renderedSourceRectangle(int position) const {
    QRectF result;
    int shortest = INT_MAX;
    for (const auto &folded : m_folded) {
        const QRectF bounds = foldedRectangle(folded);
        for (const auto &value : m_renders.value(folded.key).hits) {
            const auto hit = value.toMap();
            const int start = folded.start + folded.positions.value(hit.value("start").toInt());
            const int end = folded.start + folded.positions.value(hit.value("end").toInt());
            if (start <= position && position < end && end - start < shortest) {
                shortest = end - start;
                result = QRectF(bounds.x() + hit.value("x").toDouble() * bounds.width(),
                    bounds.y() + hit.value("y").toDouble() * bounds.height(),
                    hit.value("width").toDouble() * bounds.width(), hit.value("height").toDouble() * bounds.height());
            }
        }
    }
    return result.isValid() ? result.translated(leftMargin(), 32 - m_scrollY) : QRectF();
}
int DocumentEditor::hitTest(QPointF point) const {
    point -= QPointF(leftMargin(), 32 - m_scrollY);
    for (const auto &folded : m_folded) {
        const QRectF bounds = foldedRectangle(folded);
        if (!bounds.adjusted(-2, -2, 2, 2).contains(point)) continue;
        qreal best = std::numeric_limits<qreal>::max();
        int position = -1;
        for (const auto &value : m_renders.value(folded.key).hits) {
            const auto hit = value.toMap();
            const QRectF glyph(bounds.x() + hit.value("x").toDouble() * bounds.width(),
                bounds.y() + hit.value("y").toDouble() * bounds.height(),
                hit.value("width").toDouble() * bounds.width(), hit.value("height").toDouble() * bounds.height());
            const qreal dx = std::max({glyph.left() - point.x(), 0.0, point.x() - glyph.right()});
            const qreal dy = std::max({glyph.top() - point.y(), 0.0, point.y() - glyph.bottom()});
            const qreal score = dx * dx + dy * dy + glyph.width() * glyph.height() * 0.00001;
            if (score < best) {
                best = score;
                const int index = hit.value(point.x() > glyph.center().x() ? "end" : "start").toInt();
                position = folded.start + folded.positions.value(index);
            }
        }
        if (position >= 0) return position;
        // Macro expansions without source specials still open near the clicked
        // horizontal position, rather than at one fixed midpoint.
        return folded.start + qBound(0, qRound((point.x() - bounds.x()) / bounds.width() * (folded.end - folded.start)), folded.end - folded.start);
    }
    const int hit = m_document.documentLayout()->hitTest(point, Qt::FuzzyHit);
    if (hit < 0) return point.y() < 0 ? 0 : m_source.size();
    const int position = qBound(0, hit, int(m_positions.size()) - 1);
    if (position + 1 < m_positions.size() && m_positions[position + 1] - m_positions[position] > 1)
        return (m_positions[position] + m_positions[position + 1]) / 2;
    return sourcePosition(hit);
}
void DocumentEditor::mousePressEvent(QMouseEvent *event) {
    if (m_mode == 2) emit editModeRequested();
    forceActiveFocus();
    const int position = hitTest(event->position());
    m_pressPoint = event->position();
    m_pressedWord = event->modifiers() == Qt::NoModifier && !spellingAt(position).isEmpty() ? position : -1;
    moveCursor(position, event->modifiers() & Qt::ShiftModifier);
    if (event->button() == Qt::RightButton && m_pressedWord >= 0) { requestSpelling(position, event->position().x(), event->position().y()); return; }
    if (event->button() == Qt::RightButton) { emit contextMenuRequested(activeRow(), event->position().x(), event->position().y()); return; }
    m_dragging = true; m_dragPoint = event->position(); m_dragTimer.start();
}
void DocumentEditor::mouseMoveEvent(QMouseEvent *event) { if (m_dragging) { m_dragPoint = event->position(); moveCursor(hitTest(m_dragPoint), true); } }
void DocumentEditor::mouseReleaseEvent(QMouseEvent *event) {
    m_dragging = false; m_dragTimer.stop();
    if (event->button() == Qt::LeftButton && m_pressedWord >= 0 && m_cursor == m_anchor
        && (event->position() - m_pressPoint).manhattanLength() < 5)
        requestSpelling(m_pressedWord, event->position().x(), event->position().y());
    m_pressedWord = -1;
}
void DocumentEditor::mouseDoubleClickEvent(QMouseEvent *event) {
    m_pressedWord = -1; ++m_suggestionRequest; emit spellingDismissed();
    moveCursor(hitTest(event->position()));
    QTextCursor cursor(&m_document); cursor.setPosition(visualPosition(m_cursor)); cursor.select(QTextCursor::WordUnderCursor);
    selectSource(sourcePosition(cursor.selectionStart()), sourcePosition(cursor.selectionEnd()));
}
void DocumentEditor::wheelEvent(QWheelEvent *event) { scrollBy(event->pixelDelta().isNull() ? -event->angleDelta().y() * 0.5 : -event->pixelDelta().y()); event->accept(); }
void DocumentEditor::inputMethodEvent(QInputMethodEvent *event) {
    if (!event->commitString().isEmpty() || event->replacementLength()) {
        const int start = event->replacementStart() ? m_cursor + event->replacementStart() : qMin(m_cursor, m_anchor);
        replaceSource(start, event->replacementLength() ? start + event->replacementLength() : qMax(m_cursor, m_anchor), event->commitString(), false, false);
    }
    m_preedit = event->preeditString(); update(); event->accept();
}
QVariant DocumentEditor::inputMethodQuery(Qt::InputMethodQuery query) const {
    switch (query) {
    case Qt::ImEnabled: return m_mode != 2;
    case Qt::ImCursorRectangle: return cursorRectangle();
    case Qt::ImCursorPosition: return m_cursor;
    case Qt::ImAnchorPosition: return m_anchor;
    case Qt::ImSurroundingText: return m_source;
    case Qt::ImCurrentSelection: return selectedText();
    default: return QQuickPaintedItem::inputMethodQuery(query);
    }
}
void DocumentEditor::focusInEvent(QFocusEvent *event) { QQuickPaintedItem::focusInEvent(event); m_caretVisible = true; m_blinkTimer.start(); update(); }
void DocumentEditor::focusOutEvent(QFocusEvent *event) { QQuickPaintedItem::focusOutEvent(event); m_blinkTimer.stop(); m_preedit.clear(); update(); }

void DocumentEditor::scheduleSpelling() {
    m_spellChecker.cancel();
    ++m_suggestionRequest;
    m_spellingTarget.clear(); m_spellingSuggestions.clear();
    emit spellingDismissed();
    if (m_spellEnabled) m_spellTimer.start(); else m_spellTimer.stop();
}
void DocumentEditor::setSpellLanguage(const QString &value) {
    const QString language = value == "en" ? "en" : "sv";
    if (m_spellLanguage == language) return;
    m_spellLanguage = language; m_spellingIssues.clear(); m_spellError.clear();
    scheduleSpelling(); update(); emit spellingChanged();
}
void DocumentEditor::setSpellcheckEnabled(bool value) {
    if (m_spellEnabled == value) return;
    m_spellEnabled = value; m_spellingIssues.clear(); m_spellError.clear();
    scheduleSpelling(); update(); emit spellingChanged();
}
void DocumentEditor::setSpellingIgnored(const QStringList &value) {
    if (m_spellIgnored == value) return;
    m_spellIgnored = value; m_spellingIssues.clear();
    scheduleSpelling(); update(); emit spellingChanged();
}
QVariantMap DocumentEditor::spellingAt(int position) const {
    if (!m_spellEnabled) return {};
    for (const auto &value : m_spellingIssues) {
        const auto issue = value.toMap();
        const int start = issue.value("start").toInt(), end = issue.value("end").toInt();
        if (position >= start && position < end && m_source.mid(start, end - start) == issue.value("word")) return issue;
    }
    return {};
}
QRectF DocumentEditor::spellingRectangle(int position) const {
    const auto issue = spellingAt(position);
    if (issue.isEmpty()) return {};
    const int start = visualPosition(issue.value("start").toInt()), end = visualPosition(issue.value("end").toInt());
    const auto block = m_document.findBlock(start);
    if (!block.isValid() || !block.layout()) return {};
    const auto line = block.layout()->lineForTextPosition(start - block.position());
    if (!line.isValid()) return {};
    const QPointF origin = m_document.documentLayout()->blockBoundingRect(block).topLeft();
    const qreal left = line.cursorToX(start - block.position());
    const qreal right = line.cursorToX(qMin(end - block.position(), line.textStart() + line.textLength()));
    return QRectF(origin.x() + left, origin.y() + line.y(), qMax(1.0, right - left), line.height()).translated(leftMargin(), 32 - m_scrollY);
}
void DocumentEditor::requestSpelling(int position, qreal x, qreal y) {
    m_spellingTarget = spellingAt(position);
    if (m_spellingTarget.isEmpty()) return;
    m_spellingPopupPoint = QPointF(x, y); m_spellingSuggestions.clear();
    const QString word = m_spellingTarget.value("word").toString();
    emit spellingMenuRequested(word, {}, true, x, y);
    m_spellChecker.suggest(++m_suggestionRequest, word, m_spellLanguage);
}
void DocumentEditor::correctSpelling(const QString &replacement) {
    if (m_spellingTarget.isEmpty() || !m_spellingSuggestions.contains(replacement)) return;
    const int start = m_spellingTarget.value("start").toInt(), end = m_spellingTarget.value("end").toInt();
    if (m_source.mid(start, end - start) != m_spellingTarget.value("word")) return;
    replaceSource(start, end, replacement, false, true);
    forceActiveFocus();
}
void DocumentEditor::ignoreSpelling() {
    if (m_spellingTarget.isEmpty()) return;
    emit spellingIgnoreRequested(m_spellingTarget.value("word").toString());
    forceActiveFocus();
}

namespace {
struct DollarPair { int start, end, width; bool closed; };
QVector<DollarPair> dollarPairs(const QString &source) {
    QVector<DollarPair> pairs;
    for (const auto &span : LatexSyntax::mathSpans(source, "latex")) {
        if (source[span.start] != '$') continue;
        const int width = span.display ? 2 : 1;
        const bool closed = span.end >= span.start + 2 * width
            && source.mid(span.end - width, width) == QString(width, '$')
            && !escapedAt(source, span.end - width);
        pairs.append({span.start, span.end, width, closed});
    }
    return pairs;
}
}

bool DocumentEditor::handleDollarInput() {
    const QString source = text();
    const int at = cursorPosition(), offset = rowTextOffset(activeRow());
    const auto pairs = dollarPairs(source);
    // The freshly inserted $|$ is lexically an unfinished display opening.
    for (const auto &pair : pairs) {
        if (!pair.closed && (m_emptyDollarAt == m_cursor || at + 1 == source.size())
            && pair.start == at - 1 && source.mid(at - 1, 2) == "$$"
            && (at < 2 || source[at - 2] != '$') && (at + 1 == source.size() || source[at + 1] != '$')) {
            replaceSource(offset + at - 1, offset + at + 1, "$$$$", false, false);
            moveCursor(offset + at + 1); return true;
        }
    }
    auto upgrade = [&](const DollarPair &pair, bool fromOpening) {
        const QString body = source.mid(pair.start + 1, pair.end - pair.start - 2);
        replaceSource(offset + pair.start, offset + pair.end, "$$" + body + "$$", false, true);
        moveCursor(offset + (fromOpening ? pair.start + 2 : pair.end + 2));
    };
    // Prefer the opening at the caret when two different formulas are adjacent.
    for (const auto &pair : pairs) {
        if ((!pair.closed && pair.width != 2) || at < pair.start || at > pair.start + pair.width) continue;
        if (pair.width == 1) upgrade(pair, true);
        else moveCursor(offset + pair.start + 2);
        return true;
    }
    for (const auto &pair : pairs) {
        if (!pair.closed) continue;
        if (at >= pair.end - pair.width && at < pair.end) {
            moveCursor(offset + at + 1); return true;
        }
        if (at == pair.end) {
            if (pair.width == 1) upgrade(pair, false);
            return true;
        }
    }
    // Inside math (including an unfinished opening), $ means one literal
    // delimiter. Never create another empty math pair inside the expression.
    for (const auto &span : LatexSyntax::mathSpans(source, "latex")) {
        if (at > span.start && (at < span.end || (at == span.end && !span.complete))) {
            replaceSource(m_cursor, m_cursor, "$", false, false); return true;
        }
    }
    for (int neighbor : {at - 1, at}) {
        if (neighbor >= 0 && neighbor < source.size() && source[neighbor] == '$' && !escapedAt(source, neighbor)) {
            replaceSource(m_cursor, m_cursor, "$", false, false); return true;
        }
    }
    return false;
}

bool DocumentEditor::shrinkDisplayDollars(bool backward) {
    const QString source = text();
    const int at = cursorPosition(), erased = at - (backward ? 1 : 0);
    if (erased < 0 || erased >= source.size() || source[erased] != '$' || inComment(source, erased)) return false;
    for (const auto &pair : dollarPairs(source)) {
        if (!pair.closed || pair.width != 2 || !((erased >= pair.start && erased < pair.start + 2)
            || (erased >= pair.end - 2 && erased < pair.end))) continue;
        const QString body = source.mid(pair.start + 2, pair.end - pair.start - 4);
        const int offset = rowTextOffset(activeRow());
        int caret = backward ? at - 1 : at;
        if (caret > pair.end - 1) --caret;
        if (caret > pair.start + 1) --caret;
        replaceSource(offset + pair.start, offset + pair.end, "$" + body + "$", false, true);
        moveCursor(offset + caret); return true;
    }
    return false;
}
