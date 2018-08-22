#ifndef DATABASE_H
#define DATABASE_H

#include <QObject>
#include <QSqlRecord>
#include <QSqlError>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlDriver>
#include <bokzdbexceptions.h>
#include <QUuid>
#include <libpq-fe.h>
#include <QSqlRecord>
#include <QSqlField>
#include <utility.h>
using namespace std;

struct ConnectionData
{
    QString dbName;
    QString userName;
    QString password;
    QString host;
    qint32 port;
};



class DbRecord : public QSqlRecord
{
public:
    void insertValue(const QString& fieldName, const QVariant& value)
    {
        append(QSqlField(fieldName));
        setValue(fieldName, value);
        setGenerated(fieldName, false);
        flags.resize(flags.size() + 1);
    }
    void setFlag(int i, bool v)
    {
        if (i > flags.size()
                || i == -1)
        {
            Q_ASSERT(false);
        }
        flags.setBit(i, v);
    }

    bool isFlagSet(int i) const
    {
        if (i > flags.size()
                || i == -1)
        {
            Q_ASSERT(false);
        }
        return flags.at(i);
    }

    void setFlag(const QString& field, bool v)
    {
        qint32 id  = indexOf(field);
        setFlag(id, v);
    }

    bool isFlagSet(const QString& field) const
    {
        qint32 id  = indexOf(field);
        return isFlagSet(id);
    }
private:
    QBitArray flags;
};



class Database : public QObject
{
    Q_OBJECT
public:
    explicit Database (QString dbDriver = "QPSQL", QObject* parent = nullptr);

    //конструктор для копирования подключения главного потока в другой объект
    // использовать только в главном потоке
    explicit Database (Database* other, QObject* parent = nullptr);

    Database(const Database&)                   = delete;

    Database(Database&& )                       = delete;

    Database& operator=(const Database&)        = delete;

    Database& operator=(Database&&)             = delete;

    void connectDb(const ConnectionData& cData);

    bool isDriverValid() const;

    bool isConnected() const;

    const ConnectionData* getConnData() const;

    const QSqlDatabase* getQSqlDatabase() const;

    void startTransaction();

    void endTransaction();

    void cancelTransaction();

    void analyzeDb(const QString& tableName = "");

    void disconnectDb();

    void checkConnection();

    qint32 activeConnectionsCount();

    void cancelQuery(const QUuid& connId);

    bool isQueryActive(const QUuid& connId);

    virtual void setConnectionOptions(Database* newConn) = 0;

    void simpleInsert(const DbRecord& rec, const QString& tableName);

    QSqlQuery simpleInsertReturning(const DbRecord& rec, const QString& tableName);

    void simpleUpdate(const DbRecord& rec, const QString& tableName);

    void simpleDelete(const DbRecord& rec, const QString& tableName);

    QVector <QVariant> getValueByRelation
    (const QString& tableName, const QVector<QVariant> &values, const QStringList& relTables, const QStringList& relFieldNames, const QStringList& fieldNames);

    QVector <QString> getTablePrimaryKey(const QString& tableName);

    QVector <QString> getTableColumnNames(const QString& tableName, QString schema = "public");

    QVector<QPair<QString, QString> > getTablesRelation(const QString& table1, const QString& table2);

    QString getUserName()
    {
        return connData->userName;
    }

    template <typename DB, typename Function , typename ...Args, typename CallBack,typename ReturnValue>
    static QUuid callAsync(DB* db, QSharedPointer<QFutureWatcher<ReturnValue>> watcher, Function&& query,CallBack&& callBack, Args&& ...args);

    template <typename DB, typename Function , typename ...Args, typename CallBack, typename ReturnValue, typename ConnSetter>
    static QUuid callAsync(DB* db, QSharedPointer<QFutureWatcher<ReturnValue>> watcher, Function&& query, CallBack&& callBack, ConnSetter&& setter, Args&& ...args);


    //    template <typename DB, typename Function , typename ...Args, typename CallBack,typename ReturnValue>
    //    static QUuid callAsync(DB* db, QFutureWatcher<ReturnValue>& watcher, Function&& query,CallBack&& callBack, Args&& ...args);


    virtual ~Database();

signals:
    void queryCanceled();

protected:

    void internalStartTransaction();

