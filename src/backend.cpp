#include "backend.h"
#include "documenteditor.h"
#include "latexsyntax.h"
#include <qqml.h>

#include <QCryptographicHash>
#include <QClipboard>
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QGuiApplication>
#include <QImage>
#include <QProcess>
#include <QPainter>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSvgRenderer>
#include <QXmlStreamReader>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QTemporaryDir>
#include <QThread>
#include <QtMath>
#include <QUrl>
#include <QUuid>

#include <algorithm>
#include <QSet>

namespace {
QString unquote(QString value) {
    value = value.trimmed();
    if (value.size() >= 2
        && ((value.front() == QLatin1Char('"') && value.back() == QLatin1Char('"'))
            || (value.front() == QLatin1Char('\'') && value.back() == QLatin1Char('\''))))
        return value.mid(1, value.size() - 2);
    return value;
}

QString bareMath(QString source) {
    return LatexSyntax::renderBody(source);
}

QString escapedText(const QString &text) {
    QString escaped;
    for (const QChar character : text) {
        switch (character.unicode()) {
        case '\\': escaped += QStringLiteral("\\textbackslash{}"); break;
        case '{': escaped += QStringLiteral("\\{"); break;
        case '}': escaped += QStringLiteral("\\}"); break;
        case '#': escaped += QStringLiteral("\\#"); break;
        case '$': escaped += QStringLiteral("\\$"); break;
        case '%': escaped += QStringLiteral("\\%"); break;
        case '&': escaped += QStringLiteral("\\&"); break;
        case '_': escaped += QStringLiteral("\\_"); break;
        case '^': escaped += QStringLiteral("\\textasciicircum{}"); break;
        case '~': escaped += QStringLiteral("\\textasciitilde{}"); break;
        case '\n': escaped += QStringLiteral("\\strut\\\\\n"); break;
        default: escaped += character;
        }
    }
    return escaped;
}

QString inlineLineTex(const QVariantMap &line, const QString &preamble) {
    const QString source = line.value(QStringLiteral("source")).toString();
    return LatexSyntax::toTex(source, line.value("mode", "auto").toString(), preamble);
}

QString blockLabel(const QString &kind) {
    if (kind == QStringLiteral("definition")) return QStringLiteral("Definition");
    if (kind == QStringLiteral("theorem")) return QStringLiteral("Theorem");
    if (kind == QStringLiteral("proof")) return QStringLiteral("Proof");
    if (kind == QStringLiteral("example")) return QStringLiteral("Example");
    if (kind == QStringLiteral("remark")) return QStringLiteral("Remark");
    if (kind == QStringLiteral("exercise")) return QStringLiteral("Exercise");
    if (kind == QStringLiteral("solution")) return QStringLiteral("Solution");
    if (kind == QStringLiteral("catchup")) return QStringLiteral("Catch up");
    return QString();
}

QString documentTex(const QVariantMap &document) {
    const QString title = document.value(QStringLiteral("title"),
                                         QStringLiteral("Untitled notes")).toString();
    const QString course = document.value(QStringLiteral("course")).toString();
    const QString date = document.value(QStringLiteral("lectureDate")).toString();
    const QString noteKind = document.value(QStringLiteral("noteKind"),
                                            QStringLiteral("lecture")).toString();
    const QString detail = noteKind == QStringLiteral("problem-solving")
        ? document.value(QStringLiteral("problemSet")).toString()
        : document.value(QStringLiteral("lecture")).toString();
    const QVariantList lines = document.value(QStringLiteral("lines")).toList();
    const auto setup = LatexSyntax::documentPreamble(lines);
    QString text = QStringLiteral(
        "\\documentclass[11pt]{article}\n"
        "\\usepackage[margin=1in]{geometry}\n"
        "\\usepackage{amsmath,amssymb,mathtools}\n"
        "\\usepackage{graphicx}\n"
        "\\usepackage[T1]{fontenc}\n"
        "\\usepackage[utf8]{inputenc}\n"
        "\\title{%1}\n"
        "\\author{%2}\n"
        "\\date{%3}\n"
        "\\begin{document}\n"
        "\\maketitle\n").arg(escapedText(title), escapedText(course), escapedText(date));
    if (!setup.documentClass.isEmpty()) {
        const int newline = text.indexOf('\n');
        text.replace(0, newline + 1, setup.documentClass);
    }
    text.insert(text.indexOf("\\title{"), setup.source + "\n");
    if (!detail.isEmpty())
        text += QStringLiteral("\\begin{center}\\textit{%1}\\end{center}\n")
                    .arg(escapedText(detail));

    QString openList;
    QString priorKind;
    for (int rowIndex = 0; rowIndex < lines.size(); ++rowIndex) {
        if (setup.rows.contains(rowIndex)) continue;
        const QVariantMap line = lines[rowIndex].toMap();
        const QString source = line.value(QStringLiteral("source")).toString();
        const QString kind = line.value(QStringLiteral("kind"),
                                        QStringLiteral("normal")).toString();
        const QString wantedList = kind == QStringLiteral("bullet")
            ? QStringLiteral("itemize")
            : kind == QStringLiteral("numbered") ? QStringLiteral("enumerate") : QString();
        if (wantedList != openList) {
            if (!openList.isEmpty())
                text += QStringLiteral("\\end{%1}\n").arg(openList);
            openList = wantedList;
            if (!openList.isEmpty())
                text += QStringLiteral("\\begin{%1}\n").arg(openList);
        }

        if (kind == QStringLiteral("image")) {
            const QString asset = line.value(QStringLiteral("asset")).toString();
            if (!asset.isEmpty()) {
                text += QStringLiteral(
                    "\\begin{figure}[htbp]\n\\centering\n"
                    "\\includegraphics[width=\\linewidth,height=0.65\\textheight,keepaspectratio]"
                    "{\\detokenize{%1}}\n").arg(asset);
                if (!source.trimmed().isEmpty())
                    text += QStringLiteral("\\caption{%1}\n").arg(escapedText(source));
                text += QStringLiteral("\\end{figure}\n");
            }
        } else if (kind == QStringLiteral("heading")) {
            text += QStringLiteral("\\section*{%1}\n").arg(escapedText(source));
        } else if (kind == QStringLiteral("subheading")) {
            text += QStringLiteral("\\subsection*{%1}\n").arg(escapedText(source));
        } else if (!openList.isEmpty()) {
            text += QStringLiteral("\\item %1\n").arg(inlineLineTex(line, setup.source));
        } else if (source.trimmed().isEmpty()) {
            text += QLatin1Char('\n');
        } else {
            const QString label = line.value(QStringLiteral("label")).toString();
            const QString defaultLabel = blockLabel(kind);
            if (!defaultLabel.isEmpty() && priorKind != kind) {
                text += QStringLiteral("\\medskip\\noindent\\textbf{%1. }\n")
                            .arg(escapedText(label.isEmpty() ? defaultLabel : label));
            }
            text += LatexSyntax::toTex(source, line.value("mode", "auto").toString(), setup.source)
                    + QStringLiteral("\\par\n");
        }
        priorKind = kind;
    }
    if (!openList.isEmpty())
        text += QStringLiteral("\\end{%1}\n").arg(openList);
    text += QStringLiteral("\\end{document}\n");
    return text;
}

bool writeTextFile(const QString &path, const QString &text) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    file.write(text.toUtf8());
    return file.commit();
}

