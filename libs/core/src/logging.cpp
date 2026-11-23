#include "napp/core/logging.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

#include <cstdio>

namespace napp {
namespace {

QString g_logFilePath;
QMutex g_logMutex;

const char* levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg: return "DEBUG";
    case QtInfoMsg: return "INFO";
    case QtWarningMsg: return "WARN";
    case QtCriticalMsg: return "ERROR";
    case QtFatalMsg: return "FATAL";
    }
    return "LOG";
}

void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const QString line = QStringLiteral("%1 [%2] %3%4")
                             .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                  QString::fromLatin1(levelName(type)),
                                  message,
                                  context.file ? QStringLiteral(" (%1:%2)")
                                                     .arg(QString::fromLatin1(context.file))
                                                     .arg(context.line)
                                               : QString());

    {
        QMutexLocker locker(&g_logMutex);
        if (!g_logFilePath.isEmpty()) {
            QFile file(g_logFilePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                QTextStream stream(&file);
                stream.setEncoding(QStringConverter::Utf8);
                stream << line << '\n';
            }
        }
    }

    std::fputs(qPrintable(line + QLatin1Char('\n')), stderr);
    std::fflush(stderr);

    if (type == QtFatalMsg)
        std::abort();
}

} // namespace

void initLogging(const QString& filePath)
{
    {
        QMutexLocker locker(&g_logMutex);
        g_logFilePath = filePath;
        if (!filePath.isEmpty())
            QDir().mkpath(QFileInfo(filePath).absolutePath());
    }
    qInstallMessageHandler(messageHandler);
}

QString logFilePath()
{
    QMutexLocker locker(&g_logMutex);
    return g_logFilePath;
}

} // namespace napp
