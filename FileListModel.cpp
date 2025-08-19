#include "FileListModel.h"
#include <QtConcurrent>
#include <QFileInfo>
#include <QDebug>
static QString stripDotLower(const QString& ext)
{
    QString e = ext;
    if (e.startsWith('.'))
        e.remove(0,1);
    return e.toLower();
}

FileListModel::FileListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int FileListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_items.size();
}

QVariant FileListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};
    const Entry& e = m_items.at(index.row());
    switch (role)
    {
        case NameRole:        return e.name;
        case PathRole:        return e.path;
        case SizeRole:        return e.size;
        case PrettySizeRole:  return prettySize(e.size);
        case ExtRole:         return e.ext;
        case BusyRole:        return e.busy;
        case StatusTextRole:  return e.status;
        case ErrorRole:       return e.failed;
        case ErrorTextRole:   return e.errText;
    }
    return {};
}

QHash<int, QByteArray> FileListModel::roleNames() const
{
    return {
        { NameRole, "name" },
        { PathRole, "path" },
        { SizeRole, "size" },
        { PrettySizeRole, "prettySize" },
        { ExtRole, "ext" },
        { BusyRole, "busy" },
        { StatusTextRole, "statusText" },
        { ErrorRole, "hasError" },
        { ErrorTextRole, "errorText" }
    };
}

void FileListModel::setDirectory(const QString& path)
{
    QDir d(path);
    if (!d.exists())
        d = QDir::current();
    if (m_dir.absolutePath() == d.absolutePath())
        return;
    beginResetModel();
    m_dir = d;
    m_items.clear();
    endResetModel();
    emit directoryChanged();
    refresh();
}

void FileListModel::refresh()
{
    beginResetModel();
    m_items.clear();

    QStringList filters = { "*.bmp", "*.png", "*.barch" };
    QFileInfoList list = m_dir.entryInfoList(filters, QDir::Files | QDir::Readable, QDir::Name);
    m_items.reserve(list.size());
    for (const QFileInfo& fi : list)
    {
        Entry e;
        e.name = fi.fileName();
        e.path = fi.absoluteFilePath();
        e.size = fi.size();
        e.ext  = stripDotLower(fi.suffix());
        e.busy = false;
        e.status.clear();
        e.failed = false;
        e.errText.clear();
        m_items.push_back(e);
    }
    endResetModel();
}

void FileListModel::process(int row)
{
    if (row < 0 || row >= m_items.size())
        return;
    const QString ext = m_items[row].ext;

    if (m_items[row].busy)
        return; // already working

    if (ext == "bmp")
        startEncode(row);
     else if (ext == "barch")
        startDecode(row);
     else
     {
         qDebug() << "ERROR";
        setError(QStringLiteral("Unknown File"));
     }
}

void FileListModel::clearError()
{
    if (m_error.isEmpty())
        return;
    m_error.clear();
    emit errorChanged();
}

QString FileListModel::prettySize(qint64 bytes)
{
    const char* units[] = {"B","KB","MB","GB"};
    double v = double(bytes);
    int u = 0;
    while (v >= 1024.0 && u < 3) { v /= 1024.0; ++u; }
    return QString::number(v, 'f', (u==0?0:1)) + " " + units[u];
}

void FileListModel::setError(const QString& text)
{
    m_error = text;
    emit errorChanged();
}

void FileListModel::setBusy(int row, bool busy, const QString& statusText)
{
    if (row < 0 || row >= m_items.size())
        return;
    Entry& e = m_items[row];
    e.busy = busy;
    e.status = statusText;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, { BusyRole, StatusTextRole });
}

void FileListModel::setFailure(int row, bool failed, const QString& msg)
{
    if (row < 0 || row >= m_items.size()) return;
    Entry& e = m_items[row];
    e.failed = failed;
    e.errText = msg;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, { ErrorRole, ErrorTextRole, StatusTextRole });
}

