#include "latexsyntax.h"

#include <QRegularExpression>
#include <QCache>
#include <QStringList>
#include <algorithm>

namespace LatexSyntax {
namespace {
bool escaped(const QString &s, int at) {
    int count = 0;
    while (at > 0 && s.at(--at) == QLatin1Char('\\')) ++count;
    return count % 2;
}
bool commented(const QString &s, int at) {
    for (int i = s.lastIndexOf('\n', qMax(0, at - 1)) + 1; i < at; ++i)
        if (s[i] == '%' && !escaped(s, i)) return true;
    return false;
}
int closing(const QString &s, const QString &token, int from) {
    for (int i = from; i < s.size(); ++i) {
        if (s[i] == QLatin1Char('%') && !escaped(s, i)) {
            const int next = s.indexOf(QLatin1Char('\n'), i);
            if (next < 0) return -1;
            i = next;
        } else if (!escaped(s, i) && s.mid(i, token.size()) == token) return i;
    }
    return -1;
}
bool completeMath(const QString &source) {
    // An unfinished group/environment is ordinary input, not a TeX error.
    // Consume control sequences and comments before looking at delimiters so
    // escaped braces, dollars and commented-out endings do not affect balance.
    static const QRegularExpression tokens(R"(%[^\n]*|\\(?:begin|end)\{[^{}]*\}|\\[A-Za-z]+|\\.|\$\$?|[{}])");
    QStringList stack;
    auto matches = tokens.globalMatch(source);
    while (matches.hasNext()) {
        const QString token = matches.next().captured();
        if (token.startsWith('%')) continue;
        if (token == "{" || token == "\\(" || token == "\\[") stack.append(token);
        else if (token == "}" || token == "\\)" || token == "\\]") {
            const QString opening = token == "}" ? "{" : token == "\\)" ? "\\(" : "\\[";
            if (stack.isEmpty() || stack.takeLast() != opening) return false;
        } else if (token == "$" || token == "$$") {
            if (!stack.isEmpty() && stack.last() == token) stack.removeLast();
            else stack.append(token);
        } else if (token.startsWith("\\begin{")) stack.append(token.mid(7, token.size() - 8));
        else if (token.startsWith("\\end{")) {
            if (stack.isEmpty() || stack.takeLast() != token.mid(5, token.size() - 6)) return false;
        }
    }
    const QString trimmed = source.trimmed();
    if (!trimmed.isEmpty() && QString("\\_^").contains(trimmed.back()) && !escaped(trimmed, trimmed.size() - 1)) return false;
    return stack.isEmpty();
}
}

QMap<int, int> textLineBreaks(const QString &source) {
    QMap<int, int> result;
    for (int i = 0; i + 1 < source.size(); ++i) {
        if (source[i] != '\\' || source[i + 1] != '\\' || escaped(source, i) || commented(source, i)) continue;
        int next = i + 2;
        while (next < source.size() && (source[next] == ' ' || source[next] == '\t')) ++next;
        // A source newline following an explicit break is not a second break.
        const int resume = next < source.size() && source[next] == '\n' ? next + 1 : i + 2;
        result.insert(i, resume);
        ++i;
    }
    return result;
}

QVector<TextMarkup> textMarkup(const QString &source) {
    static thread_local QCache<QString, QVector<TextMarkup>> cache(4096);
    if (const auto *cached = cache.object(source)) return *cached;
    static const QRegularExpression command(R"(\\(textbf|textit|emph|texttt|textrm|textsf|textnormal|textsc|textup|textmd|underline)\s*\{)");
    QVector<TextMarkup> result;
    auto matches = command.globalMatch(source);
    while (matches.hasNext()) {
        const auto match = matches.next();
        if (escaped(source, match.capturedStart()) || commented(source, match.capturedStart())) continue;
        int depth = 1, end = match.capturedEnd();
        for (; end < source.size() && depth; ++end) {
            if (escaped(source, end)) continue;
            if (source[end] == '{') ++depth;
            else if (source[end] == '}') --depth;
        }
        if (!depth) result.append({int(match.capturedStart()), end, int(match.capturedEnd()), end - 1, match.captured(1)});
    }
    cache.insert(source, new QVector<TextMarkup>(result));
    return result;
}

Preamble documentPreamble(const QVariantList &rows) {
    Preamble result;
    static const QRegularExpression declaration(R"(^\s*\\(?:documentclass|usepackage|RequirePackage|newcommand|renewcommand|providecommand|DeclareMathOperator|newenvironment|renewenvironment|newtheorem|def|let|setlength|addtolength|PassOptionsToPackage)\b)");
    QString statement;
    int depth = 0, groups = 0;
    bool scanning = true;
    for (int r = 0; r < rows.size(); ++r) {
        const QString source = rows[r].toMap().value("source").toString();
        const QString trimmed = source.trimmed();
        if (trimmed == "\\begin{document}" || trimmed == "\\end{document}") {
            result.rows.insert(r); scanning = false; continue;
        }
        if (!scanning) continue;
        if (statement.isEmpty() && (trimmed.isEmpty() || trimmed.startsWith('%'))) continue;
        if (statement.isEmpty() && !declaration.match(source).hasMatch()) { scanning = false; continue; }
        result.rows.insert(r);
        statement += source + '\n';
        for (int i = 0; i < source.size(); ++i) {
            if (escaped(source, i)) continue;
            if (source[i] == '%') break;
            if (source[i] == '{') ++depth;
            if (source[i] == '}') { --depth; if (depth == 0) ++groups; }
        }
        if (depth == 0) {
            static const QRegularExpression head(R"(^\s*\\([A-Za-z]+)\*?\s*(\{?))");
            const auto declarationHead = head.match(statement);
            const QString command = declarationHead.captured(1);
            int required = 1;
            if (command.endsWith("command") || command == "DeclareMathOperator")
                required = declarationHead.captured(2).isEmpty() ? 1 : 2;
            else if (command.endsWith("environment")) required = 3;
            else if (command == "let") required = 0;
            if (groups < required) continue;
            if (statement.trimmed().startsWith("\\documentclass")) result.documentClass = statement;
            else result.source += statement;
            statement.clear(); groups = 0;
        }
    }
    result.complete = statement.isEmpty() && depth == 0;
    return result;
}

namespace {
// dvisvgm raw groups retain the TeX source range of each atom. Specials do
// not turn binary operators into ordinary atoms or change formula spacing.
// Arguments consumed by TeX are kept adjacent to their commands; unbraced
// script arguments are made explicit before inserting any specials.
class MathMapper {
public:
    explicit MathMapper(const QString &source) : s(source) {}
    QString run() { return sequence(QChar()); }
private:
    const QString &s;
    int at = 0;
    QString marked(const QString &body, int start, int end) {
        if (body.isEmpty()) return body;
        return "\\special{dvisvgm:raw <g id='ft-src-" + QString::number(start) + "-" + QString::number(end)
            + "'>}" + body + "\\special{dvisvgm:raw </g>}";
    }
    QString spaces() { QString result; while (at < s.size() && s[at].isSpace()) result += s[at++]; return result; }
    QString rawGroup(QChar open, QChar close) {
        const int start = at++;
        int depth = 1;
        while (at < s.size() && depth) {
            if (!escaped(s, at)) { if (s[at] == open) ++depth; else if (s[at] == close) --depth; }
            ++at;
        }
        return s.mid(start, at - start);
    }
    QString argument(bool map = true) {
        QString prefix = spaces();
        if (at >= s.size()) return prefix;
        if (s[at] == '{') {
            if (!map) return prefix + rawGroup('{', '}');
            ++at; return prefix + "{" + sequence('}') + "}";
        }
        // A single token argument must not consume an instrumentation special.
        const int start = at;
        if (s[at] == '\\') { ++at; while (at < s.size() && s[at].isLetter()) ++at; if (at == start + 1 && at < s.size()) ++at; }
        else ++at;
        return prefix + "{" + marked(s.mid(start, at - start), start, at) + "}";
    }
    QString sequence(QChar close) {
        QString result;
        while (at < s.size()) {
            const int start = at;
            QChar c = s[at++];
            if (c == close) break;
            if (c == '%') { while (at < s.size() && s[at] != '\n') ++at; result += s.mid(start, at - start); continue; }
            if (c == '{') { result += "{" + sequence('}') + "}"; continue; }
            if (c == '^' || c == '_') { result += c; result += argument(); continue; }
            if (c.isSpace() || c == '&' || c == '}' || c == '\'') { result += c; continue; }
            if (c != '\\') { result += marked(QString(c), start, at); continue; }
            while (at < s.size() && s[at].isLetter()) ++at;
            if (at == start + 1 && at < s.size()) ++at;
            const QString command = s.mid(start, at - start);
            QString body = command;
            if (command == "\\begin" || command == "\\end") {
                body += spaces();
                QString group;
                if (at < s.size() && s[at] == '{') group = rawGroup('{', '}');
                body += group;
                if (command == "\\begin" && (group == "{array}" || group == "{alignedat}")) {
                    body += spaces();
                    if (at < s.size() && s[at] == '{') body += rawGroup('{', '}');
                }
                result += body; continue;
            }
            if (command == "\\left" || command == "\\right" || command == "\\middle"
                || command == "\\big" || command == "\\Big" || command == "\\bigg" || command == "\\Bigg") {
                body += spaces();
                if (at < s.size()) {
                    int delimiter = at++;
                    if (s[delimiter] == '\\') { while (at < s.size() && s[at].isLetter()) ++at; if (at == delimiter + 1 && at < s.size()) ++at; }
                    body += s.mid(delimiter, at - delimiter);
                }
                result += body; continue;
            }
            static const QSet<QString> twoArgs{"\\frac", "\\dfrac", "\\tfrac", "\\binom", "\\dbinom", "\\tbinom", "\\overset", "\\underset"};
            static const QSet<QString> oneArg{"\\sqrt", "\\overline", "\\underline", "\\hat", "\\widehat", "\\tilde", "\\widetilde", "\\vec", "\\bar", "\\dot", "\\ddot", "\\overbrace", "\\underbrace"};
            if (twoArgs.contains(command)) { body += argument(); body += argument(); result += marked(body, start, at); continue; }
            if (oneArg.contains(command)) {
                body += spaces();
                if (command == "\\sqrt" && at < s.size() && s[at] == '[') body += rawGroup('[', ']');
                body += argument(); result += marked(body, start, at); continue;
            }
            static const QSet<QString> structural{"\\displaystyle", "\\textstyle", "\\scriptstyle", "\\scriptscriptstyle", "\\limits", "\\nolimits", "\\hline", "\\\\", "\\,", "\\;", "\\:", "\\!", "\\quad", "\\qquad"};
            if (structural.contains(command)) { result += command; continue; }
            // Keep unknown macro calls and their explicit arguments together.
            // Font/text arguments remain intact, preserving ligatures and fonts.
            body += spaces();
            if (at < s.size() && s[at] == '*') { body += s[at++]; body += spaces(); }
            if (at < s.size() && s[at] == '[') { body += rawGroup('[', ']'); body += spaces(); }
            while (at < s.size() && s[at] == '{') { body += rawGroup('{', '}'); body += spaces(); }
            result += marked(body, start, at);
        }
        return result;
    }
};
}
QString instrumentMath(const QString &source) { return MathMapper(source).run(); }

QString renderBody(QString source, QVector<int> *positions) {
    using MappedBody = QPair<QString, QVector<int>>;
    static thread_local QCache<QString, MappedBody> cache(4096);
    const QString original = source;
    if (const auto *cached = cache.object(original)) {
        if (positions) *positions = cached->second;
        return cached->first;
    }
    QVector<int> map;
    for (int i = 0; i <= source.size(); ++i) map.append(i);
    auto slice = [&](int from, int length) { source = source.mid(from, length); map = map.mid(from, length + 1); };
    auto trim = [&] {
        int from = 0, end = source.size();
        while (from < end && source[from].isSpace()) ++from;
        while (end > from && source[end - 1].isSpace()) --end;
        slice(from, end - from);
    };
    trim();
    const QList<QPair<QString, QString>> delimiters{{"\\[", "\\]"}, {"\\(", "\\)"}, {"$$", "$$"}, {"$", "$"}};
    for (const auto &pair : delimiters) {
        if (source.startsWith(pair.first) && source.endsWith(pair.second)
                && source.size() >= pair.first.size() + pair.second.size()) {
            slice(pair.first.size(), source.size() - pair.first.size() - pair.second.size()); trim(); break;
        }
    }
    static const QRegularExpression environment(R"(^\\begin\{(align\*?|gather\*?|equation\*?)\}([\s\S]*)\\end\{\1\}$)");
    const auto match = environment.match(source);
    if (match.hasMatch()) {
        const QString name = match.captured(1);
        const int originalStart = map.first(), originalEnd = map.last();
        slice(match.capturedStart(2), match.capturedLength(2));
        QString nested;
        if (name.startsWith("align")) nested = "aligned";
        else if (name.startsWith("gather")) nested = "gathered";
        if (!nested.isEmpty()) {
            const QString prefix = "\\begin{" + nested + "}", suffix = "\\end{" + nested + "}";
            source = prefix + source + suffix;
            QVector<int> next(prefix.size(), originalStart); next += map;
            for (int i = 0; i < suffix.size(); ++i) next.append(originalEnd);
            map = next;
        }
    }
    if (positions) *positions = map;
    cache.insert(original, new MappedBody(source, map));
    return source;
}

bool bareMath(const QString &source) {
    // Retain the old fast formula workflow, without interpreting prose or
    // structural/text LaTeX commands as one large equation.
    if (QRegularExpression(R"(^\s*\\text\s*\{)").match(source).hasMatch()) return true;
    if (!textMarkup(source).isEmpty()) return false;
    if (QRegularExpression(R"(\\(?:section|subsection|subsubsection|textbf|textit|emph|label|ref|cite|includegraphics)\b)").match(source).hasMatch()) return false;
    if (QRegularExpression(R"(\b[A-Za-zÅÄÖåäö]{3,}\s+[A-Za-zÅÄÖåäö]{2,}\b)").match(source).hasMatch()
            && !source.contains("\\begin{")) return false;
    return QRegularExpression(R"(\\[A-Za-z]+|[_^=])").match(source).hasMatch();
}

static QVector<Span> parseMathSpans(const QString &source, const QString &mode) {
    QVector<Span> result;
    if (mode == "text") return result;
    static const QRegularExpression legacyEnvironment(R"(^\s*\\begin\{(?:aligned|alignedat|gathered|cases|split|array|matrix|[pbBvV]matrix)\})");
    if (!source.trimmed().isEmpty() && (mode == "math"
            || (mode == "auto" && !source.contains('$') && !source.contains("\\(")
                && !source.contains("\\[") && (!source.contains("\\begin{") || legacyEnvironment.match(source).hasMatch())
                && bareMath(source))))
        return {{0, int(source.size()), renderBody(source), true, completeMath(source)}};
    static const QRegularExpression begin(R"(^\\begin\{(align\*?|aligned|alignedat|gather\*?|gathered|equation\*?|cases|split|array|matrix|[pbBvV]matrix)\})");
    for (int i = 0; i < source.size(); ++i) {
        if (escaped(source, i)) continue;
        if (source[i] == QLatin1Char('%')) {
            int next = source.indexOf(QLatin1Char('\n'), i);
            if (next < 0) break;
            i = next;
            continue;
        }
        if (source[i] != QLatin1Char('$') && source[i] != QLatin1Char('\\')) continue;
        QString open, close;
        bool display = false;
        if (source.mid(i, 2) == "\\[") { open = "\\["; close = "\\]"; display = true; }
        else if (source.mid(i, 2) == "\\(") { open = "\\("; close = "\\)"; }
        else if (source.mid(i, 2) == "$$") { open = close = "$$"; display = true; }
        else if (source[i] == QLatin1Char('$')) { open = close = "$"; }
        else {
            const auto match = begin.match(source.mid(i));
            if (match.hasMatch()) {
                open = match.captured(); close = "\\end{" + match.captured(1) + "}"; display = true;
                // Match nesting of the same environment rather than the first end.
                int depth = 1, at = i + open.size(), end = -1;
                while (depth && at < source.size()) {
                    int nextOpen = closing(source, open, at);
                    int nextClose = closing(source, close, at);
                    if (nextClose < 0) break;
                    if (nextOpen >= 0 && nextOpen < nextClose) { ++depth; at = nextOpen + open.size(); }
                    else { --depth; at = nextClose + close.size(); if (!depth) end = at; }
                }
                result.append({i, end < 0 ? int(source.size()) : end,
                               renderBody(source.mid(i, (end < 0 ? source.size() : end) - i)), true,
                               end >= 0 && completeMath(source.mid(i, end - i))});
                i = result.last().end - 1;
                continue;
            }
        }
        if (open.isEmpty()) continue;
        int end = closing(source, close, i + open.size());
        result.append({i, end < 0 ? int(source.size()) : end + int(close.size()),
                       end < 0 ? source.mid(i + open.size()) : source.mid(i + open.size(), end - i - open.size()), display,
                       end >= 0 && completeMath(source.mid(i + open.size(), end - i - open.size()))});
        i = result.last().end - 1;
    }
    if (result.isEmpty() && !source.trimmed().isEmpty() && (mode == "math" || (mode == "auto" && bareMath(source))))
        result.append({0, int(source.size()), renderBody(source), true, completeMath(source)});
    return result;
}

QVector<Span> mathSpans(const QString &source, const QString &mode) {
    static thread_local QCache<QString, QVector<Span>> cache(4096);
    const QString key = mode + QChar(0) + source;
    if (const auto *cached = cache.object(key)) return *cached;
    auto result = parseMathSpans(source, mode);
    cache.insert(key, new QVector<Span>(result));
    return result;
}

QVariantList migrateRowModes(const QVariantList &rows) {
    // Preserve older bare equations by expressing their meaning in the source.
    // Explicit delimiters always take precedence over an old forced row mode.
    const auto setup = documentPreamble(rows);
    QVariantList result;
    for (int i = 0; i < rows.size(); ++i) {
        auto row = rows[i].toMap();
        const QString mode = row.value("mode", "auto").toString();
        if (mode == "latex") { result.append(row); continue; }
        const QString source = row.value("source").toString();
        bool explicitMath = false;
        for (const auto &span : mathSpans(source, "latex")) {
            const QString opening = source.mid(span.start, 2);
            if (opening.startsWith('$') || opening == "\\(" || opening == "\\[") explicitMath = true;
        }
        static const QRegularExpression displayEnvironment(R"(\\begin\{(?:align|gather|equation)\*?\})");
        if (mode != "latex" && mode != "text" && row.value("kind") != "image"
            && !setup.rows.contains(i) && !source.trimmed().isEmpty()
            && !explicitMath && !displayEnvironment.match(source).hasMatch()) {
            const auto spans = previewSpans(source, mode == "math" ? "math" : "auto", setup.source);
            if (spans.size() == 1 && !spans.first().textMode && spans.first().start == 0
                && spans.first().end == source.size())
                row.insert("source", QString("\\[\n") + source + "\n\\]");
        }
        row.insert("mode", "latex");
        result.append(row);
    }
    return result;
}

QVector<Span> previewSpans(const QString &source, const QString &mode, const QString &preamble) {
    auto result = mathSpans(source, mode);
    if (mode == "text" || mode == "math" || !source.contains('\\')) return result;
    if (preamble.isEmpty() && result.size() == 1 && result.first().start == 0 && result.first().end == source.size()) return result;
    static thread_local QCache<QString, QVector<Span>> cache(4096);
    const QString cacheKey = mode + QChar(0) + preamble + QChar(0) + source;
    if (const auto *cached = cache.object(cacheKey)) return *cached;
    static const QRegularExpression definitions(R"(\\(?:newcommand|renewcommand|providecommand|DeclareMathOperator)\*?\s*\{?\\([A-Za-z]+))");
    QSet<QString> names;
    auto definitionsFound = definitions.globalMatch(preamble);
    while (definitionsFound.hasNext()) names.insert(definitionsFound.next().captured(1));
    static const QRegularExpression invocation(R"(\\([A-Za-z]+))");
    auto commands = invocation.globalMatch(source);
    bool custom = false;
    while (commands.hasNext()) if (names.contains(commands.next().captured(1))) custom = true;
    // Without math delimiters a user macro follows normal LaTeX text rules.
    // Explicit Math mode continues to support older bare-formula notes.
    if (custom && mode == "auto" && !source.contains('$') && !source.contains("\\(") && !source.contains("\\["))
        result = mathSpans(source, "latex");
    const auto markup = textMarkup(source);
    commands = invocation.globalMatch(source);
    int coveredUntil = 0;
    while (commands.hasNext()) {
        const auto command = commands.next();
        const int start = command.capturedStart();
        if (start < coveredUntil || escaped(source, start) || commented(source, start)) continue;
        bool covered = false;
        for (const auto &span : result) if (span.start <= start && start < span.end) { covered = true; break; }
        for (const auto &mark : markup) if (mark.start == start) covered = true;
        if (covered || command.captured(1) == "section" || command.captured(1) == "subsection" || command.captured(1) == "subsubsection") continue;
        int at = command.capturedEnd();
        if (at < source.size() && source[at] == '*') ++at;
        bool complete = true;
        while (at < source.size()) {
            int next = at;
            while (next < source.size() && source[next].isSpace()) ++next;
            if (next >= source.size() || (source[next] != '{' && source[next] != '[')) break;
            const QChar open = source[next], close = open == '{' ? '}' : ']';
            int depth = 1; at = next + 1;
            while (at < source.size() && depth) {
                if (!escaped(source, at)) { if (source[at] == open) ++depth; else if (source[at] == close) --depth; }
                ++at;
            }
            if (depth) { complete = false; break; }
        }
        result.append({start, at, source.mid(start, at - start), false, complete, true}); coveredUntil = at;
    }
    std::sort(result.begin(), result.end(), [](const Span &a, const Span &b) { return a.start < b.start; });
    cache.insert(cacheKey, new QVector<Span>(result));
    return result;
}

QStringList splitBlocks(const QString &source) {
    const auto spans = mathSpans(source, "latex");
    QStringList blocks;
    int start = 0, span = 0;
    for (int i = 0; i < source.size(); ++i) {
        while (span < spans.size() && spans[span].end <= i) ++span;
        if (source[i] == QLatin1Char('\n') && !(span < spans.size() && spans[span].start <= i && i < spans[span].end)) {
            blocks.append(source.mid(start, i - start)); start = i + 1;
        }
    }
    blocks.append(source.mid(start));
    return blocks;
}

QString textToTex(const QString &source) {
    QString result;
    for (QChar c : source) {
        switch (c.unicode()) {
        case '\\': result += "\\textbackslash{}"; break;
        case '{': case '}': case '#': case '$': case '%': case '&': case '_': result += '\\'; result += c; break;
        case '^': result += "\\textasciicircum{}"; break;
        case '~': result += "\\textasciitilde{}"; break;
        case '\n': result += "\\strut\\\\\n"; break;
        default: result += c;
        }
    }
    return result;
}

QString toTex(const QString &source, const QString &mode, const QString &preamble) {
    if (mode == "text") return textToTex(source);
    if (QRegularExpression(R"(^\s*\\(?:section|subsection|subsubsection)\{[^\n]*\}\s*$)").match(source).hasMatch()) return source;
    const auto markup = textMarkup(source);
    const auto breaks = textLineBreaks(source);
    auto prose = [&](int start, int end, bool afterDisplay = false, bool beforeDisplay = false) {
        QString text;
        for (int i = start; i < end; ++i) {
            // Display math supplies its own line boundaries. A physical
            // newline next to it must not become an extra forced text line.
            if (source[i] == '\n' && ((afterDisplay && i == start) || (beforeDisplay && i == end - 1))) continue;
            const auto lineBreak = breaks.constFind(i);
            if (lineBreak != breaks.cend() && lineBreak.value() <= end) {
                if ((lineBreak.value() == end && beforeDisplay) || (afterDisplay && i == start)) {
                    // The adjoining display already supplies this boundary.
                } else if (lineBreak.value() == source.size()) {
                    // A forced break immediately before \\par creates an
                    // empty justified line (Underfull \\hbox). Keep the
                    // requested space explicitly at the paragraph boundary.
                    text += "\\par\\vspace{\\baselineskip}";
                } else text += source.mid(i, lineBreak.value() - i);
                i = lineBreak.value() - 1;
                continue;
            }
            bool syntax = false;
            for (const auto &mark : markup)
                if ((i >= mark.start && i < mark.contentStart) || (i >= mark.contentEnd && i < mark.end)) { syntax = true; break; }
            text += syntax ? QString(source[i]) : textToTex(QString(source[i]));
        }
        return text;
    };
    QString result;
    int at = 0;
    bool afterDisplay = false;
    for (const auto &span : previewSpans(source, mode, preamble)) {
        result += prose(at, span.start, afterDisplay, span.display && span.complete);
        result += span.textMode ? source.mid(span.start, span.end - span.start)
            : span.complete ? (span.display ? "\\[\n" : "\\(") + renderBody(span.body) + (span.display ? "\n\\]" : "\\)")
                                : textToTex(source.mid(span.start, span.end - span.start));
        at = span.end;
        afterDisplay = span.display && span.complete;
    }
    result += prose(at, source.size(), afterDisplay);
    return result;
}
}
