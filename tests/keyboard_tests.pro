QT += core gui widgets qml quick quickcontrols2 quickdialogs2 testlib

CONFIG += c++17 console
CONFIG -= app_bundle
TARGET = keyboard_regression
TEMPLATE = app

INCLUDEPATH += ../src
HEADERS += ../src/backend.h
SOURCES += keyboard_regression.cpp ../src/backend.cpp
