#ifndef DBFILEWATCHER_H
#define DBFILEWATCHER_H

#include <QObject>
#include <database.h>
#include <QString>

class DbFileWatcher : public Database
{
    Q_OBJECT
public:
    explicit DbFileWatcher(QString dbDriver = "QPSQL", QObject* parent = nullptr);

    QStringList getDirectroriesList();

    void tryToUpdatePath(const QString& updatePathOld, const QString& updatePathNew);

    void setConnectionOptions(Database* newConn) override ;

signals:

    void errorOccured(const QString& str);

private:
    void updateFilepath(const QString& tableName, const QString& updatePathNew, QSqlQuery &query);
};

#endif // DBFILEWATCHER_H
