import QtQuick
import "ModuleNumbering.js" as ModuleNumbering
import QtQuick.Controls
import QtQuick.Shapes
import "Appearance.js" as Appearance

// 桥接 KDE 应用菜单（KServiceGroup）：类别在右侧卡片区，中央显示图标网格。
Item {
    id: root
    width: 590; height: 510
    property bool presented: false
    property bool reduceMotion: false
    property var catalogData: ({ "categories": [], "apps": [] })
    readonly property string category: appShortcutsController.launchCategory
    readonly property var appsList: catalogData.apps || []

    readonly property var visibleApps: {
        if (category === "all") return appsList
        if (category === "frequent") {
            return appsList.filter(app => app.favorite >= 0).sort((left, right) => left.favorite - right.favorite)
        }
        return appsList.filter(app => (app.categories || []).indexOf(category) >= 0)
    }

    function reload() { catalogData = appShortcutsController.applicationCatalog() }
    onPresentedChanged: { if (presented) reload() }
    Connections {
        target: appShortcutsController
        function onChanged() { if (root.presented) root.reload() }
    }

    SettingsHeader {
        title: qsTr("应用程序"); english: "APPLICATIONS"; code: ModuleNumbering.code("addApp")
        count: String(root.appsList.length).padStart(2, "0")
    }

    // 中央图标区
    GridView {
        id: grid; objectName: "appLauncherGrid"
        x: 55; y: 64; width: 438; height: 422
        cellWidth: 109; cellHeight: 98; clip: true
        model: root.visibleApps; boundsBehavior: Flickable.StopAtBounds
        delegate: Item {
            required property var modelData
            width: grid.cellWidth; height: grid.cellHeight
            Rectangle {
                anchors.fill: parent; anchors.margins: 3
                color: tileMouse.containsMouse ? "#1CFFFFFF" : "transparent"
                border.width: 1
                border.color: tileMouse.containsMouse ? Appearance.accent("#FFFA00") : "#00000000"
            }
            Rectangle { anchors.horizontalCenter: parent.horizontalCenter; y: 10; width: 46; height: 46; color: "#232323"; visible: !modelData.icon }
            Image {
                anchors.horizontalCenter: parent.horizontalCenter; y: 10; width: 46; height: 46
                source: modelData.icon || ""; sourceSize: Qt.size(92, 92); mipmap: true
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter; y: 60
                width: parent.width - 12; horizontalAlignment: Text.AlignHCenter
                text: modelData.name; color: "#E8E8E8"
                elide: Text.ElideRight; maximumLineCount: 2; wrapMode: Text.Wrap
                font.family: harmonyRegular.name; font.pixelSize: 11; renderType: Text.CurveRendering
            }
            MouseArea {
                id: tileMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                onClicked: appShortcutsController.requestOpenPath(modelData.path)
            }
        }
        ScrollBar.vertical: ScrollBar {
            width: 4; policy: ScrollBar.AsNeeded; active: true
            contentItem: Rectangle { implicitWidth: 4; color: Appearance.accent("#FFFA00") }
            background: Rectangle { color: "#222222"; visible: false }
        }
    }

    Text {
        x: 55; y: 250; width: 438; horizontalAlignment: Text.AlignHCenter
        visible: root.visibleApps.length === 0
        text: root.category === "frequent" ? qsTr("还没有常用记录，从“全部应用程序”里启动一次即可") : qsTr("此类别暂无应用")
        color: Appearance.tone("#777777"); wrapMode: Text.Wrap
        font.family: harmonyMedium.name; font.pixelSize: 12; renderType: Text.CurveRendering
    }

    Text {
        x: 0; y: 506; width: 590
        text: root.category === "frequent" ? qsTr("来自 KDE 收藏列表") : qsTr("共 %1 个应用").arg(root.visibleApps.length)
        color: "#6E6E6E"; font.family: harmonyRegular.name; font.pixelSize: 10; renderType: Text.CurveRendering
    }
}
