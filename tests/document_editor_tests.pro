QT += core gui widgets qml quick quickcontrols2 quickdialogs2 testlib
CONFIG += c++17 console
CONFIG -= app_bundle
TARGET = document_editor_regression
TEMPLATE = app
INCLUDEPATH += ../src
HEADERS += ../src/backend.h ../src/snippetstore.h ../src/documenteditor.h ../src/latexsyntax.h
SOURCES += document_editor_regression.cpp ../src/backend.cpp ../src/snippetstore.cpp ../src/documenteditor.cpp ../src/latexsyntax.cpp

QT += svg

CONFIG += link_pkgconfig
PKGCONFIG += hunspell
HEADERS += ../src/spellchecker.h
SOURCES += ../src/spellchecker.cpp
RESOURCES += ../src/spelling.qrc
