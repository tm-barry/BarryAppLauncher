pragma Singleton

import QtCore

Settings {
    property string appImageDefaultLocation: StandardPaths.writableLocation(StandardPaths.HomeLocation) + "/Applications"
}