    void internalEndTransaction();

    void internalCancelTransaction();

    bool checkQueryValueNull(const QVariant& value);

    template <typename DB, typename ReturnValue, typename Function , typename ...Args>
    static auto exceptionWrapper (QUuid id, QSharedPointer<QFutureWatcher<ReturnValue>> w, DB* db, Function function, Args& ...args)
    -> decltype((db->*function)(args...));

    template <typename DB, typename ReturnValue, typename Function , typename ConnSetter, typename ...Args>
    static auto exceptionWrapper (QUuid id, QSharedPointer<QFutureWatcher<ReturnValue>> w, DB* db, Function function, ConnSetter setter,  Args& ...args)
    -> decltype((db->*function)(args...));

    QSqlDatabase* getDb();

    QSqlQuery execAndCheck(const QString& queryText);

    QUuid getConnId() const noexcept {return connId;}

    QStringList getSimpleList(const QString& queryText);

    void setQueryCanceled(){queryCancel = true;}

    QVariant getSomeInfo(const QString& queryText);

    void execPreparedQuery(QSqlQuery &query);

    explicit Database(QUuid id);

    bool queryCancel = false;

    bool outsideTransaction = false;

private:

    QSqlQuery simpleInsertPrivate(const DbRecord &rec, const QString& tableName, const QString& addQuery = QString());
    void cancelQueryPrivate(QSqlDatabase* _db);
    QVector <QVariant> getValueByRelationManyToManyClosed
    (const QString& tableName, const QVector<QVariant> &values, const QString& relTable, const QString& relFieldName);
    static QMap <QUuid, QPair<QSqlDatabase*, bool>> connections;
    static int connectionsCount;
    static int localConnCount;
    bool isLocal = false;
    static QMutex mutex;
    QSqlDatabase* db = nullptr;
    QUuid connId;
    QSharedPointer <ConnectionData> connData;
};

// Добавить Uuid для отмены запросов, хранить в connections
template <typename DB, typename Function , typename ...Args, typename CallBack, typename ReturnValue>
QUuid Database::callAsync(DB* db, QSharedPointer <QFutureWatcher<ReturnValue>> watcher, Function&& query,CallBack&& callBack, Args&& ...args)
{
    static void* prevFuture = nullptr;
    if(prevFuture != watcher.data()) {
        auto conn = QSharedPointer <QMetaObject::Connection>::create();
        *conn = QObject::connect(watcher.data(), &QFutureWatcher <ReturnValue>::finished, callBack);
        QObject::connect(watcher.data(), &QFutureWatcher <ReturnValue>::finished, [conn](){QObject::disconnect(*conn);});
    }
    prevFuture = reinterpret_cast <void*> (watcher.data());
    auto id = QUuid::createUuid();
    QFuture <ReturnValue> futureValue =
            QtConcurrent::run(
                bind(
                    exceptionWrapper <DB,ReturnValue,Function,Args...>, watcher, id, db, forward<Function>(query), forward<Args>(args)...));
    watcher->setFuture(futureValue);
    return id;
}

template <typename DB, typename Function , typename ...Args, typename CallBack, typename ReturnValue, typename ConnSetter>
QUuid Database::callAsync(DB* db, QSharedPointer<QFutureWatcher<ReturnValue>> watcher, Function&& query, CallBack&& callBack, ConnSetter&& setter, Args&& ...args)
{

    static void* prevFuture = nullptr;
    if(prevFuture != watcher.data()) {
        auto conn = QSharedPointer <QMetaObject::Connection>::create();
        *conn = QObject::connect(watcher.data(), &QFutureWatcher <ReturnValue>::finished, callBack);
        QObject::connect(watcher.data(), &QFutureWatcher <ReturnValue>::finished, [conn](){QObject::disconnect(*conn);});
    }
    prevFuture = reinterpret_cast <void*> (watcher.data());
    auto id = QUuid::createUuid();
    QFuture<ReturnValue> futureValue
            = QtConcurrent::run(
                bind(
                    exceptionWrapper <DB,ReturnValue,Function,ConnSetter,Args...>, id, watcher, db, forward<Function>(query), setter, forward<Args>(args)...));
    watcher->setFuture(futureValue);
    return id;
}


