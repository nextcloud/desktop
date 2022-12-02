import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.2
import Style 1.0
import com.nextcloud.desktopclient 1.0

EnforcedPlainTextLabel {
    required property string section

    topPadding: Style.unifiedSearchResultSectionItemVerticalPadding
    bottomPadding: Style.unifiedSearchResultSectionItemVerticalPadding
    leftPadding: Style.unifiedSearchResultSectionItemLeftPadding

    text: section
    font.pixelSize: Style.unifiedSearchResultTitleFontSize
    color: UserModel.currentUser.accentColor

    Accessible.role: Accessible.Separator
    Accessible.name: qsTr("Search results section %1").arg(section)
}
