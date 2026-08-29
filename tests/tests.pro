QT += core gui widgets

CONFIG += c++17 console
CONFIG -= app_bundle
TARGET = backend_regression
TEMPLATE = app

INCLUDEPATH += ../src
HEADERS += ../src/backend.h
SOURCES += backend_regression.cpp ../src/backend.cpp
