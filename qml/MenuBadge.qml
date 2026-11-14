import QtQuick
import HyprEndfield.Profile 1.0
import "Appearance.js" as Appearance

// endfield_menu/player-badge.js. The content is local desktop identity only.
Item {
    id: badge
    property real leanX: 0
    property real leanY: 0
    signal clicked()
    signal hoverEntered()
    readonly property var profile: profileController.state
    width: 389
    height: 116
    Rectangle {
        x: 8 + badge.leanX * 16
        y: 9 + badge.leanY * 16
        width: 389
        height: 116
        radius: 20
        color: "#1fd6dbe1"
        antialiasing: true
    }
    MenuArt {
        anchors.fill: parent
        asset: "badge"
    }
    Item { x: 10; y: 7; width: 373; height: 102; clip: true; visible: !!badge.profile.background && !Appearance.recording()
        ProfileArtwork { anchors.fill: parent; kind: "thumbnail"; controller: profileController; opacity: .52 }
    }
    Rectangle { x: 26; y: 25; width: 64; height: 64; clip: true; color: "#383838"
        ProfileArtwork { anchors.fill: parent; controller: profileController; visible: !Appearance.recording() }
    }
    ProfileArtwork { x: 12; y: 11; width: 92; height: 92; controller: profileController; kind: "frame"; accent: badge.profile.themeColor || Appearance.accent("#FFFA00") }
    Text {
        x: 16
        y: 45
        width: 84
        text: "LOCAL"
        visible: false
        horizontalAlignment: Text.AlignHCenter
        font.family: harmonyMedium.name
        font.pixelSize: 10
        font.letterSpacing: 1
        color: "#8cffffff"
        renderType: Text.CurveRendering
    }
    Text {
        x: 120
        y: 8
        width: 195
        text: badge.profile.name
        visible: !Appearance.recording()
        elide: Text.ElideRight
        font.family: harmonyBold.name
        font.pixelSize: 24
        color: "white"
        renderType: Text.CurveRendering
    }
    Redaction { x: 120; y: 10; width: 150; height: 26; visible: Appearance.recording() }
    Text {
        x: 120
        y: 37
        text: "#" + badge.profile.tag
        font.family: harmonyRegular.name
        font.pixelSize: 11
        font.letterSpacing: 1.9
        color: "#66ffffff"
        renderType: Text.CurveRendering
    }
    Text {
        x: 120
        y: 47
        text: "ARCH"
        font.family: harmonyBold.name
        font.pixelSize: 36
        color: "white"
        renderType: Text.CurveRendering
    }
    Text {
        x: 120
        y: 96
        text: appController.compositorName
        font.family: harmonyRegular.name
        font.pixelSize: 13
        color: "#80ffffff"
        renderType: Text.CurveRendering
    }
    MouseArea { anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onEntered: badge.hoverEntered(); onClicked: badge.clicked() }
}
