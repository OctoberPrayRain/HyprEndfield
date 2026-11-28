import QtQuick
import "Appearance.js" as Appearance
import QtQuick.Shapes

// EndfieldHUD/Sources/HUDNavigation.swift, iconPath(for:).
// Preserve the authored 32 px silhouettes and even-odd holes as vector paths.
Item {
    id: root
    property string kind: ""
    property color ink: Appearance.tone("#202020")
    function box(x, y, w, h) {
        return "M" + x + " " + y + "h" + w + "v" + h + "h" + (-w) + "Z";
    }
    function roundBox(x, y, w, h, r) {
        return "M" + (x + r) + " " + y + "h" + (w - 2 * r) + "a" + r + " " + r + " 0 0 1 " + r + " " + r + "v" + (h - 2 * r) + "a" + r + " " + r + " 0 0 1 " + (-r) + " " + r + "h" + (2 * r - w) + "a" + r + " " + r + " 0 0 1 " + (-r) + " " + (-r) + "v" + (2 * r - h) + "a" + r + " " + r + " 0 0 1 " + r + " " + (-r) + "Z";
    }
    function circle(r) {
        return "M" + (16 - r) + " 16a" + r + " " + r + " 0 1 0 " + 2 * r + " 0a" + r + " " + r + " 0 1 0 " + (-2 * r) + " 0Z";
    }
    function silhouette() {
        switch (kind) {
        case "system":
            {
                let p = box(7, 7, 18, 18) + box(12, 12, 8, 8);
                for (const v of [9, 19])
                    p += box(v, 1, 4, 5) + box(v, 26, 4, 5) + box(1, v, 5, 4) + box(26, v, 5, 4);
                return p;
            }
        case "display":
            return roundBox(2, 4, 28, 20, 2) + box(6, 8, 20, 12) + box(13, 25, 6, 2) + box(8, 28, 16, 3);
        case "hotkeys":
            {
                let p = roundBox(1, 6, 30, 21, 2) + box(6, 19, 20, 3);
                for (const x of [5, 11, 17, 23])
                    p += box(x, 10, 4, 4);
                return p;
            }
        case "about":
            return circle(14) + box(14, 7, 4, 4) + box(14, 14, 4, 11);
        case "account":
            return "M10 8a6 6 0 1 0 12 0a6 6 0 1 0 -12 0Z" + box(4, 18, 24, 12);
        case "nowPlaying":
            return circle(14) + circle(11) + "M12 9L24 16L12 23Z";
        case "volume":
            return "M2 12H8L17 5V27L8 20H2Z";
        case "storage":
            return box(12, 2, 8, 8) + box(14.5, 11, 3, 4) + box(5, 15, 22, 3) + box(5, 19, 2, 3) + box(15, 19, 2, 3) + box(25, 19, 2, 3) + box(2, 23, 8, 7) + box(12, 23, 8, 7) + box(22, 23, 8, 7);
        case "activity":
            return box(3, 23, 5, 7) + box(13, 18, 5, 12) + box(23, 12, 5, 18);
        case "power":
            return box(14, 1, 4, 15);
        case "back":
            return "M13 4L3 16L13 28V20H29V12H13Z";
        case "addApp":
            return roundBox(3, 3, 26, 26, 3) + box(7, 7, 18, 18) + "M14 10H18V14H22V18H18V22H14V18H10V14H14Z";
        }
        return "";
    }
    Item {
        width: 32
        height: 32
        transform: Scale {
            xScale: root.width / 32
            yScale: root.height / 32
        }
        Shape {
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                fillRule: ShapePath.OddEvenFill
                strokeWidth: -1
                fillColor: root.ink
                PathSvg {
                    path: root.silhouette()
                }
            }
            ShapePath {
                fillColor: "transparent"
                strokeColor: root.ink
                strokeWidth: root.kind === "power" ? 4.2 : root.kind === "activity" ? 2.8 : 3
                capStyle: ShapePath.FlatCap
                joinStyle: ShapePath.MiterJoin
                PathSvg {
                    path: root.kind === "volume" ? "M21.9497 11.0503A7 7 0 0 1 21.9497 20.9497M25.4853 7.5147A12 12 0 0 1 25.4853 24.4853" : root.kind === "activity" ? "M3 17L12 8L19 13L28 2M20 2H28V10" : root.kind === "power" ? "M23.0117 8.5244A11 11 0 1 1 8.9883 8.5244" : ""
                }
            }
        }
    }
}
