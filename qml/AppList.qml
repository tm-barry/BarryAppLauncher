import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    ScrollView {
        id: scrollView
        anchors.fill: parent
        anchors.margins: 10

        ColumnLayout {
            width: scrollView.width
            spacing: 10

            Label {
                text: "App List"
            }
        }
    }
}
