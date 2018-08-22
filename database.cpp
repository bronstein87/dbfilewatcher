#include "database.h"
#include <QDebug>

bool operator!=(const ConnectionData& a1, const ConnectionData& a2)
{
    return     a1.dbName != a2.dbName
            || a1.userName != a2.userName
            || a1.password != a2.password
            || a1.host != a2.host
            || a1.port != a2.port;
}


int Database::connectionsCount = 0;
int Database::localConnCount = 0;
QMutex Database::mutex;
QMap <QUuid, QPair<QSqlDatabase*, bool>> Database::connections;



Database::Database(QString dbDriver, QObject* parent) :
    QObject(parent), connData(new ConnectionData)

{
    mutex.lock();
    // Название подключения формируется из Uuid и номера подключения
    connId = QUuid::createUuid();
    QString connName  = QString("conn" + QString::number(++connectionsCount) + connId.toString());
    db = new QSqlDatabase(QSqlDatabase::addDatabase(dbDriver, connName));
    connections.insert(connId, qMakePair(db, false));
    mutex.unlock();
    if (localConnCount == 0)
    {
        isLocal = true;
        ++localConnCount;
    }
}
Database::Database (Database* other, QObject* parent) :
    QObject(parent)
{
    isLocal = true;
    ++localConnCount;
    connId = other->connId;
    db = other->db;
    connData = other->connData;
}

Database::Database(QUuid id) :
    QObject(nullptr), connData(new ConnectionData)
{
    mutex.lock();
    QString connName = QString("conn" + QString::number(++connectionsCount) + id.toString());
    db = new QSqlDatabase(QSqlDatabase::addDatabase("QPSQL", connName));
    connections.insert(id, qMakePair(db, false));
    mutex.unlock();
    connId = id;
}

void Database::connectDb(const ConnectionData& cData)
{
    if (*connData != cData)
    {
        *connData = cData;
    }

    if (!isDriverValid())
    {
        throw DbException("Драйвер не подключен");
    }

    db->setHostName(connData->host);
    db->setPort(connData->port);
    db->setDatabaseName(connData->dbName);
    db->setUserName(Utility::translate(connData->userName));
    db->setPassword(connData->password);

    if (!db->open())
    {
        throw DbException(db->lastError().text().toStdString());
    }
}


bool Database::isDriverValid() const
{
    return db->isDriverAvailable("QPSQL") && db->isValid();
}


void Database::disconnectDb()
{
    if (db->isOpen())
    {
        db->close();
    }
}

bool Database::isConnected() const
{
    return db->isOpen();
}


void Database::analyzeDb(const QString& tableName)
{
    checkConnection();
    QSqlQuery query(QString(), *db);
    QString queryText = QString("VACUUM ANALYZE %1").arg(tableName);
    if (!query.exec(queryText))
    {
        throw DbException(query.lastError().text().toStdString());
    }

}


const ConnectionData* Database::getConnData() const
{
    return connData.data();
}

const QSqlDatabase* Database::getQSqlDatabase() const
{
    return db;
}

void Database::startTransaction()
{
    checkConnection();
    if (!db->transaction())
    {
        throw DbException(db->lastError().text().toStdString());
    }
    outsideTransaction = true;
}

void Database::endTransaction()

{
    checkConnection();
    outsideTransaction = false;
    if(!db->commit())
    {
        throw DbException(db->lastError().text().toStdString());
    }
}


void Database::internalStartTransaction()
{
    if (!outsideTransaction)
    {
        if (!db->transaction())
        {
            throw DbException(db->lastError().text().toStdString());
        }
    }
}

void Database::internalEndTransaction()
{
    if (!outsideTransaction)
    {
        if(!db->commit())
        {
            throw DbException(db->lastError().text().toStdString());
        }
    }
}


void Database::internalCancelTransaction()
{
    if (!outsideTransaction)
    {
        if (!db->rollback())
        {
            throw DbException(db->lastError().text().toStdString());
        }
    }
}

bool Database::checkQueryValueNull(const QVariant& value)
{
    return (value.toString() == "NULL" || value.isNull()) ? true : false;
}

