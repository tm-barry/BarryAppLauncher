import QtQuick

QtObject {
    function formatBytes(bytes) {
        if (bytes < 0) return "";

        const gb = 1024 * 1024 * 1024;
        const mb = 1024 * 1024;
        const kb = 1024;

        if (bytes >= gb) {
            return (bytes / gb).toFixed(2) + " GB";
        } else if (bytes >= mb) {
            return (bytes / mb).toFixed(1) + " MB";
        } else if (bytes >= kb) {
            return (bytes / kb).toFixed(0) + " KB";
        } else {
            return bytes + " B";
        }
    }

    function getUpdateDownloadText(received, total) {
        if (total <= 0) {
            return formatBytes(received);
        }

        const gb = 1024 * 1024 * 1024;
        const mb = 1024 * 1024;
        const kb = 1024;

        let unit = "B";
        let divisor = 1;
        let precision = 0;

        if (total >= gb) { unit = "GB"; divisor = gb; precision = 2; }
        else if (total >= mb) { unit = "MB"; divisor = mb; precision = 1; }
        else if (total >= kb) { unit = "KB"; divisor = kb; precision = 0; }

        const receivedFormatted = (received / divisor).toFixed(precision);
        const totalFormatted = (total / divisor).toFixed(precision);

        return receivedFormatted + " " + unit + " / " + totalFormatted + " " + unit;
    }

    function getUpdateProgressText(state) {
        switch (state) {
            case AppImageMetadata.Downloading:
                return qsTr("Downloading...");
            case AppImageMetadata.Extracting:
                return qsTr("Extracting...");
            case AppImageMetadata.Installing:
                return qsTr("Installing...");
            case AppImageMetadata.Success:
                return qsTr("Success");
            case AppImageMetadata.Failed:
                return qsTr("Failed");
            default:
                return qsTr("Waiting...");
        }
    }
}
