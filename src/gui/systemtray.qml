import QtQuick 2.0
import Qt.labs.platform 1.1

SystemTrayIcon {
    visible: true
    //icon.source: "qrc:/client/theme/colored/state-offiline-32.png"
    icon.source: "qrc:/client/theme/colored/state-sync-32.png";

    Component.onCompleted: {
        showMessage("Desktop Client 2.7", "New QML menu!", 1000)
    }

    onActivated: {
        var component = Qt.createComponent("qrc:/qml/src/gui/traywindow.qml")
        win = component.createObject()
        win.show()
        win.raise()
        win.requestActivate()
    }
}
