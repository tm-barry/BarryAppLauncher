pragma Singleton

import QtCore

Settings {
    enum AppImageFileOperation {
        Move,
        Copy
    }

    property string appImageDefaultLocation: StandardPaths.writableLocation(
                                                 StandardPaths.HomeLocation) + "/Applications"
    property int appImageFileOperation: AppImageFileOperation.Move
}
