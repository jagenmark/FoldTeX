#pragma once

#include <QString>
#include <QVector>
#include <QSet>
#include <QVariantList>
#include <QMap>

namespace LatexSyntax {
struct Span {
    int start = 0;
    int end = 0;
    QString body;
    bool display = false;
    bool complete = true;
    bool textMode = false;
};
struct TextMarkup { int start, end, contentStart, contentEnd; QString command; };
struct Preamble { QString source, documentClass; QSet<int> rows; bool complete = true; };
QVector<TextMarkup> textMarkup(const QString &source);
// Source start -> resume after \\ and one optional source newline. Apply only
// within prose spans; mathematical row separators belong to the TeX renderer.
QMap<int, int> textLineBreaks(const QString &source);
Preamble documentPreamble(const QVariantList &rows);
QVariantList migrateRowModes(const QVariantList &rows);
QString instrumentMath(const QString &source);
QVector<Span> mathSpans(const QString &source, const QString &mode = QStringLiteral("auto"));
QVector<Span> previewSpans(const QString &source, const QString &mode, const QString &preamble);
QStringList splitBlocks(const QString &source);
QString renderBody(QString source, QVector<int> *positions = nullptr);
bool bareMath(const QString &source);
QString textToTex(const QString &source);
QString toTex(const QString &source, const QString &mode = QStringLiteral("auto"), const QString &preamble = QString());
}
