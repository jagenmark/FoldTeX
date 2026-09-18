#include "backend.h"
#include "documenteditor.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>
#include <QTest>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <vector>

namespace {
struct Measurement {
    QString name;
    double milliseconds = 0;
    double limit = 0;
};

bool check(bool condition, const QString &message) {
    if (!condition) {
        qCritical().noquote() << "FAIL:" << message;
        std::fprintf(stderr, "FAIL: %s\n", qPrintable(message));
    }
    return condition;
}

Measurement measure(const QString &name, double limit,
                    const std::function<bool()> &operation, bool &operationsOk) {
    QElapsedTimer timer;
    timer.start();
    const bool ok = operation();
    const double elapsed = timer.nsecsElapsed() / 1000000.0;
    operationsOk = operationsOk && ok;
    std::fprintf(stdout, "TIMING %s %.2f ms (limit %.0f ms)\n",
                 qPrintable(name), elapsed, limit);
    std::fflush(stdout);
    return {name, elapsed, limit};
}

QVariantMap stressLine(int index) {
    const QString marker = index % 25 == 0 ? QStringLiteral(" stress-needle") : QString();
    const int shape = index % 12;
    QString source;
    QString kind = QStringLiteral("normal");
    QString label;
    QString mode = QStringLiteral("auto");

    if (shape == 0) {
        source = QStringLiteral("Lecture prose row %1 about limits and continuity%2")
                     .arg(index).arg(marker);
    } else if (shape == 1) {
        source = QStringLiteral("\\frac{x_{%1}+1}{%2}=y_{%1}%3")
                     .arg(index).arg(index + 2).arg(marker);
    } else if (shape == 2) {
        source = QStringLiteral("Definition %1: a bounded sequence%2").arg(index).arg(marker);
        kind = QStringLiteral("definition");
        label = QStringLiteral("Definition");
        mode = QStringLiteral("text");
    } else if (shape == 3) {
        source = QStringLiteral("a_{%1} \\leq M \\quad \\forall %1%2").arg(index).arg(marker);
        kind = QStringLiteral("theorem");
        label = QStringLiteral("Theorem");
        mode = QStringLiteral("math");
    } else if (shape == 4) {
        source = QStringLiteral("Proof step %1 follows from the prior estimate%2")
                     .arg(index).arg(marker);
        kind = QStringLiteral("proof");
        label = QStringLiteral("Proof");
        mode = QStringLiteral("text");
    } else if (shape == 5) {
        source = QStringLiteral("\\sum_{k=1}^{%1} k = \\frac{%1(%1+1)}{2}%2")
                     .arg(index + 1).arg(marker);
        kind = QStringLiteral("example");
        label = QStringLiteral("Example");
    } else if (shape == 6) {
        source = QStringLiteral("Forced prose with x_%1 = y_%1 kept as text%2")
                     .arg(index).arg(marker);
        mode = QStringLiteral("text");
    } else if (shape == 7) {
        source = QStringLiteral("x_{%1} \\in \\mathbb{R}%2").arg(index).arg(marker);
        mode = QStringLiteral("math");
    } else if (shape == 8) {
        source = QStringLiteral("Problem-solving note %1: isolate the unknown%2")
                     .arg(index).arg(marker);
    } else if (shape == 9) {
        source = QStringLiteral("\\begin{bmatrix}%1&1\\\\0&%2\\end{bmatrix}%3")
                     .arg(index).arg(index + 1).arg(marker);
    } else if (shape == 10) {
        source = QStringLiteral("Example discussion row %1 with a check%2")
                     .arg(index).arg(marker);
        kind = QStringLiteral("example");
        label = QStringLiteral("Example");
    } else {
        source = QStringLiteral("\\lim_{n \\to \\infty} \\frac{%1}{n}=0%2")
                     .arg(index).arg(marker);
    }

    return {
        {QStringLiteral("source"), source},
        {QStringLiteral("kind"), kind},
        {QStringLiteral("label"), label},
        {QStringLiteral("asset"), QString()},
        {QStringLiteral("slide"), -1},
        {QStringLiteral("mode"), mode}
    };
}
}

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("foldtex-lecture-stress"));
    app.setOrganizationName(QStringLiteral("JagenmarkTests"));
    QSettings().clear();
    QQuickStyle::setStyle(QStringLiteral("Material"));
    if (argc != 2)
        return 2;

    Backend backend;
    backend.saveRecovery(QStringLiteral("Stress bootstrap"), {
        QVariantMap{{QStringLiteral("source"), QStringLiteral("bootstrap prose")},
                    {QStringLiteral("mode"), QStringLiteral("text")}}
    });

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
    engine.load(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])));
    if (!check(!engine.rootObjects().isEmpty(), QStringLiteral("Could not load Main.qml")))
        return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QObject *searchInput = window
        ? window->findChild<QObject *>(QStringLiteral("searchInput")) : nullptr;
    if (!check(window && searchInput, QStringLiteral("Stress-test QML objects were not found")))
        return 1;

    QTest::qWait(80);
    QMetaObject::invokeMethod(window, "cancelQueuedRenders", Qt::DirectConnection);

    constexpr int rowCount = 1500;
    QVariantList rows;
    rows.reserve(rowCount);
    for (int index = 0; index < rowCount; ++index)
        rows.append(stressLine(index));
    const QVariantMap document{
        {QStringLiteral("title"), QStringLiteral("Long lecture stress note")},
        {QStringLiteral("course"), QStringLiteral("Performance studies")},
        {QStringLiteral("noteKind"), QStringLiteral("lecture")},
        {QStringLiteral("lecture"), QStringLiteral("1,500-row lecture")},
        {QStringLiteral("lectureDate"), QStringLiteral("2026-08-31")},
        {QStringLiteral("lines"), rows}
    };

    bool operationsOk = true;
    std::vector<Measurement> measurements;
    measurements.push_back(measure(QStringLiteral("loadData(1500 rows)"), 2500,
        [&]() {
            return QMetaObject::invokeMethod(
                window, "loadData", Qt::DirectConnection,
                Q_ARG(QVariant, QVariant(document)), Q_ARG(QVariant, QVariant(QString())));
        }, operationsOk));

    measurements.push_back(measure(QStringLiteral("editLine x200"), 1500,
        [&]() {
            bool ok = true;
            for (int step = 0; step < 200; ++step) {
                const int index = (step * 73) % rowCount;
                ok = QMetaObject::invokeMethod(window, "editLine", Qt::DirectConnection,
                                               Q_ARG(QVariant, QVariant(index))) && ok;
            }
            return ok;
        }, operationsOk));
    QMetaObject::invokeMethod(window, "cancelQueuedRenders", Qt::DirectConnection);

    // Row mode switching was removed: text and math now follow LaTeX delimiters.
    const QStringList kinds{QStringLiteral("definition"), QStringLiteral("theorem"),
                            QStringLiteral("proof"), QStringLiteral("example"),
                            QStringLiteral("normal")};
    measurements.push_back(measure(QStringLiteral("row kind + history x40"), 2500,
        [&]() {
            bool ok = true;
            for (int step = 0; step < 40; ++step) {
                const int index = (step * 41) % rowCount;
                ok = QMetaObject::invokeMethod(window, "editLine", Qt::DirectConnection,
                                               Q_ARG(QVariant, QVariant(index))) && ok;
                ok = QMetaObject::invokeMethod(window, "setActiveRowKind", Qt::DirectConnection,
                                               Q_ARG(QVariant, QVariant(kinds.at(step % 5)))) && ok;
            }
            return ok;
        }, operationsOk));

    QVariant serialized;
    auto *editor = window->findChild<DocumentEditor *>(QStringLiteral("documentEditor"));
    if (!check(editor, "Continuous editor not found")) return 1;
    editor->editRow(0, 0);
    measurements.push_back(measure(QStringLiteral("type 30 characters in long note"), 1500,
        [&]() {
            for (int i = 0; i < 30; ++i) QTest::keyClick(window, 'a');
            return editor->text().startsWith(QString(30, 'a'));
        }, operationsOk));

    measurements.push_back(measure(QStringLiteral("serializedLines x10"), 2000,
        [&]() {
            bool ok = true;
            for (int step = 0; step < 10; ++step) {
                serialized.clear();
                ok = QMetaObject::invokeMethod(window, "serializedLines", Qt::DirectConnection,
                                               Q_RETURN_ARG(QVariant, serialized)) && ok;
            }
            return ok;
        }, operationsOk));

    searchInput->setProperty("text", QStringLiteral("stress-needle"));
    QVariant matchCount;
    measurements.push_back(measure(QStringLiteral("searchMatchCount x20"), 1500,
        [&]() {
            bool ok = true;
            for (int step = 0; step < 20; ++step) {
                matchCount.clear();
                ok = QMetaObject::invokeMethod(window, "searchMatchCount", Qt::DirectConnection,
                                               Q_RETURN_ARG(QVariant, matchCount)) && ok;
            }
            return ok;
        }, operationsOk));

    measurements.push_back(measure(QStringLiteral("queueRenders(all rows)"), 1500,
        [&]() {
            return QMetaObject::invokeMethod(window, "queueRenders", Qt::DirectConnection,
                                             Q_ARG(QVariant, QVariant(true)));
        }, operationsOk));
    const int pendingRenders = window->property("pendingRenderCount").toInt();
    QMetaObject::invokeMethod(window, "cancelQueuedRenders", Qt::DirectConnection);

    bool passed = operationsOk;
    for (const Measurement &measurement : measurements) {
        if (measurement.milliseconds > measurement.limit) {
            qCritical().noquote()
                << "FAIL:" << measurement.name << "took"
                << QString::number(measurement.milliseconds, 'f', 2) << "ms; limit is"
                << QString::number(measurement.limit, 'f', 0) << "ms";
            passed = false;
        }
    }
    const QVariantList serializedRows = serialized.toList();
    passed = check(serializedRows.size() == rowCount + 1,
                   QStringLiteral("Long note did not retain 1,500 rows plus its trailing row"))
        && passed;
    passed = check(matchCount.toInt() == rowCount / 25,
                   QStringLiteral("Full-note search returned the wrong match count"))
        && passed;
    passed = check(pendingRenders > 0 && pendingRenders <= rowCount + 1,
                   QStringLiteral("Math render work was not queued or was duplicated"))
        && passed;

    QVariantList plainRows;
    plainRows.reserve(rowCount);
    for (int index = 0; index < rowCount; ++index) {
        plainRows.append(QVariantMap{{QStringLiteral("source"),
                                      QStringLiteral("plain lecture row %1").arg(index)},
                                     {QStringLiteral("mode"), QStringLiteral("text")}});
    }
    const QVariantMap plainDocument{{QStringLiteral("lines"), plainRows}};
    QMetaObject::invokeMethod(window, "loadData", Qt::DirectConnection,
                              Q_ARG(QVariant, QVariant(plainDocument)),
                              Q_ARG(QVariant, QVariant(QString())));
    QMetaObject::invokeMethod(window, "cancelQueuedRenders", Qt::DirectConnection);
    QMetaObject::invokeMethod(window, "queueRenders", Qt::DirectConnection,
                              Q_ARG(QVariant, QVariant(true)));
    measurements.push_back(measure(QStringLiteral("drain plain render queue"), 250,
        [&]() {
            return QMetaObject::invokeMethod(window, "queueRenders",
                                             Qt::DirectConnection, Q_ARG(QVariant, QVariant(true)));
        }, operationsOk));
    passed = check(window->property("pendingRenderCount").toInt() <= 1,
                   QStringLiteral("Plain text queued unnecessary math renders")) && passed;

    const auto slowest = std::max_element(
        measurements.cbegin(), measurements.cend(),
        [](const Measurement &left, const Measurement &right) {
            return left.milliseconds < right.milliseconds;
        });
    if (slowest != measurements.cend())
        std::fprintf(stdout, "SLOWEST %s %.2f ms\n",
                     qPrintable(slowest->name), slowest->milliseconds);
    std::fprintf(stdout, "ROWS %d PENDING_RENDERS %d SEARCH_MATCHES %d\n",
                 rowCount, pendingRenders, matchCount.toInt());
    std::fflush(stdout);
    return passed ? 0 : 1;
}