QVariantMap normalizedDocument(QVariantMap result) {
    result.insert("spellLanguage", result.value("spellLanguage") == "en" ? "en" : "sv");
    result.insert("spellcheckEnabled", result.value("spellcheckEnabled", true).toBool());
    result.insert("spellingIgnored", result.value("spellingIgnored").toStringList());
    const bool multilineBlocks = result.value(QStringLiteral("format")).toString()
        == QStringLiteral("foldtex-3");
    QVariantList normalizedLines;
    const QVariantList storedLines = result.value(QStringLiteral("lines")).toList();
    for (const QVariant &storedLine : storedLines) {
        QVariantMap line = storedLine.toMap();
        QString source = line.value(QStringLiteral("source")).toString();
        source.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        source.replace(QLatin1Char('\r'), QLatin1Char('\n'));
        const QStringList parts = multilineBlocks ? QStringList{source}
            : source.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
        for (QString part : parts) {
            if (!multilineBlocks && (part.trimmed() == QStringLiteral("\\linebreak")
                || part.trimmed() == QStringLiteral("\\newline")))
                part.clear();
            QVariantMap normalizedLine = line;
            normalizedLine.insert(QStringLiteral("source"), part);
            if (!normalizedLine.contains(QStringLiteral("kind")))
                normalizedLine.insert(QStringLiteral("kind"), QStringLiteral("normal"));
            if (!normalizedLine.contains(QStringLiteral("label")))
                normalizedLine.insert(QStringLiteral("label"), QString());
            if (!normalizedLine.contains(QStringLiteral("asset")))
                normalizedLine.insert(QStringLiteral("asset"), QString());
            if (!normalizedLine.contains(QStringLiteral("slide")))
                normalizedLine.insert(QStringLiteral("slide"), -1);
            if (!normalizedLine.contains(QStringLiteral("mode")))
                normalizedLine.insert(QStringLiteral("mode"), QStringLiteral("auto"));
            normalizedLines.append(normalizedLine);
        }
    }
    result.insert(QStringLiteral("format"), QStringLiteral("foldtex-3"));
    result.insert(QStringLiteral("lines"), LatexSyntax::migrateRowModes(normalizedLines));
    if (!result.contains(QStringLiteral("course")))
        result.insert(QStringLiteral("course"), QString());
    if (!result.contains(QStringLiteral("lecture")))
        result.insert(QStringLiteral("lecture"), QString());
    if (!result.contains(QStringLiteral("lectureDate")))
        result.insert(QStringLiteral("lectureDate"), QString());
    if (!result.contains(QStringLiteral("noteKind")))
        result.insert(QStringLiteral("noteKind"), QStringLiteral("lecture"));
    if (!result.contains(QStringLiteral("problemSet")))
        result.insert(QStringLiteral("problemSet"), QString());
    if (!result.contains(QStringLiteral("sourcePdf")))
        result.insert(QStringLiteral("sourcePdf"), QString());
    return result;
}

QVariantMap documentWithExportImages(const QVariantMap &document,
                                     const QString &assetDirectory,
                                     const QString &assetPrefix,
                                     QString *error) {
    QVariantMap prepared = document;
    QVariantList lines = prepared.value(QStringLiteral("lines")).toList();
    if (!QDir().mkpath(assetDirectory)) {
        *error = QStringLiteral("Could not make the export image folder");
        return prepared;
    }
    int imageNumber = 0;
    for (QVariant &value : lines) {
        QVariantMap line = value.toMap();
        if (line.value(QStringLiteral("kind")).toString() != QStringLiteral("image"))
            continue;
        const QString source = line.value(QStringLiteral("asset")).toString();
        if (source.isEmpty())
            continue;
        QImage image(source);
        if (image.isNull()) {
            *error = QStringLiteral("Could not read export image: %1").arg(source);
            return prepared;
        }
        const QString name = QStringLiteral("image-%1.png").arg(++imageNumber);
        if (!image.save(QDir(assetDirectory).filePath(name), "PNG")) {
            *error = QStringLiteral("Could not prepare export image: %1").arg(source);
            return prepared;
        }
        line.insert(QStringLiteral("asset"), assetPrefix + QLatin1Char('/') + name);
        value = line;
    }
    prepared.insert(QStringLiteral("lines"), lines);
    return prepared;
}

QString storedAssetPath(const QString &documentPath, const QString &assetPath) {
    if (assetPath.isEmpty())
        return QString();
    const QFileInfo asset(assetPath);
    if (!asset.isAbsolute())
        return assetPath;
    const QDir base(QFileInfo(documentPath).absolutePath());
    const QString relative = base.relativeFilePath(asset.absoluteFilePath());
    return relative.startsWith(QStringLiteral("../")) ? asset.absoluteFilePath() : relative;
}

QString resolvedAssetPath(const QString &documentPath, const QString &assetPath) {
    if (assetPath.isEmpty() || QFileInfo(assetPath).isAbsolute())
        return assetPath;
    return QDir(QFileInfo(documentPath).absolutePath()).absoluteFilePath(assetPath);
}

QVariantMap documentForStorage(const QString &path, QVariantMap document,
                               bool portableAssets) {
    // Callers already supply logical blocks. Only legacy files need splitting
    // on load; saves, recovery copies and snapshots must preserve source lines.
    document.insert(QStringLiteral("format"), QStringLiteral("foldtex-3"));
    QVariantMap stored = normalizedDocument(document);
    if (portableAssets) {
        stored.remove(QStringLiteral("baseRevision"));
        stored.remove(QStringLiteral("recoveryDirty"));
        stored.insert(QStringLiteral("sourcePdf"), storedAssetPath(
            path, stored.value(QStringLiteral("sourcePdf")).toString()));
        QVariantList lines = stored.value(QStringLiteral("lines")).toList();
        for (QVariant &value : lines) {
            QVariantMap line = value.toMap();
            line.insert(QStringLiteral("asset"), storedAssetPath(
                path, line.value(QStringLiteral("asset")).toString()));
            value = line;
        }
        stored.insert(QStringLiteral("lines"), lines);
    }
    return stored;
}

bool writeDocument(const QString &path, const QVariantMap &document,
                   bool portableAssets = true) {
    const QVariantMap stored = documentForStorage(path, document, portableAssets);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument::fromVariant(stored).toJson(QJsonDocument::Indented));
    return file.commit();
}
}

Backend::Backend(QObject *parent) : QObject(parent) {
    static const int editorType = qmlRegisterType<DocumentEditor>("FoldTeX", 1, 0, "DocumentEditor");
    Q_UNUSED(editorType);
    m_availableFonts = QFontDatabase::families();
    m_availableFonts.sort(Qt::CaseInsensitive);
    QSettings settings;
    const QString systemMono = QFontInfo(QFont(QStringLiteral("monospace"))).family();
    m_editorFontFamily = settings.value(QStringLiteral("editor/fontFamily"), systemMono).toString();
    if (!m_availableFonts.contains(m_editorFontFamily))
        m_editorFontFamily = systemMono;
    m_editorFontSize = qBound(10, settings.value(QStringLiteral("editor/fontSize"), 17).toInt(), 40);
    m_editorSideMargin = qBound(16, settings.value(QStringLiteral("editor/sideMargin"), 100).toInt(), 240);

    connect(&m_themeWatcher, &QFileSystemWatcher::fileChanged, this, [this] {
        loadTheme();
        watchTheme();
    });
    connect(&m_themeWatcher, &QFileSystemWatcher::directoryChanged, this, [this] {
        loadTheme();
        watchTheme();
    });
    loadTheme();
    watchTheme();

    m_renderThread = new QThread(this);
    m_renderWorker = new QObject;
    m_renderWorker->moveToThread(m_renderThread);
    connect(this, &Backend::renderRequested, m_renderWorker,
            [this](int requestId, const QString &source,
                   const QString &color, int pixelSize, const QString &preamble, bool textMode) {
                const QVariantMap result = render(source, color, pixelSize, preamble, textMode);
                QMetaObject::invokeMethod(this, [this, requestId, result] {
                    emit renderFinished(requestId, result);
                }, Qt::QueuedConnection);
            });
    connect(m_renderThread, &QThread::finished, m_renderWorker, &QObject::deleteLater);
    m_renderThread->start();
}

