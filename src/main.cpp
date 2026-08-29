#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "backend.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("foldtex"));
    app.setDesktopFileName(QStringLiteral("foldtex"));
    app.setOrganizationName(QStringLiteral("Jagenmark"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("accessories-text-editor")));
    QQuickStyle::setStyle(QStringLiteral("Material"));

    QFont font(QStringLiteral("monospace"));
    font.setStyleHint(QFont::Monospace);
    app.setFont(font);

    Backend backend(&app);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
    engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}
