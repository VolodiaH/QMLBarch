#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDir>
#include <QFileInfo>
#include <QStringList>

#include "FileListModel.h"

static QString resolveStartDir(QStringList args)
{
    if (args.size() >= 2)
    {
        QString path = args.at(1);
        QFileInfo fi(path);
        if (fi.exists() && fi.isDir())
        {
            return fi.absoluteFilePath();
        }
    }
    return QDir::currentPath();
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Demo");
    QCoreApplication::setApplicationName("qmlBarch");

    QString startDir = resolveStartDir(QCoreApplication::arguments());

    FileListModel model;
    model.setDirectory(startDir);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("fileModel", &model);
    engine.rootContext()->setContextProperty("startDir", startDir);

    const QUrl url(QStringLiteral("../../Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