static void doEncode(const QString& inPath, const QString& outPath)
{
    qDebug() << "Start encode";
    RawImageData img = loadGrayBMP(inPath.toStdString());
    barch::saveToFile(outPath.toStdString(), img);
    barch::freeImage(img);
    qDebug() << "End encode";
}

static void doDecode(const QString& inPath, const QString& outPath)
{
    RawImageData img = barch::loadFromFile(inPath.toStdString());
    writeGrayBMP(outPath.toStdString(), img);
    barch::freeImage(img);
}


static QString encodeJob(const QString& inPath, const QString& outPath)
{
    try {
        RawImageData img = loadGrayBMP(inPath.toStdString());
        barch::saveToFile(outPath.toStdString(), img);
        barch::freeImage(img);
        return {};
    } catch (const std::exception& e) {
        return QString::fromUtf8(e.what());
    }
}

static QString decodeJob(const QString& inPath, const QString& outPath)
{
    try {
        RawImageData img = barch::loadFromFile(inPath.toStdString());
        writeGrayBMP(outPath.toStdString(), img);
        barch::freeImage(img);
        return {};
    } catch (const std::exception& e) {
        return QString::fromUtf8(e.what());
    }
}

void FileListModel::insertIfExists(const QString& absPath)
{
    QFileInfo fi(absPath);
    if (!fi.exists()) return;
    const QString ext = stripDotLower(fi.suffix());
    if (ext != "bmp" && ext != "png" && ext != "barch") return;

    // avoid duplicates
    for (const auto& it : m_items)
        if (it.path == fi.absoluteFilePath())
            return;

    Entry e;
    e.name = fi.fileName();
    e.path = fi.absoluteFilePath();
    e.size = fi.size();
    e.ext  = ext;
    e.busy = false;
    e.status.clear();
    e.failed = false;
    e.errText.clear();

    beginInsertRows(QModelIndex(), m_items.size(), m_items.size());
    m_items.push_back(e);
    endInsertRows();
}

void FileListModel::startEncode(int row)
{
    Entry& e = m_items[row];
    QString out = e.path + ".packed.barch";

    setFailure(row, false, {});
    setBusy(row, true, QStringLiteral("Coding"));
    auto* watcher = new QFutureWatcher<QString>(this);
    e.watcher = watcher;

    QFuture<QString> fut = QtConcurrent::run([in=e.path, out]() { return encodeJob(in, out); });

    setFailure(row, false, {});
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, row, out]() {
        Entry& en = m_items[row];
        QString err = en.watcher->future().result();
        en.watcher->deleteLater();
        en.watcher = nullptr;

        if (err.isEmpty())
        {
            setBusy(row, false, QStringLiteral("Ready"));
            insertIfExists(out);
        } else
        {
            setBusy(row, false, QStringLiteral("Error"));
            setFailure(row, true, err);
            setError(tr("Error during encode \"%1\": %2").arg(en.name, err));
        }
    });

    watcher->setFuture(fut);
}

void FileListModel::startDecode(int row)
{
    Entry& e = m_items[row];
    QString out = e.path + ".unpacked.bmp";

    setFailure(row, false, {});
    setBusy(row, true, QStringLiteral("Decoding"));
    auto* watcher = new QFutureWatcher<QString>(this);
    e.watcher = watcher;

    QFuture<QString> fut = QtConcurrent::run([in=e.path, out]() { return decodeJob(in, out); });

    setFailure(row, false, {});
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, row, out]() {
        Entry& en = m_items[row];
        QString err = en.watcher->future().result();
        en.watcher->deleteLater();
        en.watcher = nullptr;

        if (err.isEmpty())
        {
            setBusy(row, false, QStringLiteral("Ready"));
            insertIfExists(out);
        } else
        {
            setBusy(row, false, QStringLiteral("Error"));
            setFailure(row, true, err);
        }
    });

    watcher->setFuture(fut);
}

