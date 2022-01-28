import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.2
import Style 1.0
import com.nextcloud.desktopclient 1.0

Label {
    required property string section

    topPadding: 8
    bottomPadding: 8
    leftPadding: 16

    text: section
    font.pixelSize: Style.topLinePixelSize
    color: UserModel.currentUser.accentColor

    Accessible.role: Accessible.Separator
    Accessible.name: qsTr("Search results section %1").arg(section)
}
