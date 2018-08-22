#include "modifiedfilesystemwatcher.h"




void ModifiedFileSystemWatcher::addWatchPath(QString path)
{
    if (logFile.exists())
    {
        //qDebug() << "create log file";
        QFileInfo info (logFile);
        if (info.created().date() < QDate::currentDate())
        {
            logFile.close();
            createLogFile();
        }
    }
    else
    {
        createLogFile();
    }

    _sysWatcher->addPath(path);  //add path to watch

    QFileInfo f(path);

    if(f.isDir())
    {
        const QDir dirw(path);
        _currContents[path] = dirw.entryList(QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Files, QDir::DirsFirst);
    }

    //qDebug() << "Add to watch: " << path;
    out << QDateTime::currentDateTime().toString(Qt::ISODate) << "     " << "Add to watch: " << path << endl;

}

// Slot invoked whenever any of the watched directory is updated (some file in the watched dir is added, deleted or renamed)

void ModifiedFileSystemWatcher::directoryUpdated(const QString & path)
{
    //qDebug() << "Directory updated: " << path;
    out << QDateTime::currentDateTime().toString(Qt::ISODate) << "     " << "Directory updated: " << path << endl;

    QStringList currEntryList = _currContents[path];
    const QDir dir(path);

    QStringList newEntryList = dir.entryList(QDir::NoDotAndDotDot  | QDir::AllDirs | QDir::Files, QDir::DirsFirst);

    QSet<QString> newDirSet = QSet<QString>::fromList( newEntryList );

    QSet<QString> currentDirSet = QSet<QString>::fromList( currEntryList );

    // Files that have been added
    QSet<QString> newFiles = newDirSet - currentDirSet;
    QStringList newFile = newFiles.toList();

    // Files that have been removed
    QSet<QString> deletedFiles = currentDirSet - newDirSet;
    QStringList deleteFile = deletedFiles.toList();

    // Update the current set
    _currContents[path] = newEntryList;

    if(!newFile.isEmpty() && !deleteFile.isEmpty())
    {
        // File/Dir is renamed

        if(newFile.count() == 1 && deleteFile.count() == 1)
        {
            QString oldF = dir.absolutePath() + "/" + deleteFile.first();
            QString newF = dir.absolutePath() + "/" + newFile.first();
            emit renamed (oldF, newF);
            //qDebug() << "File Renamed from " << deleteFile.first()  << " to " << newFile.first();
            out << QDateTime::currentDateTime().toString(Qt::ISODate) << "     " << "File/Dir renamed from: "
                << oldF << " To:" << newF << endl;
        }
    }

    else
    {
        // New File/Dir Added to Dir
        if(!newFile.isEmpty())
        {
            //qDebug() << "New Files/Dirs added: " << newFile;
            foreach(QString file, newFile)
            {
                QString newF = dir.absolutePath() + "/" + file;
                emit added(newF);
                out << QDateTime::currentDateTime().toString(Qt::ISODate) << "     " << "New Files/Dirs added: "
                    << newF << endl;
            }
        }

        // File/Dir is deleted from Dir

        if(!deleteFile.isEmpty())
        {
            //qDebug() << "Files/Dirs deleted: " << deleteFile;
            foreach(QString file, deleteFile)
            {
                QString oldF = dir.absolutePath() + "/" + file;
                emit deleted(oldF);
                out << QDateTime::currentDateTime().toString(Qt::ISODate) << "     " << "Files/Dirs deleted: "
                    << oldF << endl;

            }
        }

    }

}


// Slot invoked whenever the watched file is modified

void ModifiedFileSystemWatcher::fileUpdated(const QString & path)
{
    QFileInfo file(path);

    QString path1 = file.absolutePath();

    QString name = file.fileName();

    //qDebug()<<"The file " << name <<" at path " << path1 << " is updated";
}


void ModifiedFileSystemWatcher::createLogFile()
{
    logFile.setFileName("log_" + QDate::currentDate().toString(Qt::ISODate) + ".txt");
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        //qDebug() << "error create log file";
        emit error();
    }
    else
    {
        //qDebug() << "OK log file";
        out.setDevice(&logFile);
    }
}
