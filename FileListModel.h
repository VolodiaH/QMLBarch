#pragma once
#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QVector>
#include <QDir>
#include <QString>

#include "barch.hpp"
#include "bmp_io.h"

class FileListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString directory READ directory WRITE setDirectory NOTIFY directoryChanged)
    Q_PROPERTY(bool hasError READ hasError NOTIFY errorChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorChanged)

public:
    enum Roles
    {
        NameRole = Qt::UserRole + 1,
        PathRole,
        SizeRole,
        PrettySizeRole,
        ExtRole,
        BusyRole,
        StatusTextRole,
        ErrorRole,
        ErrorTextRole
    };
    Q_ENUM(Roles)

    explicit FileListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString directory() const { return m_dir.absolutePath(); }
    void setDirectory(const QString& path);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void process(int row);
    Q_INVOKABLE void clearError();

    bool hasError() const { return !m_error.isEmpty(); }
    QString errorText() const { return m_error; }

signals:
    void directoryChanged();
    void errorChanged();

private:
    struct Entry
    {
        QString name;
        QString path;
        QString ext;
        qint64  size = 0;
        bool    busy = false;
        QString status;
        bool    failed = false;
        QString errText;
        QFutureWatcher<QString>* watcher = nullptr;
    };
    QVector<Entry> m_items;
    QDir m_dir;
    QString m_error;

    static QString prettySize(qint64 bytes);
    void setError(const QString& text);

    void startEncode(int row);
    void startDecode(int row);

    void insertIfExists(const QString& absPath);

    void setBusy(int row, bool busy, const QString& statusText);
    void setFailure(int row, bool failed, const QString& msg = QString());
};
