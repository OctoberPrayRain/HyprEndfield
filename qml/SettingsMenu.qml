import QtQuick
import QtQuick.Controls
import "Appearance.js" as Appearance

Item {
    id: popup
    property var entries: []
    property int selected: -1
    property int maximumHeight: 0
    signal chosen(int index)
    readonly property int fullHeight: entries.length * 30 + 8
    width: 178; height: maximumHeight > 0 ? Math.min(fullHeight, maximumHeight) : fullHeight
    Rectangle { anchors.fill: parent; color: Appearance.tone("#1A1A1A"); border.width: 1; border.color: Appearance.tone("#3D3D3D") }
    Rectangle { width: 10; height: 2; color: Appearance.accent("#D7BC18") }
    Rectangle { width: 2; height: 10; color: Appearance.accent("#D7BC18") }
    Flickable {
        id: scroller
        anchors.fill: parent; anchors.margins: 4
        contentWidth: width; contentHeight: popup.entries.length * 30
        clip: true; boundsBehavior: Flickable.StopAtBounds
        interactive: contentHeight > height
        Repeater {
            model: popup.entries
            delegate: Item {
                required property int index
                required property var modelData
                y: index * 30; width: scroller.width; height: 30
                Rectangle { anchors.fill: parent; color: pointer.containsMouse ? Appearance.tone("#D3D4CF") : "transparent" }
                Rectangle { x: 9; y: 11; width: 8; height: 8; color: index === popup.selected ? Appearance.accent("#FFFA00") : "transparent"; border.width: index === popup.selected ? 0 : 1; border.color: Appearance.tone("#555555") }
                Text { x: 27; width: parent.width - 36; height: 30; text: typeof modelData === "string" ? modelData : modelData.title; color: pointer.containsMouse ? Appearance.tone("#111111") : Appearance.tone("#D9D9D9"); elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; font.family: harmonyMedium.name; font.pixelSize: 13; renderType: Text.CurveRendering }
                MouseArea { id: pointer; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: popup.chosen(index) }
            }
        }
        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded; width: 4
            contentItem: Rectangle { implicitWidth: 4; color: Appearance.accent("#FFFA00") }
        }
    }
}
