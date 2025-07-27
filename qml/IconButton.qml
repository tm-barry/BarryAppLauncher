import QtQuick
import QtQuick.Controls

Button {
    font.family: fontAwesome.name

    FontLoader {
        id: fontAwesome
        source: "/assets/fonts/fa-6-solid-900.otf"
    }
}
