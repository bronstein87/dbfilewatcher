#include "dbfilewatcher.h"
#include <QDebug>
DbFileWatcher::DbFileWatcher(QString dbDriver, QObject* parent):
    Database(dbDriver, parent) {}

void DbFileWatcher::setConnectionOptions(Database* newConn)  {Q_UNUSED(newConn);}



QStringList DbFileWatcher::getDirectroriesList()
{
    try
    {
        //qDebug() << "QQ";
        checkConnection();
        return getSimpleList("SELECT filepath FROM testing.life_cycle_e filepath WHERE filepath NOT LIKE 'Не указан%';")
                + getSimpleList("SELECT filepath FROM testing.attr_value filepath WHERE filepath NOT LIKE 'Не указан%';");
    }
    catch (std::exception& e)
    {
        //qDebug() << QString(e.what());
        emit errorOccured(QString(e.what()));
    }
}



void DbFileWatcher::updateFilepath(const QString& tableName, const QString& updatePathNew, QSqlQuery& query)
{
    DbRecord rec;
    query.next();
    rec.insertValue("id", query.value(0).toInt());
    rec.setGenerated("id", true);
    if (updatePathNew.isEmpty())
    {
        rec.insertValue("filepath", "Не указан (был удален)");
    }
    else
    {
        rec.insertValue("filepath", updatePathNew);
    }
    simpleUpdate(rec, tableName);
}



void DbFileWatcher::tryToUpdatePath(const QString& updatePathOld, const QString& updatePathNew)
{
    try
    {

        QString oldFile = updatePathOld;
        QString newFile = updatePathNew;

        oldFile.append("/").remove("//Camera20/DATA/");
        newFile.append("/").remove("//Camera20/DATA/");;

       // qDebug() << "try TO UPDATE";
        auto query = execAndCheck(QString("SELECT id FROM testing.life_cycle_e WHERE filepath = '%1'")
                                  .arg(oldFile));
        if (query.size() == 0)
        {
            auto query = execAndCheck(QString("SELECT id FROM testing.attr_value WHERE filepath = '%1'")
                                      .arg(oldFile));
            if (query.size() != 0)
            {
                updateFilepath("testing.attr_value", newFile, query);
            }
        }
        else
        {
            updateFilepath("testing.life_cycle_e", newFile, query);
        }
    }
    catch (std::exception& e)
    {
        //qDebug() << QString(e.what());
        emit errorOccured(QString(e.what()));
    }
}
