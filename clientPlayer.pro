QT       += core gui network
QT += multimedia multimediawidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    ffmpegprocessor.cpp \
    fileuploader.cpp \
    main.cpp \
    mainwindow.cpp \
    tmyvideowidget.cpp \
    videoplayer.cpp

HEADERS += \
    ffmpegprocessor.h \
    fileuploader.h \
    mainwindow.h \
    tmyvideowidget.h \
    videoplayer.h

FORMS += \
    mainwindow.ui

INCLUDEPATH += $$PWD/ffmpeg-4.3.1-full_build-shared/include
INCLUDEPATH += $$PWD/SDL2-2.32.10/include

LIBS += $$PWD/ffmpeg-4.3.1-full_build-shared/lib/avformat.lib   \
        $$PWD/ffmpeg-4.3.1-full_build-shared/lib/avcodec.lib    \
        $$PWD/ffmpeg-4.3.1-full_build-shared/lib/avdevice.lib   \
        $$PWD/ffmpeg-4.3.1-full_build-shared/lib/avfilter.lib   \
        $$PWD/ffmpeg-4.3.1-full_build-shared/lib/avutil.lib     \
        $$PWD/ffmpeg-4.3.1-full_build-shared/lib/postproc.lib   \
        $$PWD/ffmpeg-4.3.1-full_build-shared/lib/swresample.lib \
        $$PWD/ffmpeg-4.3.1-full_build-shared/lib/swscale.lib    \
        $$PWD/SDL2-2.32.10/lib/x64/SDL2.lib

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    res.qrc