Backend::~Backend() {
    m_renderThread->quit();
    m_renderThread->wait();
}

void Backend::renderAsync(int requestId, const QString &source,
                          const QString &color, int pixelSize, const QString &preamble, bool textMode) {
    emit renderRequested(requestId, source, color, pixelSize, preamble, textMode);
}

QString Backend::localPath(const QString &urlOrPath) const {
    const QUrl url(urlOrPath);
    return url.isLocalFile() ? url.toLocalFile() : urlOrPath;
}

QString Backend::recoveryPath() const {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::StateLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/recovery.json");
}

QString Backend::recoveryPathForDocument(const QVariantMap &document) const {
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::StateLocation)
        + QStringLiteral("/recoveries");
    QDir().mkpath(directory);
    QString identity = document.value(QStringLiteral("documentPath")).toString();
    if (!identity.isEmpty())
        identity = normalizedPath(identity);
    if (identity.isEmpty())
        identity = document.value(QStringLiteral("recoveryId")).toString();
    if (identity.isEmpty())
        identity = QStringLiteral("untitled");
    const QByteArray key = QCryptographicHash::hash(identity.toUtf8(),
                                                     QCryptographicHash::Sha256)
                               .toHex().left(20);
    return directory + QLatin1Char('/') + QString::fromLatin1(key)
        + QStringLiteral(".json");
}

QString Backend::firstLatexError(const QByteArray &output) const {
    const QString text = QString::fromUtf8(output);
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.startsWith(QLatin1Char('!')))
            return line.mid(1).trimmed();
    }
    for (const QString &line : lines) {
        if (line.contains(QStringLiteral("error"), Qt::CaseInsensitive))
            return line.trimmed();
    }
    return QStringLiteral("LaTeX could not render this line");
}

namespace {
QVariantMap svgResult(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {{"error", file.errorString()}};
    const QByteArray data = file.readAll();
    QSvgRenderer svg(data);
    const QRectF view = svg.viewBoxF();
    QVariantList hits;
    QXmlStreamReader xml(data);
    static const QRegularExpression sourceGroup(R"(^ft-src-(\d+)-(\d+)$)");
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) continue;
        const QString id = xml.attributes().value("id").toString();
        const auto match = sourceGroup.match(id);
        if (!match.hasMatch()) continue;
        const QRectF box = svg.transformForElement(id).mapRect(svg.boundsOnElement(id));
        if (!box.isValid() || view.width() <= 0 || view.height() <= 0) continue;
        hits.append(QVariantMap{{"start", match.captured(1).toInt()}, {"end", match.captured(2).toInt()},
            {"x", (box.x() - view.x()) / view.width()}, {"y", (box.y() - view.y()) / view.height()},
            {"width", box.width() / view.width()}, {"height", box.height() / view.height()}});
    }
    return {{"url", QUrl::fromLocalFile(path).toString()}, {"error", QString()}, {"hits", hits}};
}
}
QVariantMap Backend::render(const QString &source, const QString &color, int pixelSize,
                           const QString &preamble, bool textMode) {
    if (source.trimmed().isEmpty())
        return {{QStringLiteral("url"), QString()}, {QStringLiteral("error"), QString()}};

    const QByteArray key = QCryptographicHash::hash(
        (QStringLiteral("v7\n") + source + QLatin1Char('\n') + color + QLatin1Char('\n')
         + QString::number(pixelSize) + (preamble.isEmpty() ? QString() : "\n" + preamble)
         + (textMode ? "\ntext" : "")).toUtf8(),
        QCryptographicHash::Sha256).toHex();
    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/renders/") + QString::fromLatin1(key);
    QDir().mkpath(cacheRoot);
    const QString texPath = cacheRoot + QStringLiteral("/line.tex");
    const QString dviPath = cacheRoot + QStringLiteral("/line.dvi");
    const QString svgPath = cacheRoot + QStringLiteral("/line.svg");
    const QString errorPath = cacheRoot + QStringLiteral("/line.error");
    if (QFile::exists(svgPath)) return svgResult(svgPath);
    QFile cachedError(errorPath);
    if (cachedError.open(QIODevice::ReadOnly | QIODevice::Text))
        return {{QStringLiteral("error"), QString::fromUtf8(cachedError.readAll())}};

    QString hex = color;
    hex.remove(QLatin1Char('#'));
    if (hex.size() != 6)
        hex = QStringLiteral("eeeeee");

    const double pointSize = qBound(10, pixelSize, 40) * (21.0 / 18.0);
    const double lineSize = pointSize * 1.2;
    QString packages = preamble;
    static const QRegularExpression classPattern(R"(^\s*\\documentclass(?:\[[^\]]*\])?\{[^}]+\}\s*)");
    const auto classMatch = classPattern.match(packages);
    const bool customClass = classMatch.hasMatch();
    const QString documentClass = customClass ? classMatch.captured() : "\\documentclass[preview,border=0pt]{standalone}\n";
    if (customClass) packages.remove(0, classMatch.capturedLength());
    const QString plain = textMode ? source : bareMath(source);
    QString body = textMode ? plain : LatexSyntax::instrumentMath(plain);
    QString compilationError;
    for (int attempt = 0; attempt < 2; ++attempt) {
        QSaveFile texFile(texPath);
        if (!texFile.open(QIODevice::WriteOnly | QIODevice::Text))
            return {{"error", "Could not create render file"}};
        QTextStream out(&texFile);
        out << documentClass << "\n\\usepackage{amsmath,amssymb,mathtools,xcolor}\n"
               "\\usepackage[T1]{fontenc}\n"
            << packages << '\n'
            << (customClass ? "\\usepackage[active,tightpage]{preview}\n" : "")
            << "\\definecolor{foldtext}{HTML}{" << hex << "}\n"
               "\\begin{document}\n" << (customClass ? "\\begin{preview}" : "")
            << "\\fontsize{" << QString::number(pointSize, 'f', 2) << "pt}{"
            << QString::number(lineSize, 'f', 2) << "pt}\\selectfont\n\\color{foldtext}"
            << (textMode ? "" : "\\(\\displaystyle ") << body << (textMode ? "" : "\\)")
            << (customClass ? "\\end{preview}" : "") << "\n\\end{document}\n";
        if (!texFile.commit()) return {{"error", "Could not write render file"}};
        QProcess latex;
        latex.setWorkingDirectory(cacheRoot);
        latex.setProcessChannelMode(QProcess::MergedChannels);
        latex.start(QStringLiteral("latex"), {"-interaction=nonstopmode", "-halt-on-error", "-no-shell-escape",
            "-output-directory=" + cacheRoot, texPath});
        if (!latex.waitForFinished(10000)) { latex.kill(); latex.waitForFinished(); return {{"error", "LaTeX rendering timed out"}}; }
        if (latex.exitStatus() == QProcess::NormalExit && latex.exitCode() == 0) { compilationError.clear(); break; }
        compilationError = firstLatexError(latex.readAll());
        if (body == plain) break;
        // Exotic commands may consume unbraced tokens. Keep their ordinary TeX
        // rendering working even when they cannot accept source-map specials.
        body = plain;
    }
    if (!compilationError.isEmpty()) {
        writeTextFile(errorPath, compilationError);
        return {{"error", compilationError}};
    }

    QProcess svg;
    svg.setWorkingDirectory(cacheRoot);
    svg.setProcessChannelMode(QProcess::MergedChannels);
    svg.start(QStringLiteral("dvisvgm"), {
        QStringLiteral("--no-fonts"), QStringLiteral("--exact"), QStringLiteral("--bbox=min"),
        QStringLiteral("--output=" ) + svgPath, dviPath
    });
    if (!svg.waitForFinished(10000) || svg.exitStatus() != QProcess::NormalExit
        || svg.exitCode() != 0) {
        svg.kill();
        return {{QStringLiteral("error"), firstLatexError(svg.readAll())}};
    }

    return svgResult(svgPath);
}

