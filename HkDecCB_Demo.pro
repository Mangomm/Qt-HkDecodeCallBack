QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11
Qt += network

#在包含头文件时必须增加include/xxx.h,不能直接写xxx.h否则出错
INCLUDEPATH += D:/Qt_Code/day01/HkDecCB_Demo/include

#在添加库文件时必须加上-L和-l且后面皆不能有空格,否则报没有找到类似目录或文件或者直接报没有xxx.lib/没有权限
LIBS += -LD:/Qt_Code/day01/HkDecCB_Demo/lib -lGdiPlus -lHCCore -lHCNetSDK -lPlayCtrl
LIBS += -LD:/Qt_Code/day01/HkDecCB_Demo/lib/HCNetSDKCom -lHCAlarm -lHCGeneralCfgMgr -lHCPreview



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
    main.cpp \
    hkdecpaly.cpp \
    workerthread.cpp

HEADERS += \
    PlayM4.h \
    hkdecpaly.h \
    workerthread.h

FORMS += \
    hkdecpaly.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

