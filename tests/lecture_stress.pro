QT += core gui widgets qml quick quickcontrols2 quickdialogs2 testlib

CONFIG += c++17 console
CONFIG -= app_bundle
TARGET = lecture_stress
TEMPLATE = app

INCLUDEPATH += ../src
HEADERS += ../src/backend.h ../src/snippetstore.h
SOURCES += lecture_stress.cpp ../src/backend.cpp ../src/snippetstore.cpp

QT += quick qml
HEADERS += ../src/documenteditor.h ../src/latexsyntax.h
SOURCES += ../src/documenteditor.cpp ../src/latexsyntax.cpp

QT += svg

CONFIG += link_pkgconfig
PKGCONFIG += hunspell
HEADERS += ../src/spellchecker.h
SOURCES += ../src/spellchecker.cpp
RESOURCES += ../src/spelling.qrc
