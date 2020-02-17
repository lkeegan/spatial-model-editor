TARGET = gui
TEMPLATE = lib

QT += core gui widgets printsupport
CONFIG += static

include(../../topdir.pri)
include($$TOPDIR/common.pri)
include($$TOPDIR/ext/ext.pri)
include($$TOPDIR/src/core/core.pri)

SOURCES += \
    $$TOPDIR/src/gui/mainwindow.cpp \
    $$TOPDIR/src/gui/guiutils.cpp \
    $$TOPDIR/src/gui/dialogs/dialogabout.cpp \
    $$TOPDIR/src/gui/dialogs/dialoganalytic.cpp \
    $$TOPDIR/src/gui/dialogs/dialogconcentrationimage.cpp \
    $$TOPDIR/src/gui/dialogs/dialogdisplayoptions.cpp \
    $$TOPDIR/src/gui/dialogs/dialogimagesize.cpp \
    $$TOPDIR/src/gui/dialogs/dialogimageslice.cpp \
    $$TOPDIR/src/gui/dialogs/dialogintegratoroptions.cpp \
    $$TOPDIR/src/gui/dialogs/dialogunits.cpp \
    $$TOPDIR/src/gui/tabs/tabfunctions.cpp \
    $$TOPDIR/src/gui/tabs/tabgeometry.cpp \
    $$TOPDIR/src/gui/tabs/tabreactions.cpp \
    $$TOPDIR/src/gui/tabs/tabsimulate.cpp \
    $$TOPDIR/src/gui/tabs/tabspecies.cpp \
    $$TOPDIR/src/gui/widgets/qlabelmousetracker.cpp \
    $$TOPDIR/src/gui/widgets/qplaintextmathedit.cpp \

HEADERS += \
    $$TOPDIR/src/gui/mainwindow.hpp \
    $$TOPDIR/src/gui/guiutils.hpp \
    $$TOPDIR/src/gui/dialogs/dialogabout.hpp \
    $$TOPDIR/src/gui/dialogs/dialoganalytic.hpp \
    $$TOPDIR/src/gui/dialogs/dialogconcentrationimage.hpp \
    $$TOPDIR/src/gui/dialogs/dialogdisplayoptions.hpp \
    $$TOPDIR/src/gui/dialogs/dialogimagesize.hpp \
    $$TOPDIR/src/gui/dialogs/dialogimageslice.hpp \
    $$TOPDIR/src/gui/dialogs/dialogintegratoroptions.hpp \
    $$TOPDIR/src/gui/dialogs/dialogunits.hpp \
    $$TOPDIR/src/gui/tabs/tabfunctions.hpp \
    $$TOPDIR/src/gui/tabs/tabgeometry.hpp \
    $$TOPDIR/src/gui/tabs/tabreactions.hpp \
    $$TOPDIR/src/gui/tabs/tabsimulate.hpp \
    $$TOPDIR/src/gui/tabs/tabspecies.hpp \
    $$TOPDIR/src/gui/widgets/qlabelmousetracker.hpp \
    $$TOPDIR/src/gui/widgets/qplaintextmathedit.hpp \

FORMS += \
    $$TOPDIR/src/gui/mainwindow.ui \
    $$TOPDIR/src/gui/dialogs/dialogabout.ui \
    $$TOPDIR/src/gui/dialogs/dialoganalytic.ui \
    $$TOPDIR/src/gui/dialogs/dialogconcentrationimage.ui \
    $$TOPDIR/src/gui/dialogs/dialogdisplayoptions.ui \
    $$TOPDIR/src/gui/dialogs/dialogimagesize.ui \
    $$TOPDIR/src/gui/dialogs/dialogimageslice.ui \
    $$TOPDIR/src/gui/dialogs/dialogintegratoroptions.ui \
    $$TOPDIR/src/gui/dialogs/dialogunits.ui \
    $$TOPDIR/src/gui/tabs/tabfunctions.ui \
    $$TOPDIR/src/gui/tabs/tabgeometry.ui \
    $$TOPDIR/src/gui/tabs/tabreactions.ui \
    $$TOPDIR/src/gui/tabs/tabsimulate.ui \
    $$TOPDIR/src/gui/tabs/tabspecies.ui \

INCLUDEPATH += \
    $$TOPDIR/src/gui \
    $$TOPDIR/src/gui/dialogs \
    $$TOPDIR/src/gui/tabs \
    $$TOPDIR/src/gui/widgets \
