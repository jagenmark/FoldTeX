#include "backend.h"

#include <QCryptographicHash>
#include <QClipboard>
#include <QColor>
#include <QDateTime>
#include <QDir>
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
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QTemporaryDir>
#include <QThread>
#include <QtMath>
#include <QUrl>
#include <QUuid>

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
    source = source.trimmed();
    if (source.startsWith(QStringLiteral("\\[")) && source.endsWith(QStringLiteral("\\]")))
        return source.mid(2, source.size() - 4).trimmed();
    if (source.startsWith(QStringLiteral("$$")) && source.endsWith(QStringLiteral("$$")))
        return source.mid(2, source.size() - 4).trimmed();
    if (source.startsWith(QLatin1Char('$')) && source.endsWith(QLatin1Char('$')))
        return source.mid(1, source.size() - 2).trimmed();
    return source;
}

bool looksLikeMath(const QString &source) {
    static const QRegularExpression mathPattern(QStringLiteral(R"(\\[A-Za-z]+|[_^=])"));
    return mathPattern.match(source).hasMatch();
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
        default: escaped += character;
        }
    }
    return escaped;
}

QString documentTex(const QString &title, const QVariantList &lines) {
    QString text = QStringLiteral(
        "\\documentclass[11pt]{article}\n"
        "\\usepackage[margin=1in]{geometry}\n"
        "\\usepackage{amsmath,amssymb,mathtools}\n"
        "\\usepackage[T1]{fontenc}\n"
        "\\usepackage[utf8]{inputenc}\n"
        "\\title{%1}\n"
        "\\date{}\n"
        "\\begin{document}\n"
        "\\maketitle\n").arg(escapedText(title));
    for (const QVariant &value : lines) {
        const QString source = value.toMap().value(QStringLiteral("source")).toString();
        if (source.trimmed().isEmpty()) {
            text += QLatin1Char('\n');
        } else if (looksLikeMath(source)) {
            text += QStringLiteral("\\[\n%1\n\\]\n").arg(bareMath(source));
        } else {
            text += escapedText(source) + QStringLiteral("\\par\n");
        }
    }
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
    QVariantList normalizedLines;
    const QVariantList storedLines = result.value(QStringLiteral("lines")).toList();
    for (const QVariant &storedLine : storedLines) {
        QVariantMap line = storedLine.toMap();
        QString source = line.value(QStringLiteral("source")).toString();
        source.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        source.replace(QLatin1Char('\r'), QLatin1Char('\n'));
        const QStringList parts = source.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
        for (QString part : parts) {
            if (part.trimmed() == QStringLiteral("\\linebreak")
                || part.trimmed() == QStringLiteral("\\newline"))
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
            normalizedLines.append(normalizedLine);
        }
    }
    result.insert(QStringLiteral("format"), QStringLiteral("foldtex-2"));
    result.insert(QStringLiteral("lines"), normalizedLines);
    if (!result.contains(QStringLiteral("course")))
        result.insert(QStringLiteral("course"), QString());
    if (!result.contains(QStringLiteral("lecture")))
        result.insert(QStringLiteral("lecture"), QString());
    if (!result.contains(QStringLiteral("lectureDate")))
        result.insert(QStringLiteral("lectureDate"), QString());
    if (!result.contains(QStringLiteral("sourcePdf")))
        result.insert(QStringLiteral("sourcePdf"), QString());
    return result;
}

bool writeDocument(const QString &path, const QVariantMap &document) {
    QVariantMap stored = normalizedDocument(document);
    stored.insert(QStringLiteral("format"), QStringLiteral("foldtex-2"));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument::fromVariant(stored).toJson(QJsonDocument::Indented));
    return file.commit();
}
}

