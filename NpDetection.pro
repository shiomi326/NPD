TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    util.cpp \
    double-conversion.cc \
    ujson.cpp \
    vlccapture.cpp

INCLUDEPATH += /usr/local/include/opencv
INCLUDEPATH += /home/shiomi/work/beanstalk-client
#INCLUDEPATH += /home/shiomi/work/picojson-1.3.0
INCLUDEPATH += /usr/include/vlc
INCLUDEPATH += /usr/include/SDL

LIBS += -L/usr/local/lib -lopencv_core -lopencv_imgcodecs -lopencv_highgui -lopencv_videoio -lopencv_imgproc -lopencv_objdetect
LIBS += -L/home/shiomi/work/beanstalk-client -lbeanstalk
LIBS += -L/usr/lib -lvlc
LIBS += -L/usr/lib/x86_64-linux-gnu -lSDL
LIBS += -pthread
HEADERS += \
    util.h \
    double-conversion.h \
    ujson.hpp \
    vlccapture.h \
    fixedqueue.h
