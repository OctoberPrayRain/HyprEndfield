import QtQuick
import "Appearance.js" as Appearance
import QtQuick.Shapes
import HyprEndfield.Source 1.0
import "WatchSurfaces.js" as Source

Item {
    id: menu
    required property var shell
    property bool bootPending: false
    readonly property bool bootActive: bootPending || boot.playing
    property real afterBoot: 1
    Connections {
        target: appController
        function onHudOpenChanged() {
            if (appController.hudOpen) { menu.bootPending = settingsController.beginBootLog(); menu.afterBoot = 1; }
            else { menu.bootPending = false; boot.cancel(); }
        }
    }
    Connections {
        target: menu.shell
        function onTransitioningChanged() {
            if (!menu.shell.transitioning && menu.bootPending && appController.hudOpen) { boot.play(); menu.bootPending = false; }
        }
    }
    NumberAnimation { id: restoreCentre; target: menu; property: "afterBoot"; from: 0; to: 1; duration: 380; easing.type: Easing.BezierSpline; easing.bezierCurve: [.2, .8, .2, 1, 1, 1] }
    width: 1920
    height: 1080
    enabled: !shell.transitioning
    readonly property real leanX: Math.round(-Math.tan(shell.cameraYaw * Math.PI / 180) * 160) / 100
    readonly property real leanY: Math.round(Math.tan((shell.cameraPitch - 20) * Math.PI / 180) * 160) / 100
    SourceDial {
        width: 1920
        height: 1080
        z: -200
        animation: sourceWatchAnimation
        logo: settingsController.centerLogo
        pose: ({phase: menu.shell.animationPhase, time: menu.shell.sourceAnimationTime,
                ambient: menu.shell.sourceAmbientSampleTime, gyro: menu.shell.sourceGyro})
    }
    // Cables live on the deck, independently of the raised cards.
    Repeater {
        model: 4
        delegate: MenuPlane {
            required property int index
            scene: menu.shell
            elevation: 0
            opacity: menu.shell.navigationListValue("leftAlpha", index, 1)
            MenuArt {
                anchors.fill: parent
                asset: "card-" + index + "-links"
            }
        }
    }
    MenuPlane {
        scene: menu.shell
        elevation: 0
        opacity: menu.shell.baseContentFrameValue("nativeOpacity", 1) * (menu.bootActive ? 0 : Math.min(1, menu.afterBoot / .6))
        localMatrix: Qt.matrix4x4(1, 0, 0, 0, 0, 1, 0, 44 * (1 - menu.afterBoot), 0, 0, 1, 0, 0, 0, 0, 1)
        SystemPage {
            settings: settingsController
            x: 630
            y: 238
            scale: 1.12
            transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "system"
        }
        DisplayPage {
            x: 630
            y: 238
            scale: 1.12
            transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "display"
            settings: menu.shell.displaySettings
        }
        AboutPage {
            x: 630; y: 238; scale: 1.12; transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "about"
            settings: settingsController
        }
    }
    MenuPlane {
        scene: menu.shell
        elevation: 0
        opacity: menu.shell.baseContentFrameValue("nativeOpacity", 1) * (menu.bootActive ? 0 : Math.min(1, menu.afterBoot / .6))
        localMatrix: Qt.matrix4x4(1, 0, 0, 0, 0, 1, 0, 44 * (1 - menu.afterBoot), 0, 0, 1, 0, 0, 0, 0, 1)
        NowPlayingPage {
            x: 630
            y: 238
            scale: 1.12
            transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "nowPlaying"
            presented: visible && appController.hudVisible
            reduceMotion: menu.shell.displaySettings.reduceMotion
        }
        VolumePage {
            x: 630
            y: 238
            scale: 1.12
            transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "volume"
            presented: visible && appController.hudVisible
            reduceMotion: menu.shell.displaySettings.reduceMotion
        }
        TerminalPage {
            x: 630
            y: 238
            scale: 1.12
            transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "hotkeys"
            presented: visible && appController.hudOpen && !menu.shell.transitioning
            rasterScale: menu.shell.rasterScale * scale
        }
        ActivityPage {
            x: 630
            y: 238
            scale: 1.12
            transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "activity"
            presented: visible && appController.hudOpen
            reduceMotion: menu.shell.displaySettings.reduceMotion
        }
        StoragePage {
            x: 630; y: 238; scale: 1.12; transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "storage"
            presented: visible && appController.hudOpen
        }
        OrbiPomPage {
            x: 630; y: 238; scale: 1.12; transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "minigame"
            presented: visible && appController.hudOpen
        }
        EventLogPage {
            x: 630; y: 238; scale: 1.12; transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "eventLog"
            presented: visible && appController.hudOpen
            reduceMotion: menu.shell.displaySettings.reduceMotion
        }
        AppShortcutsPage {
            x: 630; y: 238; scale: 1.12; transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "addApp"
            presented: visible && appController.hudOpen
            reduceMotion: menu.shell.displaySettings.reduceMotion
        }
        ProfilePage {
            x: 630; y: 238; scale: 1.12; transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "profile"
            presented: visible && appController.hudOpen
            onAccountRequested: menu.shell.selectedModule = "account"
        }
        AccountPage {
            x: 630; y: 238; scale: 1.12; transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "account"
            presented: visible && appController.hudOpen
            reduceMotion: menu.shell.displaySettings.reduceMotion
        }
        ProjectionPage {
            x: 630; y: 238; scale: 1.12; transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "projection"
            workspace: menu.shell.projectionWorkspace
        }
        MediaAssemblyPage {
            x: 630; y: 238; scale: 1.12; transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "mediaAssembly"
            presented: visible && appController.hudOpen
        }
        CalendarPage {
            x: 630
            y: 238
            scale: 1.12
            transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "calendar"
            presented: visible && appController.hudOpen
            reduceMotion: menu.shell.displaySettings.reduceMotion
        }
        ClipboardPage {
            x: 630
            y: 238
            scale: 1.12
            transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "clipboard"
            presented: visible && appController.hudOpen
            reduceMotion: menu.shell.displaySettings.reduceMotion
        }
        NotesPage {
            x: 630
            y: 238
            scale: 1.12
            transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "notes"
            presented: visible && appController.hudOpen
            reduceMotion: menu.shell.displaySettings.reduceMotion
        }
        FileShelfPage {
            x: 630
            y: 238
            scale: 1.12
            transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "fileShelf"
            presented: visible && appController.hudOpen
            reduceMotion: menu.shell.displaySettings.reduceMotion
        }
        ArchivePage {
            x: 630
            y: 238
            scale: 1.12
            transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "archive"
            presented: visible && appController.hudOpen
            reduceMotion: menu.shell.displaySettings.reduceMotion
        }
        ReaderPage {
            x: 571
            y: 187
            scale: 1.32
            transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "reader"
            presented: visible && appController.hudOpen
            reduceMotion: menu.shell.displaySettings.reduceMotion
        }
        FocusModePage {
            x: 630
            y: 198
            scale: 1.12
            transformOrigin: Item.TopLeft
            visible: menu.shell.selectedModule === "workMode"
            presented: visible && appController.hudOpen
            reduceMotion: menu.shell.displaySettings.reduceMotion
        }
    }
    MenuPlane {
        scene: menu.shell
        elevation: 34
        Repeater {
            model: menu.shell.leftButtons
            delegate: MenuCard {
                required property int index
                required property var modelData
                cardIndex: index
                label: modelData.label
                glyph: modelData.key
                selected: menu.shell.selectedModule === modelData.key
                leanX: menu.leanX
                leanY: menu.leanY
                opacity: menu.shell.navigationListValue("leftAlpha", index, 1)
                onHoverEntered: menu.shell.soundEffects.playHover()
                onTriggered: { menu.shell.soundEffects.playClick(); menu.shell.selectedModule = modelData.key }
            }
        }
    }
    MenuPlane {
        scene: menu.shell
        elevation: 28
        Item {
            id: viewport
            x: 1290
            y: 200
            width: 512
            height: 805
            clip: true
            // This is the only flattened interactive surface, for the source fade mask.
            // Render above its physical output resolution, including monitor scaling.
            layer.enabled: true
            layer.smooth: true
            layer.samples: 4
            layer.textureSize: Qt.size(Math.ceil(width * menu.shell.rasterScale), Math.ceil(height * menu.shell.rasterScale))
            layer.effect: ShaderEffect {
                fragmentShader: "qrc:/shaders/viewport-mask.frag.qsb"
            }
            Repeater {
                model: Math.ceil(menu.shell.rightButtons.length / 2)
                delegate: Item {
                    id: row
                    required property int index
                    readonly property real angle: (22.52 - index * 15.5 + menu.shell.rightScrollOffset) * Math.PI / 180
                    x: 960 + 582.32 * Math.cos(angle) - viewport.x
                    y: 468 - 490 * angle - viewport.y
                    width: 235
                    height: 104
                    transform: Rotation {
                        origin.x: 0
                        origin.y: 52
                        angle: -3
                    }
                    Repeater {
                        model: Math.min(2, menu.shell.rightButtons.length - row.index * 2)
                        delegate: MenuTile {
                            required property int index
                            readonly property int slot: row.index * 2 + index
                            x: index * 128
                            entry: menu.shell.rightButtons[slot]
                            selected: entry.launchCategory ? appShortcutsController.launchCategory === entry.launchCategory : menu.shell.selectedModule === entry.key
                            secondColumn: index === 1
                            rowX: row.x
                            rowY: row.y
                            leanX: menu.leanX
                            leanY: menu.leanY
                            opacity: menu.shell.navigationListValue("rightAlpha", slot, 1)
                            onHoverEntered: menu.shell.soundEffects.playHover()
                            onTriggered: {
                            menu.shell.soundEffects.playClick()
                            if (entry.shortcutId) appShortcutsController.requestOpen(entry.shortcutId)
                            else if (entry.launchCategory) appShortcutsController.setLaunchCategory(entry.launchCategory)
                            else if (entry.backToMenu) menu.shell.selectedModule = menu.shell.previousModule
                            else {
                                if (entry.key === "addApp" && menu.shell.selectedModule !== "addApp")
                                    menu.shell.previousModule = menu.shell.selectedModule
                                menu.shell.selectedModule = entry.key
                            }
                        }
                            DropArea {
                                anchors.fill: parent
                                enabled: parent.entry.key === "fileShelf"
                                onEntered: drag => {
                                    drag.accepted = drag.hasUrls && drag.urls.every(url => String(url).startsWith("file:"))
                                }
                                onDropped: drop => {
                                    if (fileShelfController.addUrls(drop.urls)) {
                                        menu.shell.selectedModule = "fileShelf"
                                        drop.action = Qt.CopyAction
                                        drop.accepted = true
                                    }
                                }
                            }
                        }
                    }
                }
            }
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                onWheel: wheel => {
                    menu.shell.scrollRightList(wheel.angleDelta.y < 0 ? 1 : -1);
                    wheel.accepted = true;
                }
            }
        }
        Repeater {
            model: 2
            delegate: Item {
                required property int index
                x: index === 0 ? 1600 : 1500
                y: index === 0 ? 220 : 980
                width: 70
                height: 30
                opacity: menu.shell.contentFrameValue("nativeOpacity", 1) * ((index === 0 ? menu.shell.rightScrollOffset > 0 : menu.shell.rightScrollOffset < menu.shell.maximumRightScrollOffset) ? 0.82 : 0.18)
                rotation: -3
                Shape {
                    preferredRendererType: Shape.CurveRenderer
                    ShapePath {
                        strokeColor: Appearance.tone("#e5e5e5")
                        strokeWidth: 4
                        fillColor: "transparent"
                        PathSvg {
                            path: index === 0 ? "M20 22L35 8L50 22" : "M20 8L35 22L50 8"
                        }
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: menu.shell.scrollRightList(index === 0 ? -1 : 1)
                }
            }
        }
    }
    SourceSurface {
        shell: menu.shell
        spec: Source.data.shadow
        z: 25
        opacity: menu.shell.remainingContentFrameValue("storageOpacity", 1)
    }
    Repeater {
        model: Source.data.bottom
        delegate: SourceBottomButton {
            required property int index
            required property var modelData
            shell: menu.shell
            spec: modelData
            z: 26
            label: index === 0 ? qsTr("存储") : qsTr("活动监视器")
            opacity: menu.shell.remainingContentFrameValue(index === 0 ? "storageOpacity" : "activityOpacity", 1)
            onHoverEntered: menu.shell.soundEffects.playHover()
            onTriggered: { menu.shell.soundEffects.playClick(); menu.shell.selectedModule = index === 0 ? "storage" : "activity" }
        }
    }
    MenuPlane {
        scene: menu.shell
        elevation: 72
        opacity: menu.shell.baseContentFrameValue("nativeOpacity", 1)
        MenuClock {
            x: 1306.06
            y: 58.31
            timeText: menu.shell.clockText
            dateText: menu.shell.dateText
            seconds: menu.shell.clockSeconds
            focusActive: focusModeController.state.running
            styleIndex: settingsController.clockStyleIndex
            recordingMode: settingsController.recordingMode
            reduceMotion: settingsController.reduceMotion
            onStyleRequested: index => settingsController.set("clockStyleIndex", index)
            transform: Matrix4x4 {
                matrix: Qt.matrix4x4(.9995, .0739, 0, 0, -.0306, .9972, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)
            }
        }
    }
    MenuPlane {
        scene: menu.shell
        elevation: 64
        visible: typeof accountController === "undefined" || !accountController.gameCard
        opacity: menu.shell.contentFrameValue("identityOpacity", 1)
        MenuBadge {
            x: 289
            y: 850
            leanX: menu.leanX
            leanY: menu.leanY
            onHoverEntered: menu.shell.soundEffects.playHover()
            onClicked: { menu.shell.soundEffects.playClick(); menu.shell.selectedModule = "profile" }
        }
    }
    GameProfileBadge {
        shell:menu.shell; z:64
        visible:typeof accountController !== "undefined" && accountController.gameCard
        opacity:menu.shell.contentFrameValue("identityOpacity",1)
        onHoverEntered:menu.shell.soundEffects.playHover()
        onClicked:{ menu.shell.soundEffects.playClick(); menu.shell.selectedModule="profile" }
    }
    MenuPlane {
        scene:menu.shell; elevation:0; z:90
        opacity:menu.shell.baseContentFrameValue("nativeOpacity",1)
        // Original (270,2) header + (292,0) wallet, in the shared native plane.
        AccountGauge {
            x:960+(562-500)*1.12; y:540+(2-320)*1.12; scale:1.12; transformOrigin:Item.TopLeft
            presented:appController.hudOpen && !menu.shell.transitioning
            reduceMotion:menu.shell.displaySettings.reduceMotion
        }
    }
    MenuPlane {
        scene: menu.shell
        elevation: 34
        opacity: menu.shell.contentFrameValue("closeOpacity", 1)
        Item {
            x: 202
            y: 875.5
            width: 53
            height: 53
            transform: Matrix4x4 {
                matrix: Qt.matrix4x4(1, -.02, 0, 0, -.029, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)
            }
            MenuArt {
                anchors.fill: parent
                asset: "power-plate"
            }
            HudGlyph {
                x: 6
                y: 6
                width: 41
                height: 41
                kind: "power"
                ink: "#c7edede8"
            }
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
            }
        }
    }
    MenuPlane {
        scene: menu.shell
        elevation: 144
        opacity: menu.shell.contentFrameValue("nativeOpacity", 1)
        Text {
            x: 620
            y: 72
            text: "HYPRENDFIELD"
            font.family: harmonyBold.name
            font.pixelSize: 32
            color: Appearance.tone("#f2f2f2")
            renderType: Text.CurveRendering
        }
        FacilityTags { x: 340; y: 162; visible: appController.hudOpen && !menu.bootActive; onOpenModule: module => menu.shell.selectedModule = module }
        Text {
            x: 620
            y: 114
            text: "SYSTEM INTERFACE"
            font.family: harmonyMedium.name
            font.pixelSize: 14
            font.letterSpacing: 1.4
            color: Appearance.tone("#8f9496")
            renderType: Text.CurveRendering
        }
    }
    MenuPlane {
        scene: menu.shell
        elevation: 0
        z: 30
        opacity: menu.shell.baseContentFrameValue("nativeOpacity", 1)
        BatteryBadge {
            // HUDChargeBadge center=(500,446), source offset=38. Convert its
            // design units with the same scale as the existing 590px centre.
            readonly property real sourceToStage: 1.12 / (1470 / (1920 * 1.15))
            scale: .82 * sourceToStage
            x: 960.4 - width * scale / 2
            y: 579.6 + 164 * sourceToStage - height * scale / 2
            transformOrigin: Item.TopLeft
            presented: appController.hudOpen && !menu.shell.transitioning
            reduceMotion: menu.shell.displaySettings.reduceMotion
        }
    }
    MenuPlane {
        scene: menu.shell
        elevation: 34
        z: 500
        opacity: menu.shell.baseContentFrameValue("nativeOpacity", 1)
        NotesWorkspace {
            selectedModule: menu.shell.selectedModule
            presented: appController.hudOpen
            reduceMotion: menu.shell.displaySettings.reduceMotion
        }
    }
    Text {
        x: 760
        y: 1120
        width: 400
        height: 30
        text: "SUPER + ESC TO CLOSE"
        color: Appearance.tone("#8a8d8e")
        font.family: harmonyRegular.name
        font.pixelSize: 14
        horizontalAlignment: Text.AlignHCenter
        renderType: Text.CurveRendering
        opacity: menu.shell.contentFrameValue("nativeOpacity", 1)
    }
    MenuPlane {
        scene: menu.shell; elevation: 0; z: 1100
        BootLog { id: boot; x: 641.2; y: 271.6; scale: 1.12; transformOrigin: Item.TopLeft; onFinished: { if (settingsController.reduceMotion) menu.afterBoot = 1; else restoreCentre.restart(); } }
    }
    MenuPlane {
        scene: menu.shell; elevation: 0; z: 1200
        Rectangle {
            x: 990; y: 745; width: 302.4; height: 65; visible: settingsController.notice !== "" && appController.hudOpen; color: "#F21A1A1A"; border.color: Appearance.tone("#444444")
            Rectangle { width: 3; height: parent.height; color: Appearance.accent("#FFFA00") }
            Text { x: 20; y: 10; width: 270; height: 45; text: settingsController.notice; wrapMode: Text.Wrap; maximumLineCount: 3; elide: Text.ElideRight; color: Appearance.tone("#F2F2F2"); font.family: harmonyMedium.name; font.pixelSize: 13; renderType: Text.CurveRendering }
            MouseArea { anchors.fill: parent; onClicked: settingsController.clearNotice() }
        }
        Timer { id: noticeTimer; interval: 6000; onTriggered: settingsController.clearNotice() }
        Connections { target: settingsController; function onNoticeChanged() { if (settingsController.notice) noticeTimer.restart() } }
    }
}
