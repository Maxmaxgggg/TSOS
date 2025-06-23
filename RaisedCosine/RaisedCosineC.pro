QT       += core gui
QT       += widgets printsupport
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    src/main.cpp \
    src/widget.cpp \
    src/qcustomplot.cpp \
    src/parson.c \
    src/tinyspline.c

HEADERS += \
    src/widget.h \
    src/qcustomplot.h \
    src/parson.h \
    src/tinyspline.h \
    src/check_fft.hpp \
    src/copy_array.hpp \
    src/error_handling.hpp \
    src/fft.h \
    src/fft.hpp \
    src/fft_impl.hpp \
    src/fft_settings.h

FORMS += \
    src/widget.ui
INCLUDEPATH += \
    src/
# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    src/q.qrc

RC_ICONS = src/icon.ico