QVariantList Backend::migrateRowModes(const QVariantList &rows) const {
    return LatexSyntax::migrateRowModes(rows);
}

QVariantMap Backend::loadDocument(const QString &urlOrPath) const {
    const QString path = localPath(urlOrPath);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {{QStringLiteral("error"), file.errorString()}};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
        return {{QStringLiteral("error"), QStringLiteral("Not a FoldTeX document")}};
    QVariantMap result = normalizedDocument(document.object().toVariantMap());
    result.insert(QStringLiteral("sourcePdf"), resolvedAssetPath(
        path, result.value(QStringLiteral("sourcePdf")).toString()));
    QVariantList lines = result.value(QStringLiteral("lines")).toList();
    for (QVariant &value : lines) {
        QVariantMap line = value.toMap();
        line.insert(QStringLiteral("asset"), resolvedAssetPath(
            path, line.value(QStringLiteral("asset")).toString()));
        value = line;
    }
    result.insert(QStringLiteral("lines"), lines);
    return result;
}

bool Backend::saveDocumentData(const QString &urlOrPath, const QVariantMap &document) {
    return writeDocument(localPath(urlOrPath), document);
}

QVariantMap Backend::saveDocumentDataChecked(const QString &urlOrPath,
                                             const QVariantMap &document,
                                             const QString &expectedRevision) {
    const QString path = localPath(urlOrPath);
    const QString currentRevision = fileRevision(path);
    if (!expectedRevision.isNull() && currentRevision != expectedRevision) {
        return {
            {QStringLiteral("saved"), false},
            {QStringLiteral("conflict"), true},
            {QStringLiteral("revision"), currentRevision}
        };
    }
    const bool saved = writeDocument(path, document);
    return {
        {QStringLiteral("saved"), saved},
        {QStringLiteral("conflict"), false},
        {QStringLiteral("revision"), saved ? fileRevision(path) : currentRevision}
    };
}

qint64 Backend::fileModified(const QString &urlOrPath) const {
    const QFileInfo info(localPath(urlOrPath));
    return info.exists() ? info.lastModified().toMSecsSinceEpoch() : 0;
}

QString Backend::fileRevision(const QString &urlOrPath) const {
    QFile file(localPath(urlOrPath));
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
        return QString();
    return QString::fromLatin1(hash.result().toHex());
}

QString Backend::normalizedPath(const QString &urlOrPath) const {
    if (urlOrPath.isEmpty())
        return QString();
    return QDir::cleanPath(QFileInfo(localPath(urlOrPath)).absoluteFilePath());
}

QVariantList Backend::customSnippets() const {
    return m_snippetStore.entries();
}

QVariantMap Backend::replaceCustomSnippets(const QVariantList &entries) const {
    return m_snippetStore.replace(entries);
}

QVariantMap Backend::customSnippet(const QString &trigger, const QString &course) const {
    return m_snippetStore.resolve(trigger, course);
}

QVariantMap Backend::importCustomSnippets(const QString &urlOrPath) const {
    return m_snippetStore.importFile(localPath(urlOrPath));
}

QVariantMap Backend::exportCustomSnippets(const QString &urlOrPath) const {
    return m_snippetStore.exportFile(localPath(urlOrPath));
}

bool Backend::saveDocument(const QString &urlOrPath, const QString &title,
                           const QVariantList &lines) {
    return saveDocumentData(urlOrPath, {
        {QStringLiteral("title"), title},
        {QStringLiteral("lines"), lines}
    });
}

QVariantMap Backend::loadRecovery() {
    QSettings settings;
    const QString current = settings.value(QStringLiteral("recovery/current")).toString();
    if (!current.isEmpty() && QFileInfo::exists(current))
        return loadDocument(current);
    return loadDocument(recoveryPath());
}

void Backend::saveRecovery(const QString &title, const QVariantList &lines) {
    saveRecoveryData({
        {QStringLiteral("title"), title},
        {QStringLiteral("lines"), lines},
        {QStringLiteral("documentPath"), QString()},
        {QStringLiteral("recoveryDirty"), true}
    });
}

bool Backend::saveRecoveryData(const QVariantMap &document) {
    const QString path = recoveryPathForDocument(document);
    if (!writeDocument(path, document, false))
        return false;
    QSettings().setValue(QStringLiteral("recovery/current"), path);
    return true;
}

