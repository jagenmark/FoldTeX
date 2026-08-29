#include "backend.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QStandardPaths>
#include <QTimer>
#include <QTemporaryDir>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    if (argc != 2)
        return 2;

    Backend backend;
    const QVariantMap document = backend.loadDocument(QString::fromLocal8Bit(argv[1]));
    const QVariantList lines = document.value(QStringLiteral("lines")).toList();
    const QStringList expected{
        QStringLiteral("5 \\cdot 5 = 32 \\sqrt[3]{5}"),
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
    const QString badSource = QStringLiteral("\\foldtexRegressionMissingCommand{x}");
    const QString color = QStringLiteral("#ffffff");
    const int size = 18;
    const QByteArray key = QCryptographicHash::hash(
        (QStringLiteral("v5\n") + badSource + QLatin1Char('\n') + color + QLatin1Char('\n')
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
        (QStringLiteral("v5\n") + asyncSource + QLatin1Char('\n') + color + QLatin1Char('\n')
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
        {QStringLiteral("lines"), QVariantList{
             QVariantMap{{QStringLiteral("source"), QStringLiteral("another epsilon note")}}
         }}
    });
    const QVariantMap richReloaded = backend.loadDocument(firstNote);
    const QVariantList courseMatches = backend.searchCourse(firstNote, QStringLiteral("epsilon"));
    if (richReloaded.value(QStringLiteral("format")).toString() != QStringLiteral("foldtex-2")
        || richReloaded.value(QStringLiteral("course")).toString() != QStringLiteral("Calculus I")
        || richReloaded.value(QStringLiteral("lines")).toList().first().toMap()
               .value(QStringLiteral("kind")).toString() != QStringLiteral("theorem")
        || courseMatches.size() != 2) {
        qCritical() << "FAIL: rich document or course search" << richReloaded << courseMatches;
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
    qInfo() << "PASS: multiline source became separate render rows";
    return 0;
}