Backend::Backend(QObject *parent) : QObject(parent) {
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
                   const QString &color, int pixelSize) {
                const QVariantMap result = render(source, color, pixelSize);
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
                          const QString &color, int pixelSize) {
    emit renderRequested(requestId, source, color, pixelSize);
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

QVariantMap Backend::render(const QString &source, const QString &color, int pixelSize) {
    if (source.trimmed().isEmpty())
        return {{QStringLiteral("url"), QString()}, {QStringLiteral("error"), QString()}};

    const QByteArray key = QCryptographicHash::hash(
        (QStringLiteral("v5\n") + source + QLatin1Char('\n') + color + QLatin1Char('\n')
         + QString::number(pixelSize)).toUtf8(),
        QCryptographicHash::Sha256).toHex();
    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/renders/") + QString::fromLatin1(key);
    QDir().mkpath(cacheRoot);
    const QString texPath = cacheRoot + QStringLiteral("/line.tex");
    const QString dviPath = cacheRoot + QStringLiteral("/line.dvi");
    const QString svgPath = cacheRoot + QStringLiteral("/line.svg");
    const QString errorPath = cacheRoot + QStringLiteral("/line.error");
    if (QFile::exists(svgPath))
        return {{QStringLiteral("url"), QUrl::fromLocalFile(svgPath).toString()},
                {QStringLiteral("error"), QString()}};
    QFile cachedError(errorPath);
    if (cachedError.open(QIODevice::ReadOnly | QIODevice::Text))
        return {{QStringLiteral("error"), QString::fromUtf8(cachedError.readAll())}};

    QString hex = color;
    hex.remove(QLatin1Char('#'));
    if (hex.size() != 6)
        hex = QStringLiteral("eeeeee");

    QSaveFile texFile(texPath);
    if (!texFile.open(QIODevice::WriteOnly | QIODevice::Text))
        return {{QStringLiteral("error"), QStringLiteral("Could not create render file")}};
    QTextStream out(&texFile);
    // Computer Modern has a smaller visible glyph box than the editor font at
    // the same nominal size. Match their measured cap heights, not their labels.
    const double pointSize = qBound(10, pixelSize, 40) * (21.0 / 18.0);
    const double lineSize = pointSize * 1.2;
    out << "\\documentclass[preview,border=0pt]{standalone}\n"
           "\\usepackage{amsmath,amssymb,mathtools,xcolor}\n"
           "\\definecolor{foldtext}{HTML}{" << hex << "}\n"
           "\\begin{document}\n"
           "\\fontsize{" << QString::number(pointSize, 'f', 2) << "pt}{"
        << QString::number(lineSize, 'f', 2) << "pt}\\selectfont\n"
           "\\color{foldtext}\\(\\displaystyle " << bareMath(source) << "\\)\n"
           "\\end{document}\n";
    if (!texFile.commit())
        return {{QStringLiteral("error"), QStringLiteral("Could not write render file")}};

    QProcess latex;
    latex.setWorkingDirectory(cacheRoot);
    latex.setProcessChannelMode(QProcess::MergedChannels);
    latex.start(QStringLiteral("latex"), {
        QStringLiteral("-interaction=nonstopmode"), QStringLiteral("-halt-on-error"),
        QStringLiteral("-no-shell-escape"), QStringLiteral("-output-directory=" ) + cacheRoot,
        texPath
    });
    if (!latex.waitForFinished(10000)) {
        latex.kill();
        return {{QStringLiteral("error"), firstLatexError(latex.readAll())}};
    }
    if (latex.exitStatus() != QProcess::NormalExit)
        return {{QStringLiteral("error"), firstLatexError(latex.readAll())}};
    if (latex.exitCode() != 0) {
        const QString error = firstLatexError(latex.readAll());
        writeTextFile(errorPath, error);
        return {{QStringLiteral("error"), error}};
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

    return {{QStringLiteral("url"), QUrl::fromLocalFile(svgPath).toString()},
            {QStringLiteral("error"), QString()}};
}

QVariantMap Backend::loadDocument(const QString &urlOrPath) {
    QFile file(localPath(urlOrPath));
    if (!file.open(QIODevice::ReadOnly))
        return {{QStringLiteral("error"), file.errorString()}};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
        return {{QStringLiteral("error"), QStringLiteral("Not a FoldTeX document")}};
    return normalizedDocument(document.object().toVariantMap());
}

bool Backend::saveDocumentData(const QString &urlOrPath, const QVariantMap &document) {
    return writeDocument(localPath(urlOrPath), document);
}

bool Backend::saveDocument(const QString &urlOrPath, const QString &title,
                           const QVariantList &lines) {
    return saveDocumentData(urlOrPath, {
        {QStringLiteral("title"), title},
        {QStringLiteral("lines"), lines}
    });
}

QVariantMap Backend::loadRecovery() {
    return loadDocument(recoveryPath());
}

void Backend::saveRecovery(const QString &title, const QVariantList &lines) {
    saveDocument(recoveryPath(), title, lines);
}

void Backend::saveRecoveryData(const QVariantMap &document) {
    writeDocument(recoveryPath(), document);
}

bool Backend::saveSnapshot(const QVariantMap &document) {
    const QString state = QStandardPaths::writableLocation(QStandardPaths::StateLocation);
    const QString directory = state + QStringLiteral("/snapshots");
    QDir().mkpath(directory);
    const QString stamp = QDateTime::currentDateTimeUtc().toString(
        QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    const QString path = directory + QLatin1Char('/') + stamp + QStringLiteral(".foldtex");
    if (!writeDocument(path, document))
        return false;

    QDir snapshots(directory);
    const QFileInfoList files = snapshots.entryInfoList(
        {QStringLiteral("*.foldtex")}, QDir::Files, QDir::Time);
    for (int i = 30; i < files.size(); ++i)
        QFile::remove(files.at(i).absoluteFilePath());
    return true;
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
    if (documentUrl.isEmpty())
        return {{QStringLiteral("error"), QStringLiteral("Save the note before drawing a figure")}};
    const QFileInfo documentInfo(localPath(documentUrl));
    const QString assetDirectory = documentInfo.absolutePath() + QLatin1Char('/')
        + documentInfo.completeBaseName() + QStringLiteral(".assets");
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
    if (!writeTextFile(localPath(urlOrPath), documentTex(title, lines)))
        return {{QStringLiteral("error"), QStringLiteral("Could not write the TeX file")}};
    return {{QStringLiteral("error"), QString()}};
}

QVariantMap Backend::exportPdf(const QString &urlOrPath, const QString &title,
                               const QVariantList &lines) {
    QTemporaryDir temp;
    if (!temp.isValid())
        return {{QStringLiteral("error"), QStringLiteral("Could not make a temporary folder")}};
    const QString texPath = temp.path() + QStringLiteral("/foldtex-export.tex");
    if (!writeTextFile(texPath, documentTex(title, lines)))
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
