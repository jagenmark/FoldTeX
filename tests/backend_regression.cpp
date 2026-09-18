#include "backend.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QPainter>
#include <QPdfWriter>
#include <QProcess>
#include <QStandardPaths>
#include <QSettings>
#include <QTimer>
#include <QTemporaryDir>
#include <QUrl>
#include <cstdio>

int main(int argc, char **argv) {
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &message) { std::fprintf(stderr, "%s\n", qPrintable(message)); });
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("JagenmarkTests"));
    app.setApplicationName(QStringLiteral("foldtex-backend-regression"));
    QSettings().remove(QStringLiteral("library"));
    QSettings().remove(QStringLiteral("snippets/custom"));
    if (argc != 2)
        return 2;

    Backend backend;
    const QVariantMap document = backend.loadDocument(QString::fromLocal8Bit(argv[1]));
    const QVariantList lines = document.value(QStringLiteral("lines")).toList();
    const QStringList expected{
        QStringLiteral("\\[\n5 \\cdot 5 = 32 \\sqrt[3]{5}\n\\]"),
        QString(),
        QStringLiteral("This is a plain text note."),
        QString(),
        QStringLiteral("\\text XD")
    };
    QStringList actual;
    for (const QVariant &line : lines)
        actual.append(line.toMap().value(QStringLiteral("source")).toString());

    if (actual != expected) {
        qCritical() << "FAIL: loaded rows" << actual << "expected" << expected;
        return 1;
    }
    const QVariantMap render = backend.render(actual.first(), QStringLiteral("#ffffff"), 18);
    if (!render.value(QStringLiteral("error")).toString().isEmpty()) {
        qCritical() << "FAIL: formula still errors" << render;
        return 1;
    }
    const QString analysisSource = QStringLiteral(
        R"(\begin{gathered}
x \in A,\; y \notin A,\; A \subseteq B,\; A \nsubseteq C \\
A \supseteq B,\; A \nsupseteq C,\; A \cup B,\; A \cap B,\; A \setminus B \\
A^{\complement},\; A \triangle B,\; \emptyset,\; \mathbb{N},\mathbb{Z},\mathbb{Q},\mathbb{R},\mathbb{C} \\
\left\{x \in \mathbb{R} \middle\vert x>0\right\},\; \left\lvert x-a \right\rvert \\
\forall \varepsilon>0\;\exists \delta>0 \colon |x-a|<\delta \implies |f(x)-f(a)|<\varepsilon \\
\left\lVert x \right\rVert,\; \sup A,\; \inf A,\;
\sum_{k=0}^{n}\frac{f^{(k)}(a)}{k!}(x-a)^k
\end{gathered})");
    const QVariantMap analysisRender = backend.render(analysisSource, QStringLiteral("#ffffff"), 18);
    if (!analysisRender.value(QStringLiteral("error")).toString().isEmpty()) {
        qCritical() << "FAIL: Analysis 1 notation does not render" << analysisRender;
        return 1;
    }
    const QString laterCourseSource = QStringLiteral(
        R"(\begin{gathered}
P \land Q,\; P \lor Q,\; \neg P,\; a\mid b,\; a\nmid c,\;
a\equiv b\pmod n,\; \binom nk,\; \deg(v) \\
\det(A),\; A^{\mathsf T},\; A^{-1},\; \operatorname{span}\{v_1,v_2\},\;
\dim(V),\; \operatorname{rank}(A),\; \ker(T),\; \operatorname{im}(T) \\
\langle u,v\rangle,\; Av=\lambda v,\; \nabla f,\; D_u f,\; J_f(x),\; H_f(x) \\
\iint_D f\,dA,\; \iiint_E f\,dV,\; \int_C f\,ds,\;
\nabla\cdot F,\; \nabla\times F,\; \Delta f
\end{gathered})");
    const QVariantMap laterCourseRender = backend.render(
        laterCourseSource, QStringLiteral("#ffffff"), 18);
    if (!laterCourseRender.value(QStringLiteral("error")).toString().isEmpty()) {
        qCritical() << "FAIL: later-course notation does not render" << laterCourseRender;
        return 1;
    }
    const QString badSource = QStringLiteral("\\foldtexRegressionMissingCommand{x}");
    const QString color = QStringLiteral("#ffffff");
    const int size = 18;
    const QByteArray key = QCryptographicHash::hash(
        (QStringLiteral("v7\n") + badSource + QLatin1Char('\n') + color + QLatin1Char('\n')
         + QString::number(size)).toUtf8(),
        QCryptographicHash::Sha256).toHex();
    const QString badCache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/renders/") + QString::fromLatin1(key);
    QDir(badCache).removeRecursively();
    const QVariantMap firstBadRender = backend.render(badSource, color, size);
    const QVariantMap secondBadRender = backend.render(badSource, color, size);
    if (firstBadRender.value(QStringLiteral("error")).toString().isEmpty()
        || secondBadRender.value(QStringLiteral("error"))
               != firstBadRender.value(QStringLiteral("error"))
        || !QFile::exists(badCache + QStringLiteral("/line.error"))) {
        qCritical() << "FAIL: LaTeX errors were not cached";
        return 1;
    }
    const QString asyncSource = QStringLiteral("\\foldtexAsyncMissingCommand{x}");
    const QByteArray asyncKey = QCryptographicHash::hash(
        (QStringLiteral("v7\n") + asyncSource + QLatin1Char('\n') + color + QLatin1Char('\n')
         + QString::number(size)).toUtf8(),
        QCryptographicHash::Sha256).toHex();
    const QString asyncCache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/renders/") + QString::fromLatin1(asyncKey);
    QDir(asyncCache).removeRecursively();
    QEventLoop renderLoop;
    QVariantMap asyncResult;
    int finishedRequest = -1;
    QObject::connect(&backend, &Backend::renderFinished, &renderLoop,
                     [&](int requestId, const QVariantMap &result) {
        finishedRequest = requestId;
        asyncResult = result;
        renderLoop.quit();
    });
    QElapsedTimer asyncCallTimer;
    asyncCallTimer.start();
    backend.renderAsync(77, asyncSource, color, size);
    const qint64 asyncCallMs = asyncCallTimer.elapsed();
    QTimer::singleShot(5000, &renderLoop, &QEventLoop::quit);
    renderLoop.exec();
    if (asyncCallMs >= 20 || finishedRequest != 77
        || asyncResult.value(QStringLiteral("error")).toString().isEmpty()) {
        qCritical() << "FAIL: background render" << asyncCallMs
                    << finishedRequest << asyncResult;
        return 1;
    }

    QTemporaryDir course;
    const QString firstNote = course.path() + QStringLiteral("/lecture-1.foldtex");
    const QString secondNote = course.path() + QStringLiteral("/lecture-2.foldtex");
    const QVariantMap richDocument{
        {QStringLiteral("title"), QStringLiteral("Limits")},
        {QStringLiteral("course"), QStringLiteral("Calculus I")},
        {QStringLiteral("lecture"), QStringLiteral("Lecture 1")},
        {QStringLiteral("lectureDate"), QStringLiteral("2026-08-29")},
        {QStringLiteral("lines"), QVariantList{
             QVariantMap{{QStringLiteral("source"), QStringLiteral("epsilon proof")},
                         {QStringLiteral("kind"), QStringLiteral("theorem")},
                         {QStringLiteral("label"), QStringLiteral("Theorem")}}
         }}
    };
    backend.saveDocumentData(firstNote, richDocument);
    backend.saveDocumentData(secondNote, {
        {QStringLiteral("title"), QStringLiteral("Derivatives")},
        {QStringLiteral("course"), QStringLiteral("Calculus I")},
        {QStringLiteral("lectureDate"), QStringLiteral("2026-08-28")},
        {QStringLiteral("lines"), QVariantList{
             QVariantMap{{QStringLiteral("source"), QStringLiteral("another epsilon note")}}
         }}
    });
    backend.rememberNoteFolder(firstNote);
    const QVariantList libraryByTitle = backend.noteLibrary(QStringLiteral("epsilon"),
                                                            QStringLiteral("title"));
    const QVariantList libraryByDate = backend.noteLibrary(QString(), QStringLiteral("date"));
    QTemporaryDir archiveCourse;
    backend.saveDocumentData(archiveCourse.path() + QStringLiteral("/archived.foldtex"), {
        {QStringLiteral("title"), QStringLiteral("Archived algebra")},
        {QStringLiteral("course"), QStringLiteral("Algebra")},
        {QStringLiteral("noteKind"), QStringLiteral("problem-solving")},
        {QStringLiteral("problemSet"), QStringLiteral("Problem set 7")},
        {QStringLiteral("lines"), QVariantList{
             QVariantMap{{QStringLiteral("source"), QStringLiteral("unique archive marker")}}
        }}
    });
    backend.addNoteFolder(archiveCourse.path());
    const QVariantList addedFolderMatches = backend.noteLibrary(
        QStringLiteral("unique archive marker"), QStringLiteral("course"));
    const QVariantList multiWordMatches = backend.noteLibrary(
        QStringLiteral("another epsilon"), QStringLiteral("updated"),
        QStringLiteral("Calculus I"), QStringLiteral("lecture"));
    const QVariantList problemMatches = backend.noteLibrary(
        QString(), QStringLiteral("updated"), QStringLiteral("Algebra"),
        QStringLiteral("problem-solving"));
    backend.setNotePinned(firstNote, true);
    backend.rememberOpenedNote(firstNote);
    const QVariantList pinnedNotes = backend.noteLibrary(QString(), QStringLiteral("title"));
    const QVariantMap libraryState = backend.noteLibraryState();
    const QVariantMap richReloaded = backend.loadDocument(firstNote);
    const QVariantList courseMatches = backend.searchCourse(firstNote, QStringLiteral("epsilon"));
    if (richReloaded.value(QStringLiteral("format")).toString() != QStringLiteral("foldtex-3")
        || richReloaded.value(QStringLiteral("course")).toString() != QStringLiteral("Calculus I")
        || richReloaded.value(QStringLiteral("noteKind")).toString() != QStringLiteral("lecture")
        || richReloaded.value(QStringLiteral("lines")).toList().first().toMap()
               .value(QStringLiteral("kind")).toString() != QStringLiteral("theorem")
        || courseMatches.size() != 2 || libraryByTitle.size() != 2
        || libraryByTitle.first().toMap().value(QStringLiteral("title")).toString()
               != QStringLiteral("Derivatives")
        || libraryByDate.first().toMap().value(QStringLiteral("title")).toString()
               != QStringLiteral("Limits")
        || addedFolderMatches.size() != 1
        || addedFolderMatches.first().toMap().value(QStringLiteral("course")).toString()
               != QStringLiteral("Algebra")
        || addedFolderMatches.first().toMap().value(QStringLiteral("noteKind")).toString()
               != QStringLiteral("problem-solving")
        || addedFolderMatches.first().toMap().value(QStringLiteral("problemSet")).toString()
               != QStringLiteral("Problem set 7")
        || multiWordMatches.size() != 1
        || multiWordMatches.first().toMap().value(QStringLiteral("title")).toString()
               != QStringLiteral("Derivatives")
        || multiWordMatches.first().toMap().value(QStringLiteral("matchPreview")).toString()
               != QStringLiteral("another epsilon note")
        || problemMatches.size() != 1
        || pinnedNotes.first().toMap().value(QStringLiteral("path")).toString() != firstNote
        || !pinnedNotes.first().toMap().value(QStringLiteral("pinned")).toBool()
        || libraryState.value(QStringLiteral("courses")).toStringList().size() != 2
        || libraryState.value(QStringLiteral("lastNote")).toMap()
               .value(QStringLiteral("path")).toString() != firstNote) {
        qCritical() << "FAIL: rich document, course search, or note library"
                    << richReloaded << courseMatches << libraryByTitle << libraryByDate
                    << multiWordMatches << problemMatches << pinnedNotes << libraryState;
        return 1;
    }
    if (backend.latexHint(QStringLiteral("\\frac{x}{y"), QStringLiteral("error"))
            != QStringLiteral("Add a closing brace }")
        || backend.latexHint(QStringLiteral("\\unknown{x}"),
                             QStringLiteral("Undefined control sequence."))
            != QStringLiteral("Check the LaTeX command name")
        || backend.latexHint(QStringLiteral("x"), QStringLiteral("Missing $ inserted."))
            != QStringLiteral("Could not render this row")) {
        qCritical() << "FAIL: LaTeX hints are not specific and safe";
        return 1;
    }
    const QString modeExportPath = course.path() + QStringLiteral("/row-modes.tex");
    backend.exportTex(modeExportPath, QStringLiteral("Row modes"), {
        QVariantMap{{QStringLiteral("source"), QStringLiteral("x + y")},
                    {QStringLiteral("mode"), QStringLiteral("math")}},
        QVariantMap{{QStringLiteral("source"), QStringLiteral("Let x = 2")},
                    {QStringLiteral("mode"), QStringLiteral("text")}}
    });
    QFile modeExport(modeExportPath);
    if (!modeExport.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "FAIL: row-mode TeX export was not written";
        return 1;
    }
    const QString modeTex = QString::fromUtf8(modeExport.readAll());
    if (!modeTex.contains(QStringLiteral("\\[\nx + y\n\\]"))
        || !modeTex.contains(QStringLiteral("Let x = 2\\par"))) {
        qCritical() << "FAIL: explicit row modes were not honored by export" << modeTex;
        return 1;
    }
    const QString sourcePdf = course.path() + QStringLiteral("/source.pdf");
    {
        QPdfWriter writer(sourcePdf);
        QPainter painter(&writer);
        painter.drawText(QPoint(100, 100), QStringLiteral("FoldTeX slide"));
        painter.end();
    }
    const QVariantMap firstPdfImport = backend.importPdf(firstNote, sourcePdf);
    const QVariantMap secondPdfImport = backend.importPdf(firstNote, sourcePdf);
    const QString copiedPdf = firstPdfImport.value(QStringLiteral("path")).toString();
    const QVariantMap figureAsset = backend.newFigureAsset(firstNote);
    const QString figurePath = figureAsset.value(QStringLiteral("path")).toString();
    const bool figureSaved = backend.saveFigure(
        figurePath,
        QStringLiteral("[{\"type\":\"arrow\",\"x1\":10,\"y1\":10,\"x2\":90,\"y2\":60}]"),
        120, 80, QStringLiteral("#101010"), QStringLiteral("#eeeeee"),
        QStringLiteral("monospace"));
    const QVariantMap temporaryFigureAsset = backend.newFigureAsset(QString());
    const QString temporaryFigurePath = temporaryFigureAsset
                                            .value(QStringLiteral("path")).toString();
    const bool temporaryFigureSaved = backend.saveFigure(
        temporaryFigurePath,
        QStringLiteral("[{\"type\":\"box\",\"x1\":5,\"y1\":5,\"x2\":50,\"y2\":40}]"),
        60, 50, QStringLiteral("#101010"), QStringLiteral("#eeeeee"),
        QStringLiteral("monospace"));
    const QVariantMap adoptedFigure = backend.adoptAsset(firstNote, temporaryFigurePath);
    const QString adoptedFigurePath = adoptedFigure.value(QStringLiteral("path")).toString();
    const bool temporaryFigureRemoved = backend.removeTemporaryAsset(temporaryFigurePath);
    if (!firstPdfImport.value(QStringLiteral("error")).toString().isEmpty()
        || copiedPdf.isEmpty() || !QFile::exists(copiedPdf)
        || !copiedPdf.contains(QStringLiteral("/.foldtex-assets/"))
        || secondPdfImport.value(QStringLiteral("path")) != copiedPdf
        || !figureAsset.value(QStringLiteral("path")).toString()
                .contains(QStringLiteral("lecture-1.assets/"))
        || !figureSaved || !QFile::exists(figurePath)
        || !temporaryFigureSaved || temporaryFigurePath.isEmpty()
        || !temporaryFigurePath.contains(QStringLiteral("/figures/"))
        || adoptedFigurePath.isEmpty() || !QFile::exists(adoptedFigurePath)
        || !adoptedFigurePath.contains(QStringLiteral("lecture-1.assets/"))
        || !temporaryFigureRemoved || QFile::exists(temporaryFigurePath)
        || backend.fileUrl(copiedPdf) != QUrl::fromLocalFile(copiedPdf).toString()) {
        qCritical() << "FAIL: copied PDF or figure asset" << firstPdfImport
                    << secondPdfImport << figureAsset;
        return 1;
    }

    QTemporaryDir portableRoot;
    const QString portableFolder = portableRoot.path() + QStringLiteral("/course-note");
    const QString portableAssets = portableFolder + QStringLiteral("/note.assets");
    const QString portableSlides = portableFolder + QStringLiteral("/.foldtex-assets");
    QDir().mkpath(portableAssets);
    QDir().mkpath(portableSlides);
    const QString portableImage = portableAssets + QStringLiteral("/figure.png");
    const QString portablePdf = portableSlides + QStringLiteral("/slides.pdf");
    QFile::copy(figurePath, portableImage);
    QFile::copy(sourcePdf, portablePdf);
    const QString portableNote = portableFolder + QStringLiteral("/note.foldtex");
    if (!backend.saveDocumentData(portableNote, {
            {QStringLiteral("title"), QStringLiteral("Portable note")},
            {QStringLiteral("sourcePdf"), portablePdf},
            {QStringLiteral("lines"), QVariantList{
                 QVariantMap{{QStringLiteral("kind"), QStringLiteral("image")},
                             {QStringLiteral("source"), QStringLiteral("A figure")},
                             {QStringLiteral("asset"), portableImage}}
            }}
        })) {
        qCritical() << "FAIL: portable note was not saved";
        return 1;
    }
    QFile portableJson(portableNote);
    if (!portableJson.open(QIODevice::ReadOnly)) {
        qCritical() << "FAIL: portable note could not be read";
        return 1;
    }
    const QByteArray storedPortable = portableJson.readAll();
    if (storedPortable.contains(portableRoot.path().toUtf8())
        || !storedPortable.contains("note.assets/figure.png")
        || !storedPortable.contains(".foldtex-assets/slides.pdf")) {
        qCritical() << "FAIL: note assets were not stored as portable paths"
                    << storedPortable;
        return 1;
    }
    const QString movedFolder = portableRoot.path() + QStringLiteral("/moved-note");
    if (!QDir().rename(portableFolder, movedFolder)) {
        qCritical() << "FAIL: portable note folder could not be moved";
        return 1;
    }
    const QVariantMap movedDocument = backend.loadDocument(
        movedFolder + QStringLiteral("/note.foldtex"));
    const QString movedImage = movedDocument.value(QStringLiteral("lines")).toList()
                                   .first().toMap().value(QStringLiteral("asset")).toString();
    if (!QFile::exists(movedImage)
        || !QFile::exists(movedDocument.value(QStringLiteral("sourcePdf")).toString())) {
        qCritical() << "FAIL: moved note did not resolve its image and PDF" << movedDocument;
        return 1;
    }

    const QString richExportPath = course.path() + QStringLiteral("/rich-export.tex");
    const QVariantMap richExportDocument{
        {QStringLiteral("title"), QStringLiteral("Vector calculus")},
        {QStringLiteral("course"), QStringLiteral("Analysis 2")},
        {QStringLiteral("lecture"), QStringLiteral("Lecture 8")},
        {QStringLiteral("lectureDate"), QStringLiteral("2026-08-31")},
        {QStringLiteral("lines"), QVariantList{
             QVariantMap{{QStringLiteral("source"), QStringLiteral("Vector fields")},
                         {QStringLiteral("kind"), QStringLiteral("heading")}},
             QVariantMap{{QStringLiteral("source"), QStringLiteral("First point")},
                         {QStringLiteral("kind"), QStringLiteral("bullet")}},
             QVariantMap{{QStringLiteral("source"), QStringLiteral("Show that x^2 = 1")},
                         {QStringLiteral("kind"), QStringLiteral("exercise")},
                         {QStringLiteral("mode"), QStringLiteral("text")}},
             QVariantMap{{QStringLiteral("source"), QStringLiteral("A figure")},
                         {QStringLiteral("kind"), QStringLiteral("image")},
                         {QStringLiteral("asset"), figurePath}}
        }}
    };
    const QVariantMap richExportResult = backend.exportDocumentTex(richExportPath,
                                                                    richExportDocument);
    QFile richExport(richExportPath);
    if (!richExport.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "FAIL: rich TeX export could not be read";
        return 1;
    }
    const QString richTex = QString::fromUtf8(richExport.readAll());
    if (!richExportResult.value(QStringLiteral("error")).toString().isEmpty()
        || !richTex.contains(QStringLiteral("\\author{Analysis 2}"))
        || !richTex.contains(QStringLiteral("\\section*{Vector fields}"))
        || !richTex.contains(QStringLiteral("\\begin{itemize}"))
        || !richTex.contains(QStringLiteral("\\textbf{Exercise. }"))
        || !richTex.contains(QStringLiteral("\\includegraphics"))
        || !richTex.contains(QStringLiteral("\\caption{A figure}"))) {
        qCritical() << "FAIL: rich export omitted note data or structure" << richTex;
        return 1;
    }
    const QString richPdfPath = course.path() + QStringLiteral("/rich-export.pdf");
    const QVariantMap richPdfResult = backend.exportDocumentPdf(richPdfPath,
                                                                 richExportDocument);
    if (!richPdfResult.value(QStringLiteral("error")).toString().isEmpty()
        || !QFileInfo(richPdfPath).isFile() || QFileInfo(richPdfPath).size() < 1000) {
        qCritical() << "FAIL: rich PDF export" << richPdfResult;
        return 1;
    }

    const QString oddImageDirectory = course.path() + QStringLiteral("/figures #%}");
    QDir().mkpath(oddImageDirectory);
    const QString svgPath = oddImageDirectory + QStringLiteral("/field #%}.svg");
    QFile svg(svgPath);
    if (!svg.open(QIODevice::WriteOnly | QIODevice::Text)
        || svg.write("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"40\" height=\"30\">"
                     "<rect width=\"40\" height=\"30\" fill=\"#2277aa\"/></svg>") < 0) {
        qCritical() << "FAIL: could not prepare SVG export regression";
        return 1;
    }
    svg.close();
    QVariantMap svgDocument = richExportDocument;
    svgDocument.insert(QStringLiteral("lines"), QVariantList{
        QVariantMap{{QStringLiteral("source"), QStringLiteral("SVG figure")},
                    {QStringLiteral("kind"), QStringLiteral("image")},
                    {QStringLiteral("asset"), svgPath}}
    });
    const QString svgTexPath = course.path() + QStringLiteral("/svg-export.tex");
    const QString svgPdfPath = course.path() + QStringLiteral("/svg-export.pdf");
    const QVariantMap svgTexResult = backend.exportDocumentTex(svgTexPath, svgDocument);
    const QVariantMap svgPdfResult = backend.exportDocumentPdf(svgPdfPath, svgDocument);
    QFile svgTex(svgTexPath);
    if (!svgTex.open(QIODevice::ReadOnly | QIODevice::Text))
        return 1;
    const QString svgTexText = QString::fromUtf8(svgTex.readAll());
    const int assetStart = svgTexText.indexOf(QStringLiteral(".foldtex-assets-"));
    const int assetEnd = svgTexText.indexOf(QStringLiteral("/image-1.png"), assetStart);
    const QString firstAsset = assetStart >= 0 && assetEnd > assetStart
        ? svgTexText.mid(assetStart, assetEnd - assetStart + 12) : QString();
    const QString secondTexPath = course.path() + QStringLiteral("/svg-export-2.tex");
    const QVariantMap secondTexResult = backend.exportDocumentTex(secondTexPath, svgDocument);
    QFile secondTex(secondTexPath);
    if (!secondTex.open(QIODevice::ReadOnly | QIODevice::Text))
        return 1;
    const QString secondTexText = QString::fromUtf8(secondTex.readAll());
    if (!svgTexResult.value(QStringLiteral("error")).toString().isEmpty()
        || !svgPdfResult.value(QStringLiteral("error")).toString().isEmpty()
        || !secondTexResult.value(QStringLiteral("error")).toString().isEmpty()
        || firstAsset.isEmpty()
        || !QFileInfo::exists(course.path() + QLatin1Char('/') + firstAsset)
        || secondTexText.contains(firstAsset)
        || svgTexText.contains(svgPath)
        || !QUrl(backend.fileUrl(svgPath)).isLocalFile()
        || QFileInfo(svgPdfPath).size() < 1000) {
        qCritical() << "FAIL: SVG or special-character image export"
                    << svgTexResult << svgPdfResult << svgTexText;
        return 1;
    }
    const QStringList assetsBeforeFailure = QDir(course.path()).entryList(
        {QStringLiteral(".foldtex-assets-*")}, QDir::Dirs | QDir::NoDotAndDotDot);
    QVariantMap badSvgDocument = svgDocument;
    badSvgDocument.insert(QStringLiteral("lines"), QVariantList{
        QVariantMap{{QStringLiteral("kind"), QStringLiteral("image")},
                    {QStringLiteral("asset"), svgPath}},
        QVariantMap{{QStringLiteral("kind"), QStringLiteral("image")},
                    {QStringLiteral("asset"), oddImageDirectory + QStringLiteral("/missing.svg")}}
    });
    const QString failedTexPath = course.path() + QStringLiteral("/failed-images.tex");
    const QVariantMap failedTexResult = backend.exportDocumentTex(failedTexPath, badSvgDocument);
    const QStringList assetsAfterFailure = QDir(course.path()).entryList(
        {QStringLiteral(".foldtex-assets-*")}, QDir::Dirs | QDir::NoDotAndDotDot);
    if (failedTexResult.value(QStringLiteral("error")).toString().isEmpty()
        || QFileInfo::exists(failedTexPath)
        || assetsBeforeFailure != assetsAfterFailure) {
        qCritical() << "FAIL: failed image export left partial output"
                    << failedTexResult << assetsBeforeFailure << assetsAfterFailure;
        return 1;
    }

    for (const QVariant &entryValue : backend.recoveryEntries()) {
        const QVariantMap entry = entryValue.toMap();
        const QString title = entry.value(QStringLiteral("title")).toString();
        if (title == QStringLiteral("Untitled recovery one")
            || title == QStringLiteral("Untitled recovery two"))
            backend.removeRecovery(entry.value(QStringLiteral("path")).toString());
    }
    const QString firstRecoveryId = backend.newRecoveryId();
    const QString secondRecoveryId = backend.newRecoveryId();
    backend.saveRecoveryData({
        {QStringLiteral("title"), QStringLiteral("Untitled recovery one")},
        {QStringLiteral("recoveryId"), firstRecoveryId},
        {QStringLiteral("lines"), QVariantList{QVariantMap{{QStringLiteral("source"),
                                                             QStringLiteral("one")}}}}
    });
    backend.saveRecoveryData({
        {QStringLiteral("title"), QStringLiteral("Untitled recovery two")},
        {QStringLiteral("recoveryId"), secondRecoveryId},
        {QStringLiteral("lines"), QVariantList{QVariantMap{{QStringLiteral("source"),
                                                             QStringLiteral("two")}}}}
    });
    int untitledRecoveryCount = 0;
    for (const QVariant &entryValue : backend.recoveryEntries()) {
        const QString title = entryValue.toMap().value(QStringLiteral("title")).toString();
        if (title == QStringLiteral("Untitled recovery one")
            || title == QStringLiteral("Untitled recovery two"))
            ++untitledRecoveryCount;
    }
    if (firstRecoveryId.isEmpty() || firstRecoveryId == secondRecoveryId
        || untitledRecoveryCount != 2) {
        qCritical() << "FAIL: separate untitled notes share one recovery file";
        return 1;
    }
    for (const QVariant &entryValue : backend.recoveryEntries()) {
        const QVariantMap entry = entryValue.toMap();
        const QString title = entry.value(QStringLiteral("title")).toString();
        if (title == QStringLiteral("Untitled recovery one")
            || title == QStringLiteral("Untitled recovery two"))
            backend.removeRecovery(entry.value(QStringLiteral("path")).toString());
    }

    QVariantMap recoveryDocument = richExportDocument;
    recoveryDocument.insert(QStringLiteral("title"), QStringLiteral("Recovery regression"));
    recoveryDocument.insert(QStringLiteral("documentPath"), firstNote);
    recoveryDocument.insert(QStringLiteral("recoveryDirty"), true);
    if (!backend.saveRecoveryData(recoveryDocument)) {
        qCritical() << "FAIL: per-note recovery was not saved";
        return 1;
    }
    QString recoveryEntryPath;
    for (const QVariant &entryValue : backend.recoveryEntries()) {
        const QVariantMap entry = entryValue.toMap();
        if (entry.value(QStringLiteral("title")).toString()
            == QStringLiteral("Recovery regression")) {
            recoveryEntryPath = entry.value(QStringLiteral("path")).toString();
            break;
        }
    }
    const QVariantMap environment = backend.environmentStatus();
    if (recoveryEntryPath.isEmpty()
        || backend.loadRecovery().value(QStringLiteral("title")).toString()
               != QStringLiteral("Recovery regression")
        || !environment.contains(QStringLiteral("ready"))
        || !environment.contains(QStringLiteral("missing"))
        || !backend.removeRecovery(recoveryEntryPath)
        || QFile::exists(recoveryEntryPath)) {
        qCritical() << "FAIL: recovery list, restore, removal, or environment check"
                    << backend.recoveryEntries() << environment;
        return 1;
    }
    const QString checkedPath = course.path() + QStringLiteral("/checked-save.foldtex");
    backend.saveDocumentData(checkedPath, richExportDocument);
    const QString checkedRevision = backend.fileRevision(checkedPath);
    const QVariantMap conflictSave = backend.saveDocumentDataChecked(
        checkedPath, richDocument, QStringLiteral("wrong revision"));
    const QVariantMap acceptedSave = backend.saveDocumentDataChecked(
        checkedPath, richDocument, checkedRevision);
    if (!conflictSave.value(QStringLiteral("conflict")).toBool()
        || conflictSave.value(QStringLiteral("saved")).toBool()
        || !acceptedSave.value(QStringLiteral("saved")).toBool()) {
        qCritical() << "FAIL: checked save did not protect an outside edit"
                    << conflictSave << acceptedSave;
        return 1;
    }

    const QVariantList customSnippets{
        QVariantMap{{QStringLiteral("name"), QStringLiteral("Global basis")},
                    {QStringLiteral("trigger"), QStringLiteral("mybasis")},
                    {QStringLiteral("aliases"), QStringList{QStringLiteral("mb")}},
                    {QStringLiteral("template"), QStringLiteral("\\operatorname{span}\\{«v»\\}")},
                    {QStringLiteral("allCourses"), true},
                    {QStringLiteral("enabled"), true}},
        QVariantMap{{QStringLiteral("name"), QStringLiteral("Analysis direction")},
                    {QStringLiteral("trigger"), QStringLiteral("mydir")},
                    {QStringLiteral("aliases"), QStringList{QStringLiteral("md")}},
                    {QStringLiteral("template"), QStringLiteral("D_{«u»} «f»")},
                    {QStringLiteral("allCourses"), false},
                    {QStringLiteral("courses"), QStringList{QStringLiteral("Analys 2"),
                                                             QStringLiteral("Analysis 2")}},
                    {QStringLiteral("enabled"), true}}
    };
    const QVariantMap customSave = backend.replaceCustomSnippets(customSnippets);
    const QVariantMap globalResolve = backend.customSnippet(QStringLiteral("MB"),
                                                             QStringLiteral("Any course"));
    const QVariantMap scopedResolve = backend.customSnippet(QStringLiteral("mydir"),
                                                             QStringLiteral("analys 2"));
    const QVariantMap wrongCourseResolve = backend.customSnippet(QStringLiteral("mydir"),
                                                                  QStringLiteral("Analys 1"));
    if (!customSave.value(QStringLiteral("saved")).toBool()
        || !globalResolve.value(QStringLiteral("found")).toBool()
        || !scopedResolve.value(QStringLiteral("found")).toBool()
        || wrongCourseResolve.value(QStringLiteral("found")).toBool()
        || backend.customSnippets().size() != 2) {
        qCritical() << "FAIL: custom snippet scope, alias, or default-global handling"
                    << customSave << globalResolve << scopedResolve << wrongCourseResolve;
        return 1;
    }
    QVariantList overlapping = customSnippets;
    overlapping.append(QVariantMap{
        {QStringLiteral("trigger"), QStringLiteral("mb")},
        {QStringLiteral("template"), QStringLiteral("conflict")},
        {QStringLiteral("allCourses"), true},
        {QStringLiteral("enabled"), true}
    });
    if (backend.replaceCustomSnippets(overlapping).value(QStringLiteral("saved")).toBool()) {
        qCritical() << "FAIL: overlapping custom trigger and alias were accepted";
        return 1;
    }
    const QString snippetExport = course.path() + QStringLiteral("/custom.foldtex-snippets.json");
    const QVariantMap snippetExportResult = backend.exportCustomSnippets(snippetExport);
    backend.replaceCustomSnippets({});
    const QVariantMap snippetImportResult = backend.importCustomSnippets(snippetExport);
    if (!snippetExportResult.value(QStringLiteral("saved")).toBool()
        || !snippetImportResult.value(QStringLiteral("saved")).toBool()
        || !backend.customSnippet(QStringLiteral("mybasis"), QStringLiteral("Linear algebra"))
                .value(QStringLiteral("found")).toBool()) {
        qCritical() << "FAIL: custom snippet export or import"
                    << snippetExportResult << snippetImportResult;
        return 1;
    }
    backend.replaceCustomSnippets({});
    const QString blockMath = QStringLiteral(
        "\\begin{aligned}\n&\\text{Om } n \\text{ är ett heltal} \\\\\n"
        "&3 \\mid n, \\text{då är } 9 \\text{en delare till } (4n+3)^2\n\\end{aligned}");
    const QString blockText = QStringLiteral("\nFirst line\n\nSecond line\n");
    const QVariantList blockLines{
        QVariantMap{{QStringLiteral("source"), blockMath}, {QStringLiteral("mode"), "math"}},
        QVariantMap{{QStringLiteral("source"), blockText}, {QStringLiteral("mode"), "text"}},
        QVariantMap{{QStringLiteral("source"), QStringLiteral("\\newline")},
                    {QStringLiteral("mode"), "text"}}
    };
    const QVariantMap blockDocument{{QStringLiteral("title"), "Multiline blocks"},
                                   {QStringLiteral("lines"), blockLines}};
    QTemporaryDir blocks;
    const QString blockPath = blocks.filePath(QStringLiteral("blocks.foldtex"));
    const auto canonicalBlocks = backend.migrateRowModes(blockLines);
    auto hasBlocks = [&](const QVariantMap &loaded) {
        const QVariantList rows = loaded.value("lines").toList();
        return rows.size() == 3 && rows[0].toMap().value("source") == canonicalBlocks[0].toMap().value("source")
            && rows[1].toMap().value("source") == blockText
            && rows[2].toMap().value("source") == QStringLiteral("\\newline");
    };
    if (!backend.saveDocumentData(blockPath, blockDocument)
        || !hasBlocks(backend.loadDocument(blockPath))
        || !backend.saveRecoveryData(blockDocument)
        || !hasBlocks(backend.loadRecovery())
        || !backend.saveSnapshot(blockDocument)) {
        qCritical() << "FAIL: save or recovery split a multiline block";
        return 1;
    }
    bool foundBlockSnapshot = false;
    for (const QVariant &entry : backend.recoveryEntries()) {
        const QVariantMap item = entry.toMap();
        if (item.value("title").toString() == QStringLiteral("Multiline blocks")
            && item.value("entryKind").toString() == QStringLiteral("Snapshot"))
            foundBlockSnapshot = hasBlocks(backend.loadDocument(item.value("path").toString()));
    }
    if (!foundBlockSnapshot) {
        qCritical() << "FAIL: snapshot did not preserve multiline blocks";
        return 1;
    }
    const QString blockTexPath = blocks.filePath(QStringLiteral("blocks.tex"));
    const QVariantMap blockTex = backend.exportDocumentTex(blockTexPath, blockDocument);
    const QVariantMap blockPdf = backend.exportDocumentPdf(
        blocks.filePath(QStringLiteral("blocks.pdf")), blockDocument);
    QFile blockTexFile(blockTexPath);
    if (!blockTex.value("error").toString().isEmpty()
        || !blockPdf.value("error").toString().isEmpty()
        || !blockTexFile.open(QIODevice::ReadOnly)
        || !QString::fromUtf8(blockTexFile.readAll()).contains(
            QStringLiteral("First line\\strut\\\\\n\\strut\\\\\nSecond line"))) {
        qCritical() << "FAIL: multiline TeX/PDF export" << blockTex << blockPdf;
        return 1;
    }
    // Overleaf reports successful compilation but still flags empty justified
    // lines produced by \\\\ immediately before \\par or display math.
    QVariantList spacingRows;
    for (const QString &source : {QString("First $n$ line\\\\"),
            QString("Another line\\\\\n"), QString("Before\n$$x=1$$\nAfter"),
            QString("Before\\\\\n\\[x=2\\]\\\\\nAfter"),
            QString("Before\n\\begin{align}a&=b\\\\c&=d\\end{align}"),
            QString("Keep this\\\\line break")})
        spacingRows.append(QVariantMap{{"source", source}, {"mode", "latex"}});
    const QString spacingPath = blocks.filePath("spacing.tex");
    if (!backend.exportDocumentTex(spacingPath, QVariantMap{{"lines", spacingRows}}).value("error").toString().isEmpty()) return 1;
    QProcess compiler;
    compiler.setWorkingDirectory(blocks.path());
    compiler.start("pdflatex", {"-interaction=nonstopmode", "-halt-on-error", "-no-shell-escape", "spacing.tex"});
    if (!compiler.waitForFinished(15000) || compiler.exitCode() != 0) {
        qCritical() << "FAIL: paragraph/display boundary compilation" << compiler.readAllStandardOutput();
        return 1;
    }
    QFile spacingLog(blocks.filePath("spacing.log"));
    QFile spacingTex(spacingPath);
    if (!spacingLog.open(QIODevice::ReadOnly) || !spacingTex.open(QIODevice::ReadOnly)) return 1;
    const QByteArray spacingOutput = spacingTex.readAll();
    if (spacingLog.readAll().contains("Underfull")
        || !spacingOutput.contains("Keep this\\\\line break")
        || !spacingOutput.contains("\\par\\vspace{\\baselineskip}")
        || !spacingOutput.contains("a&=b\\\\c&=d")) {
        qCritical() << "FAIL: export introduced typesetting warnings or damaged intentional line breaks" << spacingOutput;
        return 1;
    }
    qInfo() << "PASS: legacy row migration, multiline persistence/export and warning-free paragraph/display boundaries";
    return 0;
}
