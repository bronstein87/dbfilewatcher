#ifndef MODIFIEDFILESYSTEMWATCHER_H
#define MODIFIEDFILESYSTEMWATCHER_H

#include <QFileSystemWatcher>
#include <QObject>
#include <QStringList>
#include <QMap>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <dbfilewatcher.h>
#include <QFile>
#include <QDateTime>
#include <QTextStream>
#include <QScopedPointer>

class ModifiedFileSystemWatcher : public QFileSystemWatcher
{
    Q_OBJECT

public:

    ModifiedFileSystemWatcher(QObject* parent = nullptr) : QFileSystemWatcher(parent)
    {
            _sysWatcher.reset(new QFileSystemWatcher());
            connect(_sysWatcher.data(), SIGNAL(directoryChanged( QString )), this, SLOT(directoryUpdated(QString)));
            connect(_sysWatcher.data(), SIGNAL(fileChanged( QString )), this, SLOT(fileUpdated(QString)));
            createLogFile();
    }

    void addWatchPath(QString path);

signals:

    void renamed (const QString& from, const QString& to);

    void deleted (const QString& path);

    void added (const QString& path);

    void error ();

public slots:

    void directoryUpdated(const QString & path);

    void fileUpdated(const QString & path);

protected:
    void createLogFile();

    QFile logFile;

    QTextStream out;

    QMap<QString, QStringList> _currContents;

    QScopedPointer<QFileSystemWatcher> _sysWatcher;


};

class DbFileSystemWatcher : public ModifiedFileSystemWatcher
{
    Q_OBJECT
public:

    DbFileSystemWatcher(DbFileWatcher* _db, QObject* parent = nullptr) : ModifiedFileSystemWatcher(parent)
    {
            db.reset(_db);
            ConnectionData data;
            QObject::connect(this, &DbFileSystemWatcher::deleted, [this](auto& oldf) {db->tryToUpdatePath(oldf, QString());});
            QObject::connect(this, &DbFileSystemWatcher::renamed, [this](auto& oldf, auto& newf) {db->tryToUpdatePath(oldf, newf);});
            QObject::connect(db.data(), &DbFileWatcher::errorOccured, [this](auto& error)
            {out << QDateTime::currentDateTime().toString(Qt::ISODate) << "     " << "DB ERROR: " << error << endl;});
    }

    void updateWatchPath()
    {
        _currContents.clear();
        //qDebug() << "try to get dirs";
        QStringList dirList = db->getDirectroriesList();
        //qDebug() << "get dirs";
        for (auto& i : dirList)
        {
            addWatchPath("//Camera20/DATA/" + i);
            if (i[i.size() - 1] == "/")
            {
                i.remove(i.size() - 1, 1);
            }
            int pos = i.indexOf(QRegExp("(/)(?!.+/)"), 0);
            //qDebug() << pos;
            i.remove(pos, i.size() - pos);
            addWatchPath("//Camera20/DATA/" + i);
        }
    }

private:

    QScopedPointer <DbFileWatcher> db;
};

#endif // MODIFIEDFILESYSTEMWATCHER_H
