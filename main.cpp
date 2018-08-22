#include <QCoreApplication>
#include <modifiedfilesystemwatcher.h>
#include <QTimer>
#include <QTextCodec>
#include <QDebug>

#define ONE_MINUTE 60000
#define ARG_SIZE 6

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    if (a.arguments().size() == ARG_SIZE)
    {
        DbFileWatcher* db = new DbFileWatcher();
        DbFileSystemWatcher* watcher = new DbFileSystemWatcher(db, &a);

        ConnectionData connData;
        connData.dbName = a.arguments().at(1);
        connData.host = a.arguments().at(2);
        connData.port = a.arguments().at(3).toInt();
        connData.userName = a.arguments().at(4);
        connData.password = a.arguments().at(5);

        db->connectDb(connData);
        QTimer* timer = new QTimer(&a);
        timer->setInterval(ONE_MINUTE);
        QTimer::singleShot(0, watcher, &DbFileSystemWatcher::updateWatchPath);
        QObject::connect(timer, &QTimer::timeout, watcher, &DbFileSystemWatcher::updateWatchPath);
        timer->start();
    }
    else
    {
       QTimer::singleShot(1000, &a, QCoreApplication::quit);
    }

    return a.exec();
}
