QT += core gui widgets qml quick quickcontrols2 quickdialogs2

CONFIG += c++17 release
TARGET = foldtex
TEMPLATE = app

HEADERS += src/backend.h
SOURCES += src/main.cpp src/backend.cpp
RESOURCES += src/resources.qrc