// версия для статичного QFutureWatcher, для нее нужно перегрузить exceptionWrapper-ы без QSharedPointer
//template <typename DB, typename Function , typename ...Args, typename CallBack, typename ReturnValue>
//QUuid Database::callAsync(DB* db, QFutureWatcher <ReturnValue>& watcher, Function&& query, CallBack&& callBack, Args&& ...args)
//{
//    static void* prevFuture = nullptr;
//    if(prevFuture != &watcher) {
//        QObject::connect(&watcher, &QFutureWatcher <ReturnValue>::finished, callBack);
//        auto conn = QSharedPointer <QMetaObject::Connection>::create();
//        *conn = QObject::connect(&watcher, &QFutureWatcher <ReturnValue>::finished, [conn](){QObject::disconnect(*conn);});
//    }
//    prevFuture = reinterpret_cast <void*> (&watcher);
//    auto id = QUuid::createUuid();
//    QFuture <ReturnValue> futureValue =
//            QtConcurrent::run(
//                bind(
//                    exceptionWrapper <DB,ReturnValue,Function,Args...>, id, watcher, db, forward<Function>(query),forward<Args>(args)...));
//    watcher.setFuture(futureValue);
//    return id;
//}



template <typename DB ,typename ReturnValue, typename Function ,typename ConnSetter, typename ...Args>
auto Database::exceptionWrapper (QUuid id, QSharedPointer<QFutureWatcher<ReturnValue>> w, DB* db, Function function, ConnSetter setter, Args& ...args)
-> decltype((db->*function)(args...))
{
    try
    {
        QScopedPointer <DB> tempDbConn(new DB(id));
        setter(tempDbConn.data());
        QObject::connect(tempDbConn.data(), &Database::queryCanceled, tempDbConn.data(),
                         [w](){w->disconnect(); w->cancel();}, Qt::DirectConnection);
        db->setConnectionOptions(tempDbConn.data());
        return (tempDbConn.data()->*function)(args...);
    }
    catch (std::exception& e)
    {
        throw ThreadDbException(e);
    }
}


template <typename DB, typename ReturnValue, typename Function , typename ...Args>
auto Database::exceptionWrapper (QUuid id, QSharedPointer<QFutureWatcher<ReturnValue>> w, DB* db, Function function, Args& ...args)
-> decltype((db->*function)(args...))
{
    try
    {
        QScopedPointer <DB> tempDbConn(new DB(id));
        QObject::connect(tempDbConn.data(), &Database::queryCanceled, tempDbConn.data(),
                         [w](){w->disconnect(); w->cancel();}, Qt::DirectConnection);
        db->setConnectionOptions(tempDbConn.data());
        return (tempDbConn.data()->*function)(args...);
    }
    catch (std::exception& e)
    {
        throw ThreadDbException(e);
    }
}


class QueryWatcher : public QObject
{
    Q_OBJECT
signals:
    void queryStarted();
    void stillActiveQueries();
public:
    explicit QueryWatcher(Database* _d) : d(_d)
    {}

    QueryWatcher(const QueryWatcher&)                   = delete;

    QueryWatcher(QueryWatcher&& )                       = delete;

    QueryWatcher& operator=(const QueryWatcher&)        = delete;

    QueryWatcher& operator=(QueryWatcher&&)             = delete;

    void add(const QUuid& id)
    {
        activeQueries.append(id);
    }


    qint32 count() {return activeQueries.size();}

    void checkActiveQueries()
    {
        QVector<QUuid>::iterator it = activeQueries.begin();
        while (it != activeQueries.end())
        {
            if (!d->isQueryActive(*it))
            {
                it = activeQueries.erase(it);
            }
            else
            {
                ++it;
            }
        }
        if (activeQueries.isEmpty())
        {
            emit stillActiveQueries();
        }
    }
    void cancelActiveQueries()
    {
        for(auto i : activeQueries)
        {
            d->cancelQuery(i);
        }
    }

    template  <typename T>
    void updateActiveQueries(QFutureWatcher <T>* w)
    {
        emit queryStarted();
        connect(w, &QFutureWatcher <T>::finished, [this]()
        {
            checkActiveQueries();
        });
    }
private:
    QVector <QUuid> activeQueries;
    Database* d;
};


#endif // DATABASE_H
