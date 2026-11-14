import QtQuick
import "ModuleNumbering.js" as ModuleNumbering
import "Appearance.js" as Appearance
import QtQuick.Shapes

// EndfieldHUD ClipboardCanvas, MOD-03, backed by the wlr-data-control clipboard.
Item {
    id: clipboardPage
    width: 590; height: 510; focus: presented
    property bool presented: false
    property bool reduceMotion: false
    readonly property var clipboard: clipboardController.state
    property int selectedFilter: 0
    property int pendingFilter: 0
    property int direction: 1
    property string motionPhase: "idle"
    property double now: Date.now()
    property string toastTitle: ""
    property string toastDetail: ""
    property bool toastOpen: false
    property string contextId: ""
    property real contextX: 0
    property real contextY: 0
    readonly property var filterKinds: ["all", "text", "url", "image", "files"]
    readonly property var filterTitles: [qsTr("全部"), qsTr("文字"), qsTr("链接"), qsTr("图片"), qsTr("文件")]
    readonly property var filteredItems: {
        const kind = filterKinds[selectedFilter]
        const values = clipboard.items.filter(item => kind === "all" || item.kind === kind)
        return values.filter(item => item.pinned).concat(values.filter(item => !item.pinned))
    }
    readonly property var contextEntry: clipboard.items.find(item => item.id === contextId)

    function pad(value) { return String(value).padStart(2, "0") }
    function showToast(title, detail) { toastTitle = title; toastDetail = detail || ""; toastOpen = true; toastTimer.restart() }
    function age(createdAt) {
        if (!createdAt) return ""
        const seconds = Math.max(0, Math.floor((now - createdAt) / 1000))
        if (seconds < 60) return qsTr("刚刚")
        const minutes = Math.floor(seconds / 60)
        if (minutes < 60) return minutes + qsTr(" 分钟")
        const hours = Math.floor(minutes / 60)
        if (hours < 24) return hours + qsTr(" 小时")
        return Math.floor(hours / 24) + qsTr(" 天")
    }
    function selectFilter(index) {
        if (index === selectedFilter || motionPhase !== "idle") return
        direction = index > selectedFilter ? 1 : -1; pendingFilter = index
        if (reduceMotion) { selectedFilter = index; rows.currentIndex = -1; return }
        motionPhase = "leave"; filterSwap.restart()
    }
    function openContext(id, localX, localY, itemY) {
        contextId = id
        contextX = Math.max(0, Math.min(440, localX))
        contextY = Math.max(125, Math.min(406, 125 + itemY - rows.contentY + localY))
    }
    function copyAt(index) {
        if (index < 0 || index >= filteredItems.length) return
        const delegate = rows.itemAtIndex(index)
        if (clipboardController.copyItem(filteredItems[index].id) && delegate) delegate.flashCopied()
    }
    function copyVisible(offset) {
        const first = Math.max(0, rows.indexAt(2, rows.contentY + 2))
        copyAt(first + offset)
    }
    function moveFocus(step) {
        if (!filteredItems.length) return
        rows.currentIndex = rows.currentIndex < 0 ? (step < 0 ? filteredItems.length - 1 : 0)
            : Math.max(0, Math.min(filteredItems.length - 1, rows.currentIndex + step))
        rows.positionViewAtIndex(rows.currentIndex, ListView.Contain)
    }
    function askClear() {
        if (clipboard.removable <= 0) return
        contextId = ""; confirmLayer.visible = true
    }

    Component.onCompleted: now = Date.now()
    onPresentedChanged: {
        clipboardController.active = presented
        if (presented) rootFocus.forceActiveFocus()
        else { contextId = ""; confirmLayer.visible = false }
    }
    Component.onDestruction: clipboardController.active = false
    Connections {
        target: clipboardController
        function onActionFinished(action, success, message) {
            if (!success) { clipboardPage.showToast(qsTr("提示"), message); return }
            if (action === "copy") clipboardPage.showToast(qsTr("已复制到剪贴板"), "")
            else if (action === "clear") clipboardPage.showToast(qsTr("剪贴板记录已清空"), "")
        }
    }
    Item { id: rootFocus; anchors.fill: parent; focus: true }
    Timer { interval: 30000; running: clipboardPage.presented; repeat: true; onTriggered: clipboardPage.now = Date.now() }
    Timer { id: toastTimer; interval: 1500; onTriggered: clipboardPage.toastOpen = false }
    Timer {
        id: filterSwap; interval: 130
        onTriggered: { clipboardPage.selectedFilter = clipboardPage.pendingFilter; rows.currentIndex = -1; clipboardPage.motionPhase = "enter"; filterSettle.restart() }
    }
    Timer { id: filterSettle; interval: 476; onTriggered: clipboardPage.motionPhase = "idle" }
    Shortcut { sequence: "Q"; enabled: clipboardPage.presented && !confirmLayer.visible; onActivated: clipboardPage.selectFilter((clipboardPage.selectedFilter + 4) % 5) }
    Shortcut { sequence: "E"; enabled: clipboardPage.presented && !confirmLayer.visible; onActivated: clipboardPage.selectFilter((clipboardPage.selectedFilter + 1) % 5) }
    Shortcut { sequence: "Up"; enabled: clipboardPage.presented && !confirmLayer.visible; onActivated: clipboardPage.moveFocus(-1) }
    Shortcut { sequence: "Down"; enabled: clipboardPage.presented && !confirmLayer.visible; onActivated: clipboardPage.moveFocus(1) }
    Shortcut { sequence: "Return"; enabled: clipboardPage.presented && !confirmLayer.visible; onActivated: clipboardPage.copyAt(rows.currentIndex) }
    Shortcut { sequence: "Delete"; enabled: clipboardPage.presented && !confirmLayer.visible && rows.currentIndex >= 0; onActivated: clipboardController.removeItem(clipboardPage.filteredItems[rows.currentIndex].id) }
    Shortcut { sequence: "1"; enabled: clipboardPage.presented && !confirmLayer.visible; onActivated: clipboardPage.copyVisible(0) }
    Shortcut { sequence: "2"; enabled: clipboardPage.presented && !confirmLayer.visible; onActivated: clipboardPage.copyVisible(1) }
    Shortcut { sequence: "3"; enabled: clipboardPage.presented && !confirmLayer.visible; onActivated: clipboardPage.copyVisible(2) }
    Shortcut { sequence: "4"; enabled: clipboardPage.presented && !confirmLayer.visible; onActivated: clipboardPage.copyVisible(3) }
    Shortcut { sequence: "5"; enabled: clipboardPage.presented && !confirmLayer.visible; onActivated: clipboardPage.copyVisible(4) }
    Shortcut { sequence: "6"; enabled: clipboardPage.presented && !confirmLayer.visible; onActivated: clipboardPage.copyVisible(5) }
    Shortcut { sequence: "Escape"; enabled: clipboardPage.presented && (confirmLayer.visible || contextId !== ""); onActivated: { confirmLayer.visible = false; contextId = "" } }

    Item {
        width: 590; height: 81
        Row {
            y: 1.75; spacing: 6
            Repeater {
                model: ["// HUD", "/", "CLIPBOARD"]
                Text {
                    required property string modelData; required property int index
                    text: modelData; color: index === 2 ? Appearance.tone("#E6E6E6") : Appearance.tone("#8A8A8A")
                    font.family: harmonyRegular.name; font.pointSize: 10.5 * 0.75; font.letterSpacing: 0.63; renderType: Text.CurveRendering
                }
            }
        }
        Text {
            x: 549 - width; y: 1.75; text: ModuleNumbering.code("clipboard"); color: Appearance.tone("#8A8A8A")
            font.family: harmonyRegular.name; font.pointSize: 10.5 * 0.75; font.letterSpacing: 0.63; renderType: Text.CurveRendering
        }
        Item {
            x: 556; y: 3.5; width: 34; height: 7; clip: true
            Shape {
                anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
                ShapePath { strokeWidth: -1; fillColor: Qt.rgba(1, 1, 1, 0.14); PathSvg { path: { let p = ""; for (let x = -7; x < 41; x += 3.5 * Math.SQRT2) p += "M" + x + " 7h1.414l7 -7h-1.414Z"; return p } } }
            }
        }
        Text {
            id: title; y: 20; height: 30; text: qsTr("剪贴板缓存"); color: Appearance.tone("#F4F4F4")
            font.family: harmonyBold.name; font.pixelSize: 26; font.weight: Font.Bold; font.letterSpacing: 2.08; renderType: Text.CurveRendering
        }
        Text {
            x: title.width + 12; y: title.y + title.baselineOffset - baselineOffset; text: "CLIPBOARD"; color: Appearance.tone("#8A8A8A")
            font.family: harmonyMedium.name; font.pixelSize: 13; font.letterSpacing: 2.6; font.weight: Font.Medium; renderType: Text.CurveRendering
        }
        Item {
            id: clearButton; x: 512; y: 22; width: 78; height: 26; opacity: clipboardPage.clipboard.removable > 0 ? 1 : 0.35
            transform: [Translate { y: clearMouse.containsMouse && clearMouse.enabled ? -1 : 0 }, Scale {
                origin.x: clearButton.width / 2; origin.y: clearButton.height / 2; xScale: clearMouse.pressed ? 0.965 : 1; yScale: xScale
            }]
            Rectangle { anchors.fill: parent; color: clearMouse.containsMouse ? Appearance.tone("#343434") : Appearance.tone("#242424") }
            Rectangle { x: 8; y: 7; width: clearMouse.containsMouse ? 7 : 4; height: 12; color: Appearance.accent("#FFFA00") }
            ClipboardGlyph { x: 22; y: 5.5; width: 15; height: 15; kind: "trash" }
            Text { x: 45; height: 26; verticalAlignment: Text.AlignVCenter; text: qsTr("清空"); color: Appearance.tone("#EDEDED"); font.family: harmonyBold.name; font.pointSize: 10.5 * 0.75; font.weight: Font.Bold; renderType: Text.CurveRendering }
            MouseArea { id: clearMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; enabled: clipboardPage.clipboard.removable > 0; onClicked: clipboardPage.askClear() }
        }
        Item {
            y: 60; width: 590; height: 9
            Rectangle { y: 4; width: 590; height: 1; color: "#38FFFFFF" }
            Rectangle { y: 3; width: 56; height: 3; color: Appearance.accent("#FFFA00") }
            Repeater { model: 10; Rectangle { required property int index; x: 454 + index * 9; width: 1; height: 9; color: "#59FFFFFF" } }
            Rectangle { x: 590 - width; y: -2; width: counter.width + 7; height: 12; color: Appearance.tone("#151515") }
            Text {
                id: counter; x: 590 - width; y: -1.5; text: clipboardPage.pad(clipboardPage.clipboard.total) + "/" + clipboardPage.pad(clipboardPage.clipboard.capacity)
                color: Appearance.accent("#FFFA00"); font.family: harmonyBold.name; font.pixelSize: 11; font.weight: Font.Bold; renderType: Text.CurveRendering
            }
        }
    }

    Item {
        id: tabs; y: 81; width: 590; height: 44
        Rectangle { width: 590; height: 34; color: Appearance.tone("#1E1E1E"); border.color: Appearance.tone("#343434") }
        Repeater {
            model: 4
            Rectangle {
                required property int index
                x: 28 + (index + 1) * 106.8; y: 10; width: 1; height: 14
                visible: index + 1 !== clipboardPage.selectedFilter && index !== clipboardPage.selectedFilter
                color: Appearance.tone("#4A4A4A")
            }
        }
        Repeater {
            model: ["Q", "E"]
            delegate: Rectangle {
                required property string modelData; required property int index
                x: index === 0 ? 6.5 : 568.5; y: 9.5; width: 15; height: 15; color: "transparent"; border.color: Appearance.tone("#6A6A6A")
                Text { anchors.centerIn: parent; text: modelData; color: Appearance.tone("#8A8A8A"); font.family: harmonyBold.name; font.pixelSize: 9; font.weight: Font.Bold; renderType: Text.CurveRendering }
            }
        }
        Repeater {
            model: clipboardPage.filterTitles
            delegate: Item {
                id: tab
                required property string modelData; required property int index
                readonly property bool selected: index === clipboardPage.selectedFilter
                x: 28 + index * 106.8; y: selected ? -3 : 0; width: 106.8; height: selected ? 37 : 34
                transform: Translate { y: tabHover.hovered && !tab.selected ? -1 : 0 }
                Rectangle { anchors.fill: parent; color: "white"; opacity: tabHover.hovered && !tab.selected ? 0.06 : 0 }
                Rectangle {
                    anchors.fill: parent; visible: tab.selected
                    gradient: Gradient { orientation: Gradient.Horizontal; GradientStop { position: 0; color: Appearance.accent("#FFFC6B") } GradientStop { position: 0.45; color: Appearance.accent("#FFFA00") } GradientStop { position: 1; color: Appearance.accent("#FFFA00") } }
                }
                Text {
                    anchors.centerIn: parent; text: tab.modelData; color: tab.selected ? Appearance.tone("#111111") : tabHover.hovered ? Appearance.tone("#F2F2F2") : Appearance.tone("#A0A0A0")
                    font.family: tab.selected ? harmonyBold.name : harmonyMedium.name; font.pixelSize: 13; font.weight: tab.selected ? Font.Bold : Font.Medium; font.letterSpacing: 0.52; renderType: Text.CurveRendering
                }
                Rectangle { x: width * 0.32; y: height + 3; width: width * 0.36; height: 3; visible: tab.selected; color: Appearance.accent("#FFFA00") }
                HoverHandler { id: tabHover }
                MouseArea { anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: clipboardPage.selectFilter(tab.index) }
            }
        }
    }

    NowPlayingSection {
        page: clipboardPage; sectionIndex: 0; x: 0; y: 125; width: 590; height: 350
        ListView {
            id: rows; objectName: "clipboardRows"; anchors.fill: parent; clip: true
            model: clipboardPage.filteredItems; spacing: 6; boundsBehavior: Flickable.StopAtBounds; reuseItems: true
            delegate: ClipboardCard {
                required property var modelData; required property int index
                width: rows.contentHeight > rows.height ? 580 : 584
                entry: modelData; focused: rows.currentIndex === index; reduceMotion: clipboardPage.reduceMotion; age: clipboardPage.age(modelData.createdAt)
                onCopied: id => { if (clipboardController.copyItem(id)) flashCopied() }
                onPinToggled: id => clipboardController.togglePin(id)
                onRemoved: id => clipboardController.removeItem(id)
                onContextRequested: (id, localX, localY) => clipboardPage.openContext(id, localX, localY, y)
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter; y: 104; visible: rows.count === 0
                text: clipboardPage.clipboard.ready ? qsTr("NO RECORDS\n剪贴板为空") : qsTr("WAYLAND OFFLINE\n无法监视系统剪贴板")
                color: Appearance.tone("#777777"); horizontalAlignment: Text.AlignHCenter; font.family: harmonyMedium.name; font.pixelSize: 12; font.letterSpacing: 1.2; lineHeight: 1.8; renderType: Text.CurveRendering
            }
        }
        Rectangle {
            x: 586; width: 4; height: 350; visible: rows.contentHeight > rows.height; color: Appearance.tone("#222222")
            Rectangle {
                width: 4; color: Appearance.accent("#FFFA00"); height: Math.max(18, rows.height * rows.height / rows.contentHeight)
                y: rows.contentHeight > rows.height ? rows.contentY / (rows.contentHeight - rows.height) * (rows.height - height) : 0
            }
        }
    }

    Item {
        anchors.fill: parent; z: 80; visible: clipboardPage.contextId !== ""
        MouseArea { anchors.fill: parent; onClicked: clipboardPage.contextId = "" }
    }
    Rectangle {
        id: contextMenu; x: clipboardPage.contextX; y: clipboardPage.contextY; z: 81; width: 150; height: 96
        visible: clipboardPage.contextId !== ""; color: "#F51B1B1B"; border.color: Appearance.tone("#555555")
        Column {
            anchors.fill: parent
            Repeater {
                model: [{title: qsTr("复制"), action: "copy"}, {title: clipboardPage.contextEntry && clipboardPage.contextEntry.pinned ? qsTr("取消固定") : qsTr("固定"), action: "pin"}, {title: qsTr("删除"), action: "remove"}]
                delegate: Item {
                    required property var modelData; required property int index
                    width: 150; height: 32
                    readonly property bool available: index !== 2 || !(clipboardPage.contextEntry && clipboardPage.contextEntry.pinned)
                    opacity: available ? 1 : 0.35
                    Rectangle { anchors.fill: parent; color: menuMouse.containsMouse && parent.available ? Appearance.tone("#333333") : "transparent" }
                    Rectangle { x: 0; y: 31; width: 150; height: 1; color: index === 1 ? Appearance.tone("#444444") : "transparent" }
                    Text { x: 14; height: 32; verticalAlignment: Text.AlignVCenter; text: modelData.title; color: index === 2 ? "#E35B5D" : Appearance.tone("#EEEEEE"); font.family: harmonyMedium.name; font.pixelSize: 12; renderType: Text.CurveRendering }
                    MouseArea {
                        id: menuMouse; anchors.fill: parent; hoverEnabled: true; enabled: parent.available; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            const id = clipboardPage.contextId; clipboardPage.contextId = ""
                            if (modelData.action === "copy") clipboardController.copyItem(id)
                            else if (modelData.action === "pin") clipboardController.togglePin(id)
                            else clipboardController.removeItem(id)
                        }
                    }
                }
            }
        }
    }

    Item {
        id: confirmLayer; objectName: "clipboardConfirm"; anchors.fill: parent; z: 200; visible: false
        Rectangle { anchors.fill: parent; color: "#99000000" }
        MouseArea { anchors.fill: parent; onClicked: confirmLayer.visible = false }
        ClipboardConfirmBar {
            x: -15; y: 370; title: qsTr("清空剪贴板记录？")
            message: clipboardPage.clipboard.removable + qsTr(" 条记录 / 保留 ") + (clipboardPage.clipboard.total - clipboardPage.clipboard.removable) + qsTr(" 条固定记录")
            confirmTitle: qsTr("清空"); reduceMotion: clipboardPage.reduceMotion
            onCancelled: confirmLayer.visible = false
            onAccepted: { confirmLayer.visible = false; clipboardController.clearUnpinned() }
        }
    }
    Item {
        x: 320; y: 424; width: 270; height: 58; z: 300; opacity: clipboardPage.toastOpen ? 1 : 0; visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: clipboardPage.reduceMotion ? 0 : 250 } }
        Rectangle { anchors.fill: parent; color: "#F0181818"; border.color: Appearance.tone("#505050") }
        Rectangle { width: 4; height: parent.height; color: Appearance.accent("#FFFA00") }
        Text { x: 18; y: 10; width: 240; text: clipboardPage.toastTitle; color: Appearance.tone("#F2F2F2"); elide: Text.ElideRight; font.family: harmonyBold.name; font.pixelSize: 14; font.weight: Font.Bold; renderType: Text.CurveRendering }
        Text { x: 18; y: 33; width: 240; text: clipboardPage.toastDetail; color: Appearance.tone("#A0A0A0"); elide: Text.ElideRight; font.family: harmonyRegular.name; font.pointSize: 11.5 * 0.75; renderType: Text.CurveRendering }
    }
}
