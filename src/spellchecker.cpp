#include "spellchecker.h"
#include "latexsyntax.h"
#include <hunspell.hxx>
#include <QCache>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringConverter>
#include <QSet>
#include <map>

struct SpellChecker::Dictionaries {
    struct Entry {
        std::unique_ptr<Hunspell> engine;
        QStringConverter::Encoding encoding = QStringConverter::Utf8;
        QCache<QString, bool> known{16000};
        std::string encode(QString word) const {
            word.replace(QChar(0x2019), QChar('\''));
            const QByteArray bytes = QStringEncoder(encoding)(word.normalized(QString::NormalizationForm_C));
            return bytes.toStdString();
        }
    };
    std::map<QString, Entry> entries;
    Entry *get(const QString &language) {
        auto &entry = entries[language];
        if (entry.engine) return &entry;
        const QString name = language == "en" ? "en_US" : "sv_SE";
        const QString directory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/dictionaries-32b006a2";
        if (!QDir().mkpath(directory)) return nullptr;
        for (const QString &extension : {QString("aff"), QString("dic")}) {
            const QString filename = name + '.' + extension;
            if (QFile::exists(directory + '/' + filename)) continue;
            QFile input(":/spelling/" + filename);
            QSaveFile output(directory + '/' + filename);
            if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) return nullptr;
            if (output.write(input.readAll()) < 0 || !output.commit()) return nullptr;
        }
        const QByteArray aff = QFile::encodeName(directory + '/' + name + ".aff");
        const QByteArray dic = QFile::encodeName(directory + '/' + name + ".dic");
        entry.engine = std::make_unique<Hunspell>(aff.constData(), dic.constData());
        const auto encoding = QStringConverter::encodingForName(entry.engine->get_dic_encoding());
        if (!encoding) { entry.engine.reset(); return nullptr; }
        entry.encoding = *encoding;
        return &entry;
    }
};

SpellChecker::SpellChecker(QObject *parent) : QObject(parent), m_dictionaries(std::make_unique<Dictionaries>()), m_worker(new QObject) {
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread.start();
}
SpellChecker::~SpellChecker() { cancel(); m_thread.quit(); m_thread.wait(); }
void SpellChecker::cancel() { ++m_generation; }

QVariantList SpellChecker::words(const QVariantList &rows) {
    const auto preamble = LatexSyntax::documentPreamble(rows);
    static const QRegularExpression wordPattern(R"(\p{L}[\p{L}\p{M}]*(?:['’][\p{L}\p{M}]+)*)");
    static const QRegularExpression commandPattern(R"(\\(?:[A-Za-z]+\*?|.))");
    static const QRegularExpression technical(R"((?:https?://|www\.)\S+|[\w.+-]+@[\w.-]+\.[A-Za-z]+)");
    const QSet<QString> textCommands{"\\textbf", "\\textit", "\\emph", "\\texttt", "\\textrm", "\\textsf", "\\textnormal", "\\textsc", "\\textup", "\\textmd", "\\underline", "\\section", "\\subsection", "\\subsubsection", "\\paragraph", "\\caption"};
    QVariantList result;
    int offset = 0;
    for (int row = 0; row < rows.size(); ++row) {
        const auto data = rows[row].toMap();
        const QString source = data.value("source").toString();
        const bool image = data.value("kind") == "image";
        const int start = offset + (image ? 1 : 0);
        offset = start + source.size() + 1;
        if (preamble.rows.contains(row)) continue;
        QString prose = source;
        auto mask = [&](int from, int to) { for (int i = from; i < to && i < prose.size(); ++i) prose[i] = ' '; };
        const QString mode = data.value("mode", "auto").toString();
        if (mode != "text" && !image) {
            for (const auto &span : LatexSyntax::mathSpans(source, mode)) mask(span.start, span.end);
            auto commands = commandPattern.globalMatch(source);
            int covered = 0;
            while (commands.hasNext()) {
                const auto command = commands.next();
                if (command.capturedStart() < covered) continue;
                QString name = command.captured(); if (name.endsWith('*')) name.chop(1);
                int end = command.capturedEnd();
                if (!textCommands.contains(name)) {
                    while (end < source.size()) {
                        int next = end;
                        while (next < source.size() && source[next].isSpace()) ++next;
                        if (next >= source.size() || (source[next] != '{' && source[next] != '[')) break;
                        const QChar open = source[next], close = open == '{' ? '}' : ']';
                        int depth = 1; end = next + 1;
                        while (end < source.size() && depth) {
                            if (source[end] == '\\') { end += qMin(2, int(source.size()) - end); continue; }
                            if (source[end] == open) ++depth;
                            if (source[end] == close) --depth;
                            ++end;
                        }
                    }
                }
                mask(command.capturedStart(), end); covered = end;
            }
            for (int i = 0; i < source.size(); ++i) {
                if (source[i] == '\\') { ++i; continue; }
                if (source[i] == '%') { int end = source.indexOf('\n', i); if (end < 0) end = source.size(); mask(i, end); i = end; }
            }
        }
        auto links = technical.globalMatch(prose);
        while (links.hasNext()) { const auto link = links.next(); mask(link.capturedStart(), link.capturedEnd()); }
        auto words = wordPattern.globalMatch(prose);
        while (words.hasNext()) {
            const auto word = words.next();
            if (word.capturedLength() < 2) continue;
            if ((word.capturedStart() > 0 && source[word.capturedStart()-1].isDigit())
                || (word.capturedEnd() < source.size() && source[word.capturedEnd()].isDigit())) continue;
            result.append(QVariantMap{{"start", start + word.capturedStart()}, {"end", start + word.capturedEnd()}, {"word", word.captured()}});
        }
    }
    return result;
}
void SpellChecker::check(const QVariantList &rows, const QString &language, const QStringList &ignored) {
    const int generation = ++m_generation;
    QMetaObject::invokeMethod(m_worker, [this, rows, language, ignored, generation] {
        if (generation != m_generation) return;
        auto *dictionary = m_dictionaries->get(language);
        QVariantList issues;
        if (dictionary) {
            QSet<QString> accepted;
            for (const auto &word : ignored) accepted.insert(word.toCaseFolded());
            for (const auto &candidate : words(rows)) {
                if (generation != m_generation) return;
                const QString word = candidate.toMap().value("word").toString();
                if (accepted.contains(word.toCaseFolded())) continue;
                const bool *cached = dictionary->known.object(word);
                const bool correct = cached ? *cached : dictionary->engine->spell(dictionary->encode(word));
                if (!cached) dictionary->known.insert(word, new bool(correct));
                if (!correct) issues.append(candidate);
            }
        }
        QMetaObject::invokeMethod(this, [this, issues, generation, available = dictionary != nullptr] {
            if (generation == m_generation) emit checked(issues, available ? QString() : QString("Ordlistan kunde inte öppnas."));
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
void SpellChecker::suggest(int request, const QString &word, const QString &language) {
    QMetaObject::invokeMethod(m_worker, [this, request, word, language] {
        QStringList suggestions;
        if (auto *dictionary = m_dictionaries->get(language)) {
            for (const auto &suggestion : dictionary->engine->suggest(dictionary->encode(word))) {
                suggestions.append(QStringDecoder(dictionary->encoding)(QByteArray::fromStdString(suggestion)));
                if (suggestions.size() == 6) break;
            }
        }
        QMetaObject::invokeMethod(this, [this, request, suggestions] { emit suggested(request, suggestions); }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
