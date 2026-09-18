QT += core gui widgets qml quick quickcontrols2 quickdialogs2

CONFIG += c++17 release
TARGET = foldtex
TEMPLATE = app

HEADERS += src/backend.h src/snippetstore.h
SOURCES += src/main.cpp src/backend.cpp src/snippetstore.cpp
RESOURCES += src/resources.qrc

isEmpty(PREFIX): PREFIX = /usr/local
target.path = $$PREFIX/bin
desktop.files = foldtex.desktop
desktop.path = $$PREFIX/share/applications
mime.files = foldtex-mime.xml
mime.path = $$PREFIX/share/mime/packages
INSTALLS += target desktop mime

QT += quick qml
HEADERS += src/documenteditor.h src/latexsyntax.h
SOURCES += src/documenteditor.cpp src/latexsyntax.cpp

QT += svg

CONFIG += link_pkgconfig
PKGCONFIG += hunspell
HEADERS += src/spellchecker.h
SOURCES += src/spellchecker.cpp
RESOURCES += src/spelling.qrc
