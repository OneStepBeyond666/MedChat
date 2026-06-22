#ifndef SERVERDB_H
#define SERVERDB_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>

class ServerDB : public QObject
{
    Q_OBJECT
public:
    explicit ServerDB(const QString &dbDir, QObject *parent = nullptr);
    ~ServerDB();

    /// 获取主数据库连接（主线程使用）
    QSqlDatabase database() const;

    /// 为调用线程创建独立连接（线程安全）
    QSqlDatabase cloneConnection(const QString &connName);

    /// 数据库文件路径
    QString databasePath() const { return m_dbPath; }

private:
    void initDatabase();
    void createTables();

    QString m_dbPath;
    QString m_connName;
};

#endif // SERVERDB_H
