#-------------------------------------------------
#
# Project created by QtCreator 2018-06-05T08:47:36
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = measuring
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
        main.cpp \
        measuring.cpp

HEADERS += \
        measuring.h

FORMS += \
        measuring.ui

INCLUDEPATH += E:\openCV\opencv\opencv_build\install\include

LIBS += E:\openCV\opencv\opencv_build\bin\libopencv_core341.dll
LIBS += E:\openCV\opencv\opencv_build\bin\libopencv_highgui341.dll
LIBS += E:\openCV\opencv\opencv_build\bin\libopencv_imgcodecs341.dll
LIBS += E:\openCV\opencv\opencv_build\bin\libopencv_imgproc341.dll
LIBS += E:\openCV\opencv\opencv_build\bin\libopencv_features2d341.dll
LIBS += E:\openCV\opencv\opencv_build\bin\libopencv_calib3d341.dll
