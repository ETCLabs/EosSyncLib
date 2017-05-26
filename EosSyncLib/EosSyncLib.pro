TEMPLATE = app
TARGET = ../../bin/EosSyncTest
INCLUDEPATH += .
CONFIG += debug

# Input
HEADERS += EosLog.h \
           EosOsc.h \
           EosSyncLib.h \
           EosTcp.h \
           EosTimer.h \
           EosUdp.h \
           OSCParser.h
SOURCES += EosLog.cpp \
           EosOsc.cpp \
           EosSyncLib.cpp \
           EosTcp.cpp \
           EosTimer.cpp \
           EosUdp.cpp \
           main.cpp \
           OSCParser.cpp

win32 {
    HEADERS += EosTcp_Win.h \
               EosUdp_Win.h
    SOURCES += EosTcp_Win.cpp \
               EosUdp_Win.cpp
}

macx {
    HEADERS += EosTcp_Mac.h \
               EosUdp_Mac.h
    SOURCES += EosTcp_Mac.cpp \
               EosUdp_Mac.cpp
}

unix {
    HEADERS += EosTcp_Nix.h \
               EosUdp_Nix.h
    SOURCES += EosTcp_Nix.cpp \
               EosUdp_Nix.cpp
}

