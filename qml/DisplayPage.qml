import QtQuick
import QtQuick.Dialogs
import "Appearance.js" as Appearance

Item {
    id: root

    required property var settings
    property string section: "main"
    property bool confirmFacility: false
    property string openMenu: ""
    property var menuItems: []
    property real menuX: 0
    property real menuY: 0
    property bool bubbleShown: false
    property string bubbleText: ""
    property real bubbleX: 0
    property real bubbleY: 0
    property bool scrollBarShown: false
    onVisibleChanged: if (!visible) { openMenu = ""; confirmFacility = false; logoPicker.close(); colorPicker.close(); }
    onConfirmFacilityChanged: if (confirmFacility) forceActiveFocus()
    onOpenMenuChanged: if (openMenu) forceActiveFocus()
    Keys.onEscapePressed: event => {
        if (confirmFacility || openMenu || settings.layoutRemaining) { confirmFacility = false; openMenu = ""; settings.revertLayout(); event.accepted = true; }
        else event.accepted = false;
    }
    Connections { target: root.settings; function onLayoutChanged() { if (root.visible && root.settings.layoutRemaining) root.forceActiveFocus(); } }

    signal pageRequested(string page)

    readonly property var rows: [
        {
            key: "uiScale",
            title: qsTr("界面缩放"),
            mode: "slider",
            minimum: 0.2,
            maximum: 2,
            step: 0.01,
            format: "scale"
        },
        {
            key: "positionX",
            title: qsTr("界面位置 X"),
            mode: "slider",
            minimum: -50,
            maximum: 50,
            step: 1,
            format: "signedPercent",
            zero: 0
        },
        {
            key: "positionY",
            title: qsTr("界面位置 Y"),
            mode: "slider",
            minimum: -50,
            maximum: 50,
            step: 1,
            format: "signedPercent",
            zero: 0
        },
        {
            key: "parallax",
            title: qsTr("视差强度"),
            mode: "slider",
            minimum: 0,
            maximum: 200,
            step: 1,
            format: "percent"
        },
        {
            key: "perspective",
            title: qsTr("透视强度"),
            mode: "slider",
            minimum: 0,
            maximum: 200,
            step: 1,
            format: "percent"
        },
        {
            key: "brightness",
            title: qsTr("背景亮度"),
            mode: "slider",
            minimum: 0,
            maximum: 100,
            step: 1,
            format: "percent"
        },
        {
            key: "blur",
            title: qsTr("模糊程度"),
            mode: "slider",
            minimum: 0,
            maximum: 100,
            step: 1,
            format: "percent"
        },
        {
            key: "theme",
            title: qsTr("主题"),
            mode: "dropdown"
        },
        {
            key: "clockFormat",
            title: qsTr("时间格式"),
            mode: "segmented"
        },
        {
            key: "clockStyle",
            title: qsTr("时钟样式"),
            mode: "dropdown"
        },
        {
            key: "centerLogo",
            title: qsTr("中心标志"),
            mode: "dropdown"
        },
        {
            key: "appIcon",
            title: qsTr("应用 / 菜单栏图标"),
            mode: "nav"
        },
        {
            key: "palette",
            title: qsTr("主题颜色"),
            mode: "swatches"
        },
        {
            key: "battery",
            title: qsTr("电池提醒"),
            mode: "nav",
            chip: qsTr("电池")
        },
        {
            key: "ambient",
            title: qsTr("持续环境动画"),
            mode: "toggle"
        },
        {
            key: "reduceMotion",
            title: qsTr("减少动态效果"),
            mode: "toggle"
        },
        {
            key: "lowPower",
            title: qsTr("低功耗视觉模式"),
            mode: "toggle"
        },
        {
            key: "recording",
            title: qsTr("录制模式"),
            mode: "toggle"
        },
        {
            key: "bootLog",
            title: qsTr("每日启动日志"),
            mode: "toggle"
        },
        {
            key: "facility",
            title: qsTr("设施警报"),
            mode: "toggle"
        }
    ]

    readonly property var themes: [qsTr("深色"), qsTr("浅色"), qsTr("跟随系统")]
    readonly property var clockStyles: [qsTr("01  数字"), qsTr("02  分栏"), qsTr("03  表盘"), qsTr("04  轨道"), qsTr("05  堆叠")]
    readonly property var centerLogos: ["Endfield", "Rhodes Island", "Babel", "Rhine Lab", qsTr("自定")]

    width: 590
    height: 510

    function sliderValue(key) {
        switch (key) {
        case "uiScale":
            return settings.uiScale;
        case "positionX":
            return settings.hudOffsetX * 100;
        case "positionY":
            return settings.hudOffsetY * 100;
        case "parallax":
            return settings.parallaxIntensity * 100;
        case "perspective":
            return settings.perspectiveIntensity * 100;
        case "brightness":
            return settings.backgroundBrightness * 100;
        case "blur":
            return settings.blurAmount * 100;
        }
        return 0;
    }

    function writeSlider(key, value, final) {
        if (["uiScale", "positionX", "positionY"].includes(key)) {
            if (final) settings.previewLayout(key === "positionX" ? "hudOffsetX" : key === "positionY" ? "hudOffsetY" : key, key === "uiScale" ? value : value / 100);
            return;
        }
        switch (key) {
        case "uiScale":
            settings.uiScale = value;
            break;
        case "positionX":
            settings.hudOffsetX = value / 100;
            break;
        case "positionY":
            settings.hudOffsetY = value / 100;
            break;
        case "parallax":
            settings.parallaxIntensity = value / 100;
            break;
        case "perspective":
            settings.perspectiveIntensity = value / 100;
            break;
        case "brightness":
            settings.backgroundBrightness = value / 100;
            break;
        case "blur":
            settings.blurAmount = value / 100;
            break;
        }
    }

    function toggleValue(key) {
        switch (key) {
        case "ambient":
            return settings.ambientAnimation;
        case "reduceMotion":
            return settings.reduceMotion;
        case "lowPower":
            return settings.lowPowerVisualMode;
        case "recording":
            return settings.recordingMode;
        case "bootLog":
            return settings.dailyBootLog;
        case "facility":
            return settings.facilityAlerts;
        }
        return false;
    }

    function writeToggle(key, value) {
        switch (key) {
        case "ambient":
            settings.ambientAnimation = value;
            break;
        case "reduceMotion":
            settings.reduceMotion = value;
            break;
        case "lowPower":
            settings.lowPowerVisualMode = value;
            break;
        case "recording":
            settings.recordingMode = value;
            break;
        case "bootLog":
            settings.dailyBootLog = value;
            break;
        case "facility":
            if (!value) confirmFacility = true;
            else settings.facilityAlerts = true;
            break;
        }
    }

    function dropdownValue(key) {
        if (key === "theme")
            return themes[settings.themeIndex];
        if (key === "clockStyle")
            return clockStyles[settings.clockStyleIndex];
        if (key === "centerLogo")
            return centerLogos[settings.centerLogoIndex];
        return "";
    }

    function optionsFor(key) {
        if (key === "theme")
            return themes;
        if (key === "clockStyle")
            return clockStyles;
        if (key === "centerLogo")
            return centerLogos;
        return [];
    }

    function selectedOption(key) {
        if (key === "theme")
            return settings.themeIndex;
        if (key === "clockStyle")
            return settings.clockStyleIndex;
        if (key === "centerLogo")
            return settings.centerLogoIndex;
        return 0;
    }

    function pickOption(index) {
        if (openMenu === "theme")
            settings.themeIndex = index;
        else if (openMenu === "clockStyle")
            settings.clockStyleIndex = index;
        else if (openMenu === "centerLogo") {
            if (index === 4) logoPicker.open();
            else settings.centerLogoIndex = index;
        }
        openMenu = "";
    }

    function activate(key, point) {
        if (key === "appIcon" || key === "battery") {
            section = key;
            return;
        }
        const options = optionsFor(key);
        if (!options.length)
            return;
        if (openMenu === key) {
            openMenu = "";
            return;
        }
        menuItems = options;
        menuX = Math.min(width - (key === "centerLogo" ? 264 : 178), point.x);
        const menuHeight = key === "centerLogo" ? 142 : options.length * 30 + 8;
        const below = point.y + 34;
        menuY = below + menuHeight > height + 20 ? point.y - 4 - menuHeight : below;
        openMenu = key;
    }

    function showBubble(shown, text, x, y) {
        bubbleShown = shown;
        bubbleText = text;
        bubbleX = x;
        bubbleY = y;
    }

    Item {
    anchors.fill: parent
    visible: root.section === "main"
    Text {
        renderType: Text.CurveRendering
        x: 0
        y: 0
        height: 14
        text: "// HUD  /  "
        color: Appearance.tone("#8A8A8A")
        font.family: harmonyRegular.name
        font.pixelSize: 11
        font.letterSpacing: 0.06
        verticalAlignment: Text.AlignVCenter
    }

    Text {
        renderType: Text.CurveRendering
        x: 69
        y: 0
        height: 14
        text: "DISPLAY"
        color: Appearance.tone("#E6E6E6")
        font.family: harmonyRegular.name
        font.pixelSize: 11
        font.letterSpacing: 0.06
        verticalAlignment: Text.AlignVCenter
    }

    Text {
        renderType: Text.CurveRendering
        x: 500
        y: 0
        width: 49
        height: 14
        text: "SYS-02"
        color: Appearance.tone("#8A8A8A")
        font.family: harmonyRegular.name
        font.pixelSize: 11
        font.letterSpacing: 0.06
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
    }

    Item {
        x: 556
        y: 3.5
        width: 34
        height: 7
        clip: true

        Repeater {
            model: 12
            Rectangle {
                required property int index
                x: index * 3.5 - 7
                y: -5
                width: 1
                height: 18
                rotation: 45
                color: Appearance.tone("#777777")
            }
        }
    }

    Text {
        renderType: Text.CurveRendering
        x: 0
        y: 18
        id: displayTitle
        width: 140
        height: 34
        text: qsTr("显示").toUpperCase()
        color: Appearance.tone("#F4F4F4")
        font.family: harmonyBold.name
        font.pixelSize: 26
        font.weight: Font.Bold
        font.letterSpacing: 0.08
        verticalAlignment: Text.AlignVCenter
    }

    Text {
        renderType: Text.CurveRendering
        x: displayTitle.implicitWidth + 12
        y: 21
        width: 100
        height: 29
        text: "DISPLAY"
        visible: displayTitle.text !== "DISPLAY"
        color: Appearance.tone("#8A8A8A")
        font.family: harmonyMedium.name
        font.pixelSize: 13
        font.weight: Font.Medium
        font.letterSpacing: 0.2
        verticalAlignment: Text.AlignVCenter
    }

    Rectangle {
        x: 0
        y: 64
        width: 590
        height: 1
        color: Appearance.tone("#38FFFFFF")
    }
    Rectangle {
        x: 0
        y: 63
        width: 56
        height: 3
        color: Appearance.accent("#FFFA00")
    }

    Repeater {
        model: 11
        Rectangle {
            required property int index
            x: 454 + index * 9
            y: 60
            width: 1
            height: 9
            color: Appearance.tone("#59FFFFFF")
        }
    }

    Rectangle {
        x: 563
        y: 58
        width: 27
        height: 12
        color: "#151515"
        Text {
            renderType: Text.CurveRendering
            anchors.fill: parent
            text: "20"
            color: Appearance.accent("#FFFA00")
            font.family: harmonyBold.name
            font.pixelSize: 11
            font.weight: Font.Bold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    Flickable {
        id: rowsViewport
        x: 0
        y: 81
        width: 590
        height: 416
        clip: true
        contentWidth: 590
        contentHeight: root.rows.length * 50 + (root.rows.length - 1) * 6
        boundsBehavior: Flickable.StopAtBounds
        flickDeceleration: 3200

        onContentYChanged: {
            root.scrollBarShown = true;
            scrollHideTimer.restart();
            root.openMenu = "";
            root.bubbleShown = false;
        }

        Repeater {
            model: root.rows

            delegate: HudSettingsRow {
                required property int index
                required property var modelData
                x: 0
                y: index * 56
                width: 580
                title: modelData.title
                enabled: modelData.key !== "blur"
                sublabel: modelData.key === "blur" ? qsTr("暂不支持单独调整，沿用合成器设置") : modelData.key === "facility" ? qsTr("监测磁盘、内存和硬件温度") : modelData.key === "lowPower" ? qsTr("表盘动画 30 Hz，关闭模糊与持续环境动画") : modelData.key === "recording" ? qsTr("隐藏便笺、剪贴板和文件名称等隐私内容") : ""
                mode: modelData.mode
                value: root.dropdownValue(modelData.key)
                artwork: modelData.key === "centerLogo" ? "image://settings-logos/" + (root.settings.centerLogoIndex === 4 ? "custom/" + encodeURIComponent(root.settings.customLogo) : root.settings.centerLogoIndex) : ""
                switched: root.toggleValue(modelData.key)
                dropdownOpened: root.openMenu === modelData.key
                chipText: modelData.chip || ""
                sliderValue: root.sliderValue(modelData.key)
                sliderMinimum: modelData.minimum === undefined ? 0 : modelData.minimum
                sliderMaximum: modelData.maximum === undefined ? 100 : modelData.maximum
                sliderStep: modelData.step === undefined ? 1 : modelData.step
                sliderZero: modelData.zero === undefined ? Number.NaN : modelData.zero
                sliderFormat: modelData.format || "percent"
                sliderBubbleHost: bubbleLayer
                segmentIndex: root.settings.clock24Hour ? 0 : 1
                swatchIndex: root.settings.accentIndex

                onActivated: root.activate(modelData.key, mapToItem(root, 400, 10))
                onToggleRequested: value => root.writeToggle(modelData.key, value)
                onSliderMoved: (value, final) => root.writeSlider(modelData.key, value, final)
                onSliderBubbleRequested: (shown, text, x, y) => root.showBubble(shown, text, x, y)
                onSegmentRequested: index => root.settings.clock24Hour = index === 0
                onSwatchRequested: index => { if (index === 5) colorPicker.open(); else root.settings.accentIndex = index }
            }
        }

        Rectangle {
            id: scrollTrack
            x: 586
            y: rowsViewport.contentY
            width: 4
            height: rowsViewport.height
            color: Appearance.tone("#222222")
            opacity: root.scrollBarShown || scrollBarHover.hovered ? 1 : 0

            Behavior on opacity {
                NumberAnimation {
                    duration: opacity > 0 ? 150 : 300
                }
            }

            Rectangle {
                id: scrollThumb
                x: 0
                y: rowsViewport.visibleArea.yPosition * parent.height
                width: 4
                height: Math.max(18, rowsViewport.visibleArea.heightRatio * parent.height)
                color: Appearance.accent("#D7BC18")
            }

            HoverHandler {
                id: scrollBarHover
            }
            MouseArea {
                anchors.fill: parent; anchors.leftMargin: -4; anchors.rightMargin: -4; preventStealing: true
                property real grab: 0
                function move(y) { const fraction = Math.max(0, Math.min(1, (y - grab) / Math.max(1, height - scrollThumb.height))); rowsViewport.contentY = fraction * Math.max(0, rowsViewport.contentHeight - rowsViewport.height); }
                onPressed: mouse => { grab = mouse.y >= scrollThumb.y && mouse.y <= scrollThumb.y + scrollThumb.height ? mouse.y - scrollThumb.y : scrollThumb.height / 2; move(mouse.y); }
                onPositionChanged: mouse => { if (pressed) move(mouse.y); }
            }
        }
    }

    Timer {
        id: scrollHideTimer
        interval: 1000
        repeat: false
        onTriggered: root.scrollBarShown = false
    }

    MouseArea {
        anchors.fill: parent
        visible: root.openMenu.length > 0
        z: 20
        onClicked: root.openMenu = ""
    }

    Item {
        id: menuPanel
        visible: root.openMenu.length > 0 && root.openMenu !== "centerLogo"
        x: root.menuX
        y: root.menuY
        width: 178
        height: root.menuItems.length * 30 + 8
        z: 21

        Rectangle {
            anchors.fill: parent
            color: Appearance.tone("#1A1A1A")
            border.width: 1
            border.color: Appearance.tone("#3D3D3D")
        }
        Rectangle {
            x: 0
            y: 0
            width: 10
            height: 2
            color: Appearance.accent("#D7BC18")
        }
        Rectangle {
            x: 0
            y: 0
            width: 2
            height: 10
            color: Appearance.accent("#D7BC18")
        }

        Repeater {
            model: root.menuItems

            delegate: Item {
                required property int index
                required property string modelData
                x: 4
                y: 4 + index * 30
                width: 170
                height: 30

                Rectangle {
                    anchors.fill: parent
                    color: menuPointer.containsMouse ? Appearance.tone("#D3D4CF") : "transparent"
                }
                Rectangle {
                    x: 9
                    y: 11
                    width: 8
                    height: 8
                    color: index === root.selectedOption(root.openMenu) ? Appearance.accent("#FFFA00") : "transparent"
                    border.width: index === root.selectedOption(root.openMenu) ? 0 : 1
                    border.color: Appearance.tone("#555555")
                }
                Rectangle {
                    visible: menuPointer.containsMouse
                    x: 0
                    y: 0
                    width: 3
                    height: 30
                    color: Appearance.tone("#111111")
                }
                Text {
                    renderType: Text.CurveRendering
                    x: 28
                    y: 0
                    width: 132
                    height: 30
                    text: modelData
                    color: menuPointer.containsMouse ? Appearance.tone("#111111") : Appearance.tone("#D9D9D9")
                    font.family: harmonyRegular.name
                    font.pixelSize: 13
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                MouseArea {
                    id: menuPointer
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.pickOption(index)
                }
            }
        }
    }

    Rectangle {
        x: root.menuX; y: root.menuY; width: 264; height: 142; visible: root.openMenu === "centerLogo"; z: 21
        color: Appearance.tone("#1A1A1A"); border.color: Appearance.tone("#3D3D3D")
        Repeater {
            model: 4
            Rectangle {
                required property int index
                x: 8 + (index % 2) * 128; y: 8 + Math.floor(index / 2) * 52; width: 120; height: 44
                color: Appearance.tone("#212121"); border.color: Appearance.tone("#4B4B4B")
                scale: pointer.pressed ? .965 : 1
                Behavior on scale { NumberAnimation { duration: root.settings.reduceMotion ? 0 : 120 } }
                Image { x: 8; y: 8; width: 104; height: 28; source: "image://settings-logos/" + index; fillMode: Image.PreserveAspectFit; mipmap: true }
                Rectangle { x: -3; y: -3; width: 126; height: 50; color: "transparent"; border.width: 1.5; border.color: index === root.settings.centerLogoIndex ? Appearance.accent("#FFFA00") : Appearance.tone("#D9D9D9"); opacity: index === root.settings.centerLogoIndex ? 1 : pointer.containsMouse ? .7 : 0 }
                MouseArea { id: pointer; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.pickOption(index) }
            }
        }
        Text { x: 16; y: 113; height: 25; text: qsTr("选择自定义图标"); color: Appearance.tone("#D9D9D9"); font.family: harmonyRegular.name; font.pointSize: 12.5 * 0.75; renderType: Text.CurveRendering }
        MouseArea { x: 4; y: 108; width: 256; height: 30; cursorShape: Qt.PointingHandCursor; onClicked: root.pickOption(4) }
    }
    Item {
        id: bubbleLayer
        anchors.fill: parent
        z: 40

        Item {
            visible: root.bubbleShown
            x: root.bubbleX - width / 2
            y: root.bubbleY - height - 6
            width: bubbleLabel.implicitWidth + 16
            height: 25
            opacity: root.bubbleShown ? 1 : 0
            scale: root.bubbleShown ? 1 : 0.9

            Behavior on opacity {
                NumberAnimation {
                    duration: 140
                }
            }
            Behavior on scale {
                NumberAnimation {
                    duration: 180
                    easing.type: Easing.OutBack
                }
            }

            Rectangle {
                x: 0
                y: 0
                width: parent.width
                height: 21
                radius: 10
                color: Appearance.tone("#FAFAFA")
            }
            Text {
                id: bubbleLabel
                renderType: Text.CurveRendering
                x: 8
                y: 0
                height: 21
                text: root.bubbleText
                color: Appearance.tone("#111111")
                font.family: harmonyBold.name
                font.pixelSize: 11
                font.weight: Font.Bold
                verticalAlignment: Text.AlignVCenter
            }
            Canvas {
                x: parent.width / 2 - 4
                y: 21
                width: 8
                height: 4
                onPaint: {
                    const context = getContext("2d");
                    context.reset();
                    context.beginPath();
                    context.moveTo(0, 0);
                    context.lineTo(8, 0);
                    context.lineTo(4, 4);
                    context.closePath();
                    context.fillStyle = Appearance.tone("#FAFAFA");
                    context.fill();
                }
            }
        }
    }
    }
    DisplayExtras {
        anchors.fill: parent; visible: root.section !== "main"; section: root.section; settings: root.settings
        onBackRequested: root.section = "main"
    }
    FileDialog {
        id: logoPicker; title: qsTr("中心标志"); fileMode: FileDialog.OpenFile
        options: FileDialog.DontUseNativeDialog; nameFilters: [qsTr("图片") + " (*.png *.jpg *.jpeg *.webp *.bmp *.svg)"]
        onAccepted: root.settings.importLogo(selectedFile)
    }
    ColorDialog {
        id: colorPicker; title: qsTr("主题颜色"); selectedColor: root.settings.accent
        options: ColorDialog.DontUseNativeDialog
        onAccepted: { root.settings.set("customAccent", selectedColor.toString()); root.settings.set("accentIndex", 5) }
    }
    MouseArea { anchors.fill: parent; z: 24; visible: root.settings.layoutRemaining > 0 || root.confirmFacility; onClicked: { root.settings.revertLayout(); root.confirmFacility = false } }
    ClipboardConfirmBar {
        x: -15; y: 200; z: 25; visible: root.settings.layoutRemaining > 0 || root.confirmFacility
        title: root.confirmFacility ? qsTr("是否关闭全舰防御系统？") : qsTr("保留此界面布局？") + "  " + root.settings.layoutRemaining
        message: root.confirmFacility ? "" : qsTr("倒计时结束后恢复原来的缩放与位置。")
        confirmTitle: root.confirmFacility ? qsTr("关闭全舰防御系统") : qsTr("保留")
        cancelTitle: root.confirmFacility ? qsTr("关闭全舰防御系统") : qsTr("恢复")
        confirmGlyph: "check"; accentColor: root.confirmFacility ? "#E5383B" : Appearance.accent("#FFFA00")
        reduceMotion: root.settings.reduceMotion
        onAccepted: { if (root.confirmFacility) root.settings.set("facilityAlerts", false); else root.settings.confirmLayout(); root.confirmFacility = false }
        onCancelled: { if (root.confirmFacility) root.settings.set("facilityAlerts", false); else root.settings.revertLayout(); root.confirmFacility = false }
    }
}