void Database::cancelTransaction()
{ 
    checkConnection();
    if (!db->rollback())
    {
        throw DbException(db->lastError().text().toStdString());
    }
}

QSqlDatabase* Database::getDb()
{
    return db;
}

QSqlQuery Database::execAndCheck(const QString& queryText)
{
    QSqlQuery query(QString(), *getQSqlDatabase());
    if (!query.exec(queryText))
    {
        mutex.lock();
        auto it = connections.find(connId);
        if (it != connections.end())
        {
            queryCancel = it.value().second;
            if (queryCancel)
            {
                it.value().second = false;
                emit queryCanceled();
                queryCancel = false;
            }
        }
        mutex.unlock();
        cancelTransaction();
        throw DbException(query.lastError().text().toStdString());
    }
    return query;
}

QStringList Database::getSimpleList(const QString& queryText)
{
    QSqlQuery query = execAndCheck(queryText);
    QStringList list;
    while(query.next())
    {
        list.append(query.value(0).toString());
    }
    return list;
}

void Database::simpleInsert(const DbRecord &rec, const QString& tableName)
{
    simpleInsertPrivate(rec, tableName);
}

QSqlQuery Database::simpleInsertReturning(const DbRecord& rec, const QString& tableName)
{
    return simpleInsertPrivate(rec, tableName, "RETURNING *");
}

QSqlQuery Database::simpleInsertPrivate(const DbRecord &rec, const QString& tableName, const QString& addQuery)
{
    QString what;
    QString values;
    for (int i = 0; i < rec.count(); i++)
    {
        what.append(rec.fieldName(i) + ",");
        values.append(":" + rec.fieldName(i) + ",");
    }
    what.chop(1);
    values.chop(1);
    QSqlQuery query(QString(), *getQSqlDatabase());
    QString queryText =  QString("INSERT INTO %1 (%2) VALUES (%3) %4" )
            .arg(tableName)
            .arg(what)
            .arg(values)
            .arg(addQuery);
    query.prepare(queryText);
    for (int i = 0; i < rec.count(); i++)
    {
        query.bindValue(":" + rec.fieldName(i), rec.value(i));
    }
    execPreparedQuery(query);
    return query;
}

// делает апдейт всех переданных полей, даже если они переданы в качестве ПК (переделать, подумать ввести определение того,
// что это ПК, но в тоже время его тоже нужно обновить
void Database::simpleUpdate(const DbRecord& rec, const QString& tableName)
{
    QString setString = " SET ";
    QString whereString = " WHERE ";
    for (int i = 0; i < rec.count(); i++)
    {
        if (rec.isGenerated(i))
        {
            whereString.append(QString(" %1=:%2 AND")
                               .arg(rec.fieldName(i))
                               .arg(rec.fieldName(i)));
        }
        else
        {
            setString.append(QString(" %1=:%2,")
                             .arg(rec.fieldName(i))
                             .arg(rec.fieldName(i)));
        }
    }
    whereString.chop(3);
    setString.chop(1);
    QSqlQuery query(QString(), *getQSqlDatabase());
    QString queryText = QString("UPDATE %1 " + setString + whereString).arg(tableName);
    query.prepare(queryText);
    for (int i = 0; i < rec.count(); i++)
    {
        query.bindValue(":" + rec.fieldName(i), rec.value(i));/*":" + rec.fieldName(i).toUpper()*/
    }
    execPreparedQuery(query);
}


void Database::simpleDelete(const DbRecord& rec, const QString& tableName)
{
    QString whereString = " WHERE ";
    for (int i = 0; i < rec.count(); i++)
    {

        whereString.append(QString(" %1=:%2 AND")
                           .arg(rec.fieldName(i))
                           .arg(rec.fieldName(i)));

    }
    whereString.chop(3);
    QSqlQuery query(QString(), *getQSqlDatabase());
    QString queryText = QString("DELETE FROM %1 " + whereString).arg(tableName);
    query.prepare(queryText);

    for (int i = 0; i < rec.count(); i++)
    {
        query.bindValue(":" + rec.fieldName(i), rec.value(i));
    }
    execPreparedQuery(query);
}