bool Backend::saveSnapshot(const QVariantMap &document) {
    const QString state = QStandardPaths::writableLocation(QStandardPaths::StateLocation);
    const QString directory = state + QStringLiteral("/snapshots");
    QDir().mkpath(directory);
    const QString stamp = QDateTime::currentDateTimeUtc().toString(
        QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    const QString path = directory + QLatin1Char('/') + stamp + QStringLiteral(".foldtex");
    if (!writeDocument(path, document, false))
        return false;

    QDir snapshots(directory);
    const QFileInfoList files = snapshots.entryInfoList(
        {QStringLiteral("*.foldtex")}, QDir::Files, QDir::Time);
    for (int i = 30; i < files.size(); ++i)
        QFile::remove(files.at(i).absoluteFilePath());
    return true;
}

QVariantList Backend::recoveryEntries() const {
    QVariantList entries;
    const QString state = QStandardPaths::writableLocation(QStandardPaths::StateLocation);
    const QList<QPair<QString, QString>> locations = {
        {state + QStringLiteral("/recoveries"), QStringLiteral("Recovery")},
        {state + QStringLiteral("/snapshots"), QStringLiteral("Snapshot")}
    };
    for (const auto &location : locations) {
        QDir directory(location.first);
        const QFileInfoList files = directory.entryInfoList(
            {QStringLiteral("*.json"), QStringLiteral("*.foldtex")},
            QDir::Files, QDir::Time);
        for (const QFileInfo &fileInfo : files) {
            const QVariantMap document = loadDocument(fileInfo.absoluteFilePath());
            if (document.contains(QStringLiteral("error")))
                continue;
            QVariantMap entry;
            entry.insert(QStringLiteral("path"), fileInfo.absoluteFilePath());
            entry.insert(QStringLiteral("entryKind"), location.second);
            entry.insert(QStringLiteral("title"), document.value(
                QStringLiteral("title"), QStringLiteral("Untitled")).toString());
            entry.insert(QStringLiteral("course"), document.value(
                QStringLiteral("course")).toString());
            entry.insert(QStringLiteral("lectureDate"), document.value(
                QStringLiteral("lectureDate")).toString());
            entry.insert(QStringLiteral("documentPath"), document.value(
                QStringLiteral("documentPath")).toString());
            entry.insert(QStringLiteral("modified"), fileInfo.lastModified());
            entry.insert(QStringLiteral("modifiedLabel"),
                         fileInfo.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm")));
            entries.append(entry);
        }
    }
    std::sort(entries.begin(), entries.end(), [](const QVariant &left, const QVariant &right) {
        return left.toMap().value(QStringLiteral("modified")).toDateTime()
            > right.toMap().value(QStringLiteral("modified")).toDateTime();
    });
    return entries;
}

bool Backend::removeRecovery(const QString &urlOrPath) const {
    const QString path = QFileInfo(localPath(urlOrPath)).absoluteFilePath();
    const QString state = QFileInfo(QStandardPaths::writableLocation(
        QStandardPaths::StateLocation)).absoluteFilePath();
    const QString recoveries = state + QStringLiteral("/recoveries/");
    const QString snapshots = state + QStringLiteral("/snapshots/");
    if (!path.startsWith(recoveries) && !path.startsWith(snapshots))
        return false;
    if (!QFileInfo::exists(path))
        return true;
    return QFile::remove(path);
}

QVariantMap Backend::environmentStatus() const {
    QVariantList missing;
    const QList<QPair<QString, QString>> commands = {
        {QStringLiteral("latex"), QStringLiteral("LaTeX")},
        {QStringLiteral("dvisvgm"), QStringLiteral("dvisvgm")},
        {QStringLiteral("pdflatex"), QStringLiteral("pdfLaTeX")}
    };
    for (const auto &command : commands) {
        if (QStandardPaths::findExecutable(command.first).isEmpty())
            missing.append(command.second);
    }
    return {
        {QStringLiteral("ready"), missing.isEmpty()},
        {QStringLiteral("missing"), missing}
    };
}

QString Backend::newRecoveryId() const {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QVariantList Backend::searchCourse(const QString &documentUrl, const QString &query) const {
    QVariantList results;
    const QString needle = query.trimmed();
    if (needle.isEmpty() || documentUrl.isEmpty())
        return results;
    const QFileInfo current(localPath(documentUrl));
    QDir directory(current.absolutePath());
    const QFileInfoList files = directory.entryInfoList(
        {QStringLiteral("*.foldtex")}, QDir::Files, QDir::Name);
    for (const QFileInfo &fileInfo : files) {
        QFile file(fileInfo.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QJsonDocument json = QJsonDocument::fromJson(file.readAll());
        if (!json.isObject())
            continue;
        const QVariantMap document = normalizedDocument(json.object().toVariantMap());
        const QVariantList rows = document.value(QStringLiteral("lines")).toList();
        for (int line = 0; line < rows.size(); ++line) {
            const QVariantMap row = rows.at(line).toMap();
            const QString source = row.value(QStringLiteral("source")).toString();
            if (!source.contains(needle, Qt::CaseInsensitive))
                continue;
            results.append(QVariantMap{
                {QStringLiteral("path"), fileInfo.absoluteFilePath()},
                {QStringLiteral("title"), document.value(QStringLiteral("title"))},
                {QStringLiteral("course"), document.value(QStringLiteral("course"))},
                {QStringLiteral("lecture"), document.value(QStringLiteral("lecture"))},
                {QStringLiteral("line"), line},
                {QStringLiteral("source"), source},
                {QStringLiteral("kind"), row.value(QStringLiteral("kind"))}
            });
        }
    }
    return results;
}

void Backend::rememberNoteFolder(const QString &documentUrl) const {
    if (documentUrl.isEmpty())
        return;
    const QFileInfo documentInfo(localPath(documentUrl));
    const QString folder = documentInfo.absolutePath();
    if (!QDir(folder).exists())
        return;
    addNoteFolder(folder);
}

void Backend::addNoteFolder(const QString &folderUrl) const {
    if (folderUrl.isEmpty())
        return;
    const QString folder = QDir(QFileInfo(localPath(folderUrl)).absoluteFilePath()).absolutePath();
    if (!QFileInfo(folder).isDir())
        return;
    QSettings settings;
    QStringList folders = settings.value(QStringLiteral("library/folders")).toStringList();
    for (const QString &stored : folders) {
        const QString root = QDir(stored).absolutePath();
        if (folder == root || folder.startsWith(root + QLatin1Char('/')))
            return;
    }
    for (int i = folders.size() - 1; i >= 0; --i) {
        const QString stored = QDir(folders.at(i)).absolutePath();
        if (stored.startsWith(folder + QLatin1Char('/')))
            folders.removeAt(i);
    }
    folders.prepend(folder);
    settings.setValue(QStringLiteral("library/folders"), folders);
    m_noteLibraryCache.clear();
}

void Backend::removeNoteFolder(const QString &folderUrl) const {
    const QString folder = QDir(QFileInfo(localPath(folderUrl)).absoluteFilePath()).absolutePath();
    QSettings settings;
    QStringList folders = settings.value(QStringLiteral("library/folders")).toStringList();
    folders.removeAll(folder);
    settings.setValue(QStringLiteral("library/folders"), folders);
    m_noteLibraryCache.clear();
}

void Backend::rescanNoteLibrary() const {
    m_noteLibraryCache.clear();
}

void Backend::setNotePinned(const QString &documentUrl, bool pinned) const {
    const QString path = QFileInfo(localPath(documentUrl)).absoluteFilePath();
    if (path.isEmpty())
        return;
    QSettings settings;
    QStringList paths = settings.value(QStringLiteral("library/pinned")).toStringList();
    paths.removeAll(path);
    if (pinned)
        paths.prepend(path);
    settings.setValue(QStringLiteral("library/pinned"), paths);
}

void Backend::rememberOpenedNote(const QString &documentUrl) const {
    const QString path = QFileInfo(localPath(documentUrl)).absoluteFilePath();
    if (QFileInfo(path).isFile())
        QSettings().setValue(QStringLiteral("library/lastOpened"), path);
}

QVariantMap Backend::noteLibraryState() const {
    QSettings settings;
    const QStringList storedFolders = settings.value(
        QStringLiteral("library/folders")).toStringList();
    QVariantList folders;
    for (const QString &path : storedFolders) {
        folders.append(QVariantMap{
            {QStringLiteral("path"), path},
            {QStringLiteral("name"), QFileInfo(path).fileName().isEmpty()
                 ? path : QFileInfo(path).fileName()},
            {QStringLiteral("exists"), QFileInfo(path).isDir()}
        });
    }

    const QVariantList allNotes = noteLibrary(QString(), QStringLiteral("updated"));
    QStringList courses;
    for (const QVariant &value : allNotes) {
        const QString course = value.toMap().value(QStringLiteral("course")).toString();
        if (!course.isEmpty() && !courses.contains(course, Qt::CaseInsensitive))
            courses.append(course);
    }
    courses.sort(Qt::CaseInsensitive);

    const QString lastPath = settings.value(QStringLiteral("library/lastOpened")).toString();
    QVariantMap lastNote;
    for (const QVariant &value : allNotes) {
        if (value.toMap().value(QStringLiteral("path")).toString() == lastPath) {
            lastNote = value.toMap();
            break;
        }
    }
    return {{QStringLiteral("folders"), folders},
            {QStringLiteral("courses"), courses},
            {QStringLiteral("lastNote"), lastNote}};
}

QVariantList Backend::noteLibrary(const QString &query, const QString &sortKey,
                                  const QString &courseFilter,
                                  const QString &noteKindFilter) const {
    const QStringList terms = query.trimmed().split(
        QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    QSettings settings;
    const QStringList folders = settings.value(QStringLiteral("library/folders")).toStringList();
    const QStringList pinnedPaths = settings.value(QStringLiteral("library/pinned")).toStringList();
    QVariantList notes;
    QSet<QString> seenPaths;

    for (const QString &folder : folders) {
        QDir directory(folder);
        if (!directory.exists())
            continue;
        QDirIterator files(directory.absolutePath(), {QStringLiteral("*.foldtex")},
                           QDir::Files, QDirIterator::Subdirectories);
        while (files.hasNext()) {
            const QFileInfo fileInfo(files.next());
            const QString path = fileInfo.absoluteFilePath();
            if (seenPaths.contains(path))
                continue;
            seenPaths.insert(path);
            const qint64 modified = fileInfo.lastModified().toMSecsSinceEpoch();
            const qint64 size = fileInfo.size();
            QVariantMap cached = m_noteLibraryCache.value(path);
            QVariantMap note;
            if (cached.value(QStringLiteral("modified")).toLongLong() == modified
                && cached.value(QStringLiteral("size")).toLongLong() == size) {
                note = cached.value(QStringLiteral("note")).toMap();
            } else {
                QFile file(path);
                if (!file.open(QIODevice::ReadOnly))
                    continue;
                const QJsonDocument json = QJsonDocument::fromJson(file.readAll());
                if (!json.isObject())
                    continue;
                const QVariantMap document = normalizedDocument(json.object().toVariantMap());
                const QString title = document.value(QStringLiteral("title")).toString().trimmed();
                const QString course = document.value(QStringLiteral("course")).toString().trimmed();
                const QString lecture = document.value(QStringLiteral("lecture")).toString().trimmed();
                const QString date = document.value(QStringLiteral("lectureDate")).toString().trimmed();
                const QString noteKind = document.value(QStringLiteral("noteKind")).toString().trimmed();
                const QString problemSet = document.value(QStringLiteral("problemSet")).toString().trimmed();
                QString searchable = title + QLatin1Char('\n') + course + QLatin1Char('\n')
                    + lecture + QLatin1Char('\n') + problemSet + QLatin1Char('\n')
                    + noteKind + QLatin1Char('\n') + date + QLatin1Char('\n')
                    + fileInfo.completeBaseName();
                QStringList rowSources;
                const QVariantList rows = document.value(QStringLiteral("lines")).toList();
                for (const QVariant &row : rows) {
                    const QString source = row.toMap().value(QStringLiteral("source")).toString();
                    rowSources.append(source);
                    searchable += QLatin1Char('\n') + source;
                }
                note = {
                    {QStringLiteral("path"), path},
                    {QStringLiteral("title"), title.isEmpty() ? fileInfo.completeBaseName() : title},
                    {QStringLiteral("course"), course},
                    {QStringLiteral("lecture"), lecture},
                    {QStringLiteral("noteKind"), noteKind},
                    {QStringLiteral("problemSet"), problemSet},
                    {QStringLiteral("lectureDate"), date},
                    {QStringLiteral("modified"), modified},
                    {QStringLiteral("modifiedLabel"), fileInfo.lastModified().toString(
                         QStringLiteral("yyyy-MM-dd HH:mm"))},
                    {QStringLiteral("fileName"), fileInfo.fileName()},
                    {QStringLiteral("searchable"), searchable},
                    {QStringLiteral("rowSources"), rowSources}
                };
                m_noteLibraryCache.insert(path, {
                    {QStringLiteral("modified"), modified},
                    {QStringLiteral("size"), size},
                    {QStringLiteral("note"), note}
                });
            }

            const QString course = note.value(QStringLiteral("course")).toString();
            const QString noteKind = note.value(QStringLiteral("noteKind")).toString();
            if (!courseFilter.isEmpty()
                && course.compare(courseFilter, Qt::CaseInsensitive) != 0)
                continue;
            if (!noteKindFilter.isEmpty() && noteKind != noteKindFilter)
                continue;
            const QString searchable = note.value(QStringLiteral("searchable")).toString();
            bool matches = true;
            for (const QString &term : terms) {
                if (!searchable.contains(term, Qt::CaseInsensitive)) {
                    matches = false;
                    break;
                }
            }
            if (!matches)
                continue;

            QString preview;
            const QStringList rows = note.value(QStringLiteral("rowSources")).toStringList();
            for (const QString &row : rows) {
                bool rowMatches = false;
                for (const QString &term : terms) {
                    if (row.contains(term, Qt::CaseInsensitive)) {
                        rowMatches = true;
                        break;
                    }
                }
                if (rowMatches) {
                    preview = row.simplified();
                    if (preview.size() > 140)
                        preview = preview.left(137) + QStringLiteral("…");
                    break;
                }
            }
            note.remove(QStringLiteral("searchable"));
            note.remove(QStringLiteral("rowSources"));
            note.insert(QStringLiteral("matchPreview"), preview);
            note.insert(QStringLiteral("pinned"), pinnedPaths.contains(path));
            notes.append(note);
        }
    }

    for (auto it = m_noteLibraryCache.begin(); it != m_noteLibraryCache.end();) {
        if (!seenPaths.contains(it.key()))
            it = m_noteLibraryCache.erase(it);
        else
            ++it;
    }

    std::sort(notes.begin(), notes.end(), [&sortKey](const QVariant &leftValue,
                                                     const QVariant &rightValue) {
        const QVariantMap left = leftValue.toMap();
        const QVariantMap right = rightValue.toMap();
        if (left.value(QStringLiteral("pinned")).toBool()
            != right.value(QStringLiteral("pinned")).toBool())
            return left.value(QStringLiteral("pinned")).toBool();
        if (sortKey == QStringLiteral("title"))
            return QString::localeAwareCompare(left.value(QStringLiteral("title")).toString(),
                                               right.value(QStringLiteral("title")).toString()) < 0;
        if (sortKey == QStringLiteral("course")) {
            const int courseOrder = QString::localeAwareCompare(
                left.value(QStringLiteral("course")).toString(),
                right.value(QStringLiteral("course")).toString());
            if (courseOrder != 0)
                return courseOrder < 0;
            return QString::localeAwareCompare(left.value(QStringLiteral("title")).toString(),
                                               right.value(QStringLiteral("title")).toString()) < 0;
        }
        if (sortKey == QStringLiteral("date")) {
            const QString leftDate = left.value(QStringLiteral("lectureDate")).toString();
            const QString rightDate = right.value(QStringLiteral("lectureDate")).toString();
            if (leftDate != rightDate)
                return leftDate > rightDate;
        }
        return left.value(QStringLiteral("modified")).toLongLong()
            > right.value(QStringLiteral("modified")).toLongLong();
    });
    return notes;
}

QVariantMap Backend::importAsset(const QString &documentUrl,
                                 const QString &sourceUrl) const {
    if (documentUrl.isEmpty())
        return {{QStringLiteral("error"), QStringLiteral("Save the note before adding an image")}};
    const QString source = localPath(sourceUrl);
    const QFileInfo sourceInfo(source);
    if (!sourceInfo.isFile())
        return {{QStringLiteral("error"), QStringLiteral("Image file not found")}};
    const QFileInfo documentInfo(localPath(documentUrl));
    const QString assetDirectory = documentInfo.absolutePath() + QLatin1Char('/')
        + documentInfo.completeBaseName() + QStringLiteral(".assets");
    if (!QDir().mkpath(assetDirectory))
        return {{QStringLiteral("error"), QStringLiteral("Could not create the image folder")}};
    const QString suffix = sourceInfo.suffix().isEmpty()
        ? QStringLiteral("png") : sourceInfo.suffix().toLower();
    const QString target = assetDirectory + QLatin1Char('/')
        + QUuid::createUuid().toString(QUuid::WithoutBraces) + QLatin1Char('.') + suffix;
    if (!QFile::copy(source, target))
        return {{QStringLiteral("error"), QStringLiteral("Could not copy the image")}};
    return {{QStringLiteral("path"), target},
            {QStringLiteral("url"), QUrl::fromLocalFile(target).toString()}};
}

QVariantMap Backend::adoptAsset(const QString &documentUrl,
                                const QString &sourceUrl) const {
    const QString source = QFileInfo(localPath(sourceUrl)).absoluteFilePath();
    return importAsset(documentUrl, source);
}

bool Backend::removeTemporaryAsset(const QString &sourceUrl) const {
    const QString source = QFileInfo(localPath(sourceUrl)).absoluteFilePath();
    const QString temporaryDirectory = QFileInfo(
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/figures")).absoluteFilePath();
    if (QFileInfo(source).absolutePath() != temporaryDirectory)
        return false;
    return !QFileInfo::exists(source) || QFile::remove(source);
}

QVariantMap Backend::importClipboardImage(const QString &documentUrl) const {
    if (documentUrl.isEmpty())
        return {{QStringLiteral("error"), QStringLiteral("Save the note before adding an image")}};
    const QImage image = qGuiApp->clipboard()->image();
    if (image.isNull())
        return {{QStringLiteral("error"), QStringLiteral("The clipboard has no image")}};
    const QFileInfo documentInfo(localPath(documentUrl));
    const QString assetDirectory = documentInfo.absolutePath() + QLatin1Char('/')
        + documentInfo.completeBaseName() + QStringLiteral(".assets");
    if (!QDir().mkpath(assetDirectory))
        return {{QStringLiteral("error"), QStringLiteral("Could not create the image folder")}};
    const QString target = assetDirectory + QLatin1Char('/')
        + QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".png");
    if (!image.save(target, "PNG"))
        return {{QStringLiteral("error"), QStringLiteral("Could not save the image")}};
    return {{QStringLiteral("path"), target},
            {QStringLiteral("url"), QUrl::fromLocalFile(target).toString()}};
}

QVariantMap Backend::importPdf(const QString &documentUrl,
                               const QString &sourceUrl) const {
    if (documentUrl.isEmpty())
        return {{QStringLiteral("error"), QStringLiteral("Save the note before adding slides")}};
    const QString source = localPath(sourceUrl);
    const QFileInfo sourceInfo(source);
    if (!sourceInfo.isFile())
        return {{QStringLiteral("error"), QStringLiteral("PDF file not found")}};
    QFile sourceFile(source);
    if (!sourceFile.open(QIODevice::ReadOnly)
        || sourceFile.read(5) != QByteArrayLiteral("%PDF-"))
        return {{QStringLiteral("error"), QStringLiteral("This is not a PDF file")}};

    const QFileInfo documentInfo(localPath(documentUrl));
    const QString assetDirectory = documentInfo.absolutePath()
        + QStringLiteral("/.foldtex-assets");
    if (!QDir().mkpath(assetDirectory))
        return {{QStringLiteral("error"), QStringLiteral("Could not create the course asset folder")}};
    sourceFile.seek(0);
    const QByteArray digest = QCryptographicHash::hash(
        sourceFile.readAll(), QCryptographicHash::Sha256).toHex().left(12);
    QString base = sourceInfo.completeBaseName();
    base.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")),
                 QStringLiteral("-"));
    if (base.isEmpty())
        base = QStringLiteral("slides");
    const QString target = assetDirectory + QLatin1Char('/')
        + QString::fromLatin1(digest) + QLatin1Char('-') + base + QStringLiteral(".pdf");
    if (!QFile::exists(target) && !QFile::copy(source, target))
        return {{QStringLiteral("error"), QStringLiteral("Could not copy the PDF")}};
    return {{QStringLiteral("path"), target},
            {QStringLiteral("url"), QUrl::fromLocalFile(target).toString()}};
}

QVariantMap Backend::newFigureAsset(const QString &documentUrl) const {
    QString assetDirectory;
    if (documentUrl.isEmpty()) {
        assetDirectory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
            + QStringLiteral("/figures");
    } else {
        const QFileInfo documentInfo(localPath(documentUrl));
        assetDirectory = documentInfo.absolutePath() + QLatin1Char('/')
            + documentInfo.completeBaseName() + QStringLiteral(".assets");
    }
    if (!QDir().mkpath(assetDirectory))
        return {{QStringLiteral("error"), QStringLiteral("Could not create the image folder")}};
    const QString target = assetDirectory + QLatin1Char('/')
        + QUuid::createUuid().toString(QUuid::WithoutBraces) + QStringLiteral(".png");
    return {{QStringLiteral("path"), target},
            {QStringLiteral("url"), QUrl::fromLocalFile(target).toString()}};
}

bool Backend::saveFigure(const QString &path, const QString &actionsJson,
                         int width, int height, const QString &background,
                         const QString &foreground, const QString &fontFamily) const {
    if (path.isEmpty() || width < 1 || height < 1)
        return false;
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(background));
    const QVariantList actions = QJsonDocument::fromJson(actionsJson.toUtf8())
                                     .array().toVariantList();
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    for (const QVariant &value : actions) {
        const QVariantMap action = value.toMap();
        const QString type = action.value(QStringLiteral("type")).toString();
        const QColor color(type == QStringLiteral("eraser") ? background : foreground);
        painter.setPen(QPen(color, type == QStringLiteral("eraser") ? 22 : 3,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        if (type == QStringLiteral("pen") || type == QStringLiteral("eraser")) {
            const QVariantList points = action.value(QStringLiteral("points")).toList();
            for (int i = 1; i < points.size(); ++i) {
                const QVariantMap from = points.at(i - 1).toMap();
                const QVariantMap to = points.at(i).toMap();
                painter.drawLine(QPointF(from.value(QStringLiteral("x")).toReal(),
                                         from.value(QStringLiteral("y")).toReal()),
                                 QPointF(to.value(QStringLiteral("x")).toReal(),
                                         to.value(QStringLiteral("y")).toReal()));
            }
        } else if (type == QStringLiteral("text")) {
            QFont font = painter.font();
            font.setFamily(fontFamily);
            font.setPixelSize(qMax(8, action.value(QStringLiteral("size"), 22).toInt()));
            painter.setFont(font);
            painter.drawText(QPointF(action.value(QStringLiteral("x")).toReal(),
                                     action.value(QStringLiteral("y")).toReal()),
                             action.value(QStringLiteral("text")).toString());
        } else {
            const QPointF from(action.value(QStringLiteral("x1")).toReal(),
                               action.value(QStringLiteral("y1")).toReal());
            const QPointF to(action.value(QStringLiteral("x2")).toReal(),
                             action.value(QStringLiteral("y2")).toReal());
            if (type == QStringLiteral("box")) {
                painter.drawRect(QRectF(from, to).normalized());
            } else {
                painter.drawLine(from, to);
                if (type == QStringLiteral("arrow")) {
                    const QLineF line(from, to);
                    const qreal angle = qDegreesToRadians(-line.angle());
                    constexpr qreal arrowSize = 13;
                    painter.drawLine(to, to - QPointF(arrowSize * qCos(angle - M_PI / 6),
                                                       arrowSize * qSin(angle - M_PI / 6)));
                    painter.drawLine(to, to - QPointF(arrowSize * qCos(angle + M_PI / 6),
                                                       arrowSize * qSin(angle + M_PI / 6)));
                }
            }
        }
    }
    painter.end();
    return image.save(localPath(path), "PNG");
}

QString Backend::fileUrl(const QString &path) const {
    return path.isEmpty() ? QString() : QUrl::fromLocalFile(localPath(path)).toString();
}

QString Backend::latexHint(const QString &source, const QString &compilerError) const {
    int braces = 0;
    for (int i = 0; i < source.size(); ++i) {
        if (source.at(i) == QLatin1Char('\\')) {
            ++i;
            continue;
        }
        if (source.at(i) == QLatin1Char('{'))
            ++braces;
        else if (source.at(i) == QLatin1Char('}')) {
            --braces;
            if (braces < 0)
                return QStringLiteral("Remove the extra closing brace }");
        }
    }
    if (braces > 0)
        return QStringLiteral("Add a closing brace }");
    if (source.trimmed().endsWith(QLatin1Char('\\')))
        return QStringLiteral("Remove the final backslash");
    if (compilerError.contains(QStringLiteral("Undefined control sequence"),
                               Qt::CaseInsensitive))
        return QStringLiteral("Check the LaTeX command name");
    if (compilerError.contains(QStringLiteral("Double subscript"), Qt::CaseInsensitive))
        return QStringLiteral("Put the full subscript inside one pair of braces");
    if (compilerError.contains(QStringLiteral("Double superscript"), Qt::CaseInsensitive))
        return QStringLiteral("Put the full power inside one pair of braces");
    return QStringLiteral("Could not render this row");
}

void Backend::setEditorFont(const QString &family, int pixelSize) {
    const QString nextFamily = m_availableFonts.contains(family) ? family : m_editorFontFamily;
    const int nextSize = qBound(10, pixelSize, 40);
    if (nextFamily == m_editorFontFamily && nextSize == m_editorFontSize)
        return;
    m_editorFontFamily = nextFamily;
    m_editorFontSize = nextSize;
    QSettings settings;
    settings.setValue(QStringLiteral("editor/fontFamily"), m_editorFontFamily);
    settings.setValue(QStringLiteral("editor/fontSize"), m_editorFontSize);
    emit editorFontChanged();
}

void Backend::setEditorSideMargin(int margin) {
    const int nextMargin = qBound(16, margin, 240);
    if (nextMargin == m_editorSideMargin)
        return;
    m_editorSideMargin = nextMargin;
    QSettings settings;
    settings.setValue(QStringLiteral("editor/sideMargin"), m_editorSideMargin);
    emit editorSideMarginChanged();
}

QString Backend::clipboardText() const {
    return QGuiApplication::clipboard()->text();
}

void Backend::setClipboardText(const QString &text) {
    QGuiApplication::clipboard()->setText(text);
}

QVariantMap Backend::exportTex(const QString &urlOrPath, const QString &title,
                               const QVariantList &lines) {
    return exportDocumentTex(urlOrPath, {
        {QStringLiteral("title"), title},
        {QStringLiteral("lines"), lines}
    });
}

QVariantMap Backend::exportDocumentTex(const QString &urlOrPath,
                                       const QVariantMap &document) {
    const QString outputPath = localPath(urlOrPath);
    const QDir outputDirectory = QFileInfo(outputPath).absoluteDir();
    QTemporaryDir staging(outputDirectory.filePath(QStringLiteral(".foldtex-export-XXXXXX")));
    if (!staging.isValid())
        return {{QStringLiteral("error"), QStringLiteral("Could not prepare export images")}};
    const QString assetName = QStringLiteral(".foldtex-assets-")
        + QUuid::createUuid().toString(QUuid::Id128).left(12);
    const QString stagedAssets = staging.path() + QStringLiteral("/assets");
    QString error;
    const QVariantMap prepared = documentWithExportImages(
        document, stagedAssets, assetName, &error);
    if (!error.isEmpty())
        return {{QStringLiteral("error"), error}};
    const QString finalAssets = outputDirectory.filePath(assetName);
    if (!QDir().rename(stagedAssets, finalAssets))
        return {{QStringLiteral("error"), QStringLiteral("Could not place export images")}};
    if (!writeTextFile(outputPath, documentTex(prepared))) {
        QDir(finalAssets).removeRecursively();
        return {{QStringLiteral("error"), QStringLiteral("Could not write the TeX file")}};
    }
    return {{QStringLiteral("error"), QString()}};
}

QVariantMap Backend::exportPdf(const QString &urlOrPath, const QString &title,
                               const QVariantList &lines) {
    return exportDocumentPdf(urlOrPath, {
        {QStringLiteral("title"), title},
        {QStringLiteral("lines"), lines}
    });
}

QVariantMap Backend::exportDocumentPdf(const QString &urlOrPath,
                                       const QVariantMap &document) {
    QTemporaryDir temp;
    if (!temp.isValid())
        return {{QStringLiteral("error"), QStringLiteral("Could not make a temporary folder")}};
    QString imageError;
    const QVariantMap prepared = documentWithExportImages(
        document, temp.path() + QStringLiteral("/foldtex-assets"),
        QStringLiteral("foldtex-assets"), &imageError);
    if (!imageError.isEmpty())
        return {{QStringLiteral("error"), imageError}};
    const QString texPath = temp.path() + QStringLiteral("/foldtex-export.tex");
    if (!writeTextFile(texPath, documentTex(prepared)))
        return {{QStringLiteral("error"), QStringLiteral("Could not prepare the PDF")}};

    QProcess latex;
    latex.setWorkingDirectory(temp.path());
    latex.setProcessChannelMode(QProcess::MergedChannels);
    latex.start(QStringLiteral("pdflatex"), {
        QStringLiteral("-interaction=nonstopmode"), QStringLiteral("-halt-on-error"),
        QStringLiteral("-no-shell-escape"), QStringLiteral("-output-directory=" ) + temp.path(),
        texPath
    });
    if (!latex.waitForFinished(15000) || latex.exitStatus() != QProcess::NormalExit
        || latex.exitCode() != 0) {
        latex.kill();
        return {{QStringLiteral("error"), firstLatexError(latex.readAll())}};
    }

    QFile pdf(temp.path() + QStringLiteral("/foldtex-export.pdf"));
    if (!pdf.open(QIODevice::ReadOnly))
        return {{QStringLiteral("error"), QStringLiteral("PDF output was not created")}};
    QSaveFile output(localPath(urlOrPath));
    if (!output.open(QIODevice::WriteOnly) || output.write(pdf.readAll()) < 0 || !output.commit())
        return {{QStringLiteral("error"), QStringLiteral("Could not write the PDF file")}};
    return {{QStringLiteral("error"), QString()}};
}

void Backend::loadTheme() {
    m_background = QStringLiteral("#101010");
    m_foreground = QStringLiteral("#eeeeee");
    m_accent = QStringLiteral("#5584aa");
    m_selection = QStringLiteral("#186a9a");
    const QString path = QDir::homePath()
        + QStringLiteral("/.local/state/omarchy/current/theme/colors.toml");
    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            const int equals = line.indexOf(QLatin1Char('='));
            if (line.isEmpty() || line.startsWith(QLatin1Char('#')) || equals < 0)
                continue;
            const QString key = line.left(equals).trimmed();
            const QString value = unquote(line.mid(equals + 1));
            if (key == QStringLiteral("background")) m_background = value;
            else if (key == QStringLiteral("foreground")) m_foreground = value;
            else if (key == QStringLiteral("accent")) m_accent = value;
            else if (key == QStringLiteral("selection")
                     || key == QStringLiteral("selection_background")) m_selection = value;
        }
    }
    emit themeChanged();
}

void Backend::watchTheme() {
    const QStringList watched = m_themeWatcher.files() + m_themeWatcher.directories();
    if (!watched.isEmpty())
        m_themeWatcher.removePaths(watched);
    const QString themeDir = QDir::homePath()
        + QStringLiteral("/.local/state/omarchy/current/theme");
    const QString colors = themeDir + QStringLiteral("/colors.toml");
    if (QDir(themeDir).exists()) m_themeWatcher.addPath(themeDir);
    if (QFile::exists(colors)) m_themeWatcher.addPath(colors);
}
