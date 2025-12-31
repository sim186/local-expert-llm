#ifndef CONSOLELOGGER_H
#define CONSOLELOGGER_H

#include <QObject>
#include <QSocketNotifier>
#include <QMutex>
#include <unistd.h>

class ConsoleLogger : public QObject {
    Q_OBJECT
public:
    static ConsoleLogger& instance();
    
    // Start capturing output
    void install();

signals:
    // Signal to update UI
    void logMessage(const QString &message);

private:
    ConsoleLogger();
    ~ConsoleLogger();

    // Qt Message Handler
    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

    // Stdout/Stderr handling
    void readStdout();
    void readStderr();

    int m_stdoutPipe[2];
    int m_stderrPipe[2];
    int m_savedStdout;
    int m_savedStderr;

    QSocketNotifier *m_stdoutNotifier;
    QSocketNotifier *m_stderrNotifier;
};

#endif // CONSOLELOGGER_H