void Database::checkConnection()
{
    if (!isConnected())
    {
        throw DbException("Соединение с БД не установлено");
    }
}

QVariant Database::getSomeInfo(const QString& queryText)
{
    checkConnection();
    QSqlQuery query = execAndCheck(queryText);
    query.first();
    return query.value(0);
}


void Database::execPreparedQuery(QSqlQuery& query)
{
    if (!query.exec())
    {
        cancelTransaction();
        throw DbException(query.lastError().text().toStdString());
    }
}

qint32 Database::activeConnectionsCount()
{
    return connectionsCount;
}

void Database::cancelQuery(const QUuid &connId)
{
    mutex.lock();
    QMap <QUuid, QPair<QSqlDatabase*,bool>>::iterator it = connections.find(connId);
    if (it != connections.end())
    {
        cancelQueryPrivate(it.value().first);
        it.value().second = true;
    }
    mutex.unlock();
}




bool Database::isQueryActive(const QUuid& connId)
{
    return connections.find(connId) != connections.end();
}

QVector <QVariant> Database::getValueByRelation
(const QString& tableName, const QVector <QVariant>& values, const QStringList& relTables, const QStringList& relFieldNames, const QStringList& fieldNames)
{
    using pair = QPair <QString, QString>;
    QVector <pair> v;
    // если это таблица связи многие ко многим, замкнутая на одной таблице
    if (relFieldNames.size() == 2
            && relFieldNames[0] == relFieldNames[1]
            && relTables[0] == relTables[1])
    {
        return getValueByRelationManyToManyClosed(tableName, values, relTables[0], relFieldNames[0]);
    }

    for (int i = 0; i < relTables.size(); i++)
    {
        v.append(getTablesRelation(tableName, relTables[i]));
    }

    QString whereCond(" WHERE");
    QString joinCond(" ");
    QString fields(" ");
    for (int i = 0; i < values.size(); i++)
    {
        fields.append(tableName + "." + fieldNames[i] + ",");
        joinCond.append(QString(" INNER JOIN %1 ON(%2.%3 = %1.%4) ")
                        .arg(relTables[i]).arg(tableName).arg(v[i].first).arg(v[i].second));
        whereCond.append(" " + relTables[i] + "." + relFieldNames[i] + "=" + "'" + values[i].toString() + "'");
        if (i != values.size() - 1)
        {
            whereCond.append(" AND");
        }
    }
    fields.remove(fields.size() - 1, 1);
    QString queryText = QString("SELECT %1 FROM %2 %3 %4")
            .arg(fields)
            .arg(tableName)
            .arg(joinCond)
            .arg(whereCond);
    QSqlQuery query = execAndCheck(queryText);
    QVector <QVariant> res;
    query.next();
    for (int i = 0; i < values.size(); i++)
    {
        res.append(query.value(i));
    }
    return res;


}

QVector <QVariant> Database::getValueByRelationManyToManyClosed
(const QString& tableName, const QVector <QVariant>& values, const QString& relTable, const QString& relFieldName)
{
    auto v = getTablesRelation(tableName, relTable);
    QString queryText = QString("SELECT %1, %2 FROM %3 WHERE %1 = (SELECT %4 FROM %5 WHERE %6 = '%7') "
                                "AND %2 = (SELECT %4 FROM %5 WHERE %6 = '%8')")
            .arg(v[0].first)
            .arg(v[1].first)
            .arg(tableName)
            .arg(v[0].second)
            .arg(relTable)
            .arg(relFieldName)
            .arg(values[0].toString())
            .arg(values[1].toString());
    QSqlQuery query = execAndCheck(queryText);
    QVector <QVariant> res;
    query.next();
    for (int i = 0; i < values.size(); i++)
    {
        res.append(query.value(i));
    }
    return res;

}

