#ifndef NETWORKUTIL_H
#define NETWORKUTIL_H

#include <QNetworkAccessManager>

class NetworkUtil
{
public:
    static QNetworkAccessManager* networkManager() {
        static QNetworkAccessManager manager;
        return &manager;
    }
};

#endif // NETWORKUTIL_H