QVector <QString> Database::getTablePrimaryKey(const QString& tableName)
{
    checkConnection();
    QString queryText = QString("SELECT a.attname "
                                "FROM   pg_index i "
                                "JOIN   pg_attribute a ON a.attrelid = i.indrelid "
                                " AND a.attnum = ANY(i.indkey) "
                                " WHERE  i.indrelid = '%1'::regclass "
                                " AND    i.indisprimary;")
            .arg(tableName);
    QSqlQuery query = execAndCheck(queryText);
    QVector <QString> names;
    while (query.next())
    {
        names.append(query.value(0).toString());
    }
    return names;
}

QVector<QPair <QString, QString>> Database::getTablesRelation(const QString& table1, const QString& table2)
{
    checkConnection();
    QString addquery;
    auto schemaNameTable1 = table1.split(".");
    if (schemaNameTable1.size() == 2)
    {
        addquery = QString("AND tc.table_name = '%1' AND tc.table_schema = '%2' ")
                .arg(schemaNameTable1[1])
                .arg(schemaNameTable1[0]);
    }
    else
    {
        addquery = QString("AND tc.table_name = '%1'")
                .arg(table1);
    }

    auto schemaNameTable2 = table2.split(".");
    if (schemaNameTable2.size() == 2)
    {
        addquery.append(QString("AND ccu.table_name = '%1' AND ccu.table_schema = '%2' ")
                        .arg(schemaNameTable2[1])
                .arg(schemaNameTable2[0]));
    }
    else
    {
        addquery.append(QString("AND ccu.table_name = '%1'").arg(table2));
    }

    QString queryText = QString("SELECT "
                                "kcu.column_name, ccu.column_name AS foreign_column_name "
                                "FROM "
                                "information_schema.table_constraints AS tc "
                                "JOIN information_schema.key_column_usage AS kcu "
                                "ON tc.constraint_name = kcu.constraint_name "
                                "JOIN information_schema.constraint_column_usage AS ccu "
                                "ON ccu.constraint_name = tc.constraint_name "
                                "WHERE constraint_type = 'FOREIGN KEY' ");
    queryText.append(addquery);

    QSqlQuery query = execAndCheck(queryText);
    QVector <QPair <QString, QString>> columns;
    while (query.next())
    {
        columns.append(qMakePair(query.value(0).toString(),query.value(1).toString()));
    }
    return columns;
}

QVector <QString> Database::getTableColumnNames(const QString& tableName, QString schema)
{
    checkConnection();
    QString queryText = QString("select column_name from information_schema.columns where "
                                " table_name='%1' AND table_schema = '%2';")
            .arg(tableName)
            .arg(schema);
    QSqlQuery query = execAndCheck(queryText);
    QVector <QString> names;
    while (query.next())
    {
        names.append(query.value(0).toString());
    }
    return names;
}


void Database::cancelQueryPrivate(QSqlDatabase* _db)
{
    QVariant v = _db->driver()->handle();
    if (qstrcmp(v.typeName(), "PGconn*") == 0)
    {
        PGconn* rawConn = *static_cast<PGconn **>(v.data());
        if (rawConn != nullptr)
        {
            PGcancel* cancelQuery = PQgetCancel(rawConn);
            constexpr const quint16 tryCount = 10;
            int result = 0;
            quint16 tryCounter = 0;
            char errorMsg[256];
            while (result!= 1 && tryCounter < tryCount)
            {
                result = PQcancel(cancelQuery, errorMsg, 256);
            }
            if (result != 1)
            {
                throw DbException(errorMsg);
            }
            queryCancel = true;
        }
    }
}


Database::~Database()
{
    if (db != nullptr)
    {
        if (isLocal)
        {
            if (localConnCount == 1)
            {
                QString connectionName;
                connectionName = db->connectionName();
                if (db->isOpen())
                {
                    db->close();
                }
                delete db;
                db = nullptr;
                QSqlDatabase::removeDatabase(connectionName);
                connectionsCount = 0;
            }
            else
            {
                --localConnCount;
            }
        }
        else
        {
            QString connectionName;
            connectionName = db->connectionName();
            if (db->isOpen())
            {
                db->close();
            }
            delete db;
            db = nullptr;
            mutex.lock();
            connections.erase(connections.find(connId));
            QSqlDatabase::removeDatabase(connectionName);
            --connectionsCount;
            mutex.unlock();
        }
    }
}


