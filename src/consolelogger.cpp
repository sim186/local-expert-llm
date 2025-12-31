#include "consolelogger.h"
#include <iostream>
#include <cstdio>
#include <QDateTime>
#include <QTextStream>
#include <QRegularExpression>

ConsoleLogger& ConsoleLogger::instance() {
    static ConsoleLogger instance;
    return instance;
}

ConsoleLogger::ConsoleLogger() : m_stdoutNotifier(nullptr), m_stderrNotifier(nullptr), m_savedStdout(-1), m_savedStderr(-1) {
    // Initialize pipes
    if (pipe(m_stdoutPipe) != 0 || pipe(m_stderrPipe) != 0) {
        perror("pipe");
    }
}

ConsoleLogger::~ConsoleLogger() {
    // Restore original stdout/stderr if needed
    if (m_savedStdout != -1) dup2(m_savedStdout, STDOUT_FILENO);
    if (m_savedStderr != -1) dup2(m_savedStderr, STDERR_FILENO);
}

void ConsoleLogger::install() {
    // 1. Install Qt Message Handler
    qInstallMessageHandler(ConsoleLogger::messageHandler);

    // 2. Save original file descriptors
    m_savedStdout = dup(STDOUT_FILENO);
    m_savedStderr = dup(STDERR_FILENO);

    // 3. Redirect stdout/stderr to write-end of pipes
    // Flush first to ensure no data loss
    fflush(stdout);
    fflush(stderr);

    dup2(m_stdoutPipe[1], STDOUT_FILENO);
    dup2(m_stderrPipe[1], STDERR_FILENO);

    // 4. Setup Notifiers for the read-end
    m_stdoutNotifier = new QSocketNotifier(m_stdoutPipe[0], QSocketNotifier::Read, this);
    connect(m_stdoutNotifier, &QSocketNotifier::activated, this, &ConsoleLogger::readStdout);

    m_stderrNotifier = new QSocketNotifier(m_stderrPipe[0], QSocketNotifier::Read, this);
    connect(m_stderrNotifier, &QSocketNotifier::activated, this, &ConsoleLogger::readStderr);
}

void ConsoleLogger::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    // Format the message
    QString txt;
    switch (type) {
    case QtDebugMsg:    txt = QString("[Debug] %1").arg(msg); break;
    case QtWarningMsg:  txt = QString("[Warning] %1").arg(msg); break;
    case QtCriticalMsg: txt = QString("[Critical] %1").arg(msg); break;
    case QtFatalMsg:    txt = QString("[Fatal] %1").arg(msg); break;
    case QtInfoMsg:     txt = QString("[Info] %1").arg(msg); break;
    }

    // Emit signal (Thread-safe: Qt queues this to the main thread)
    emit instance().logMessage(txt);
}

void ConsoleLogger::readStdout() {
    char buffer[1024];
    ssize_t bytesRead = read(m_stdoutPipe[0], buffer, sizeof(buffer) - 1);
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';
        QString msg = QString::fromLocal8Bit(buffer);
        // Remove ANSI escape codes
        static QRegularExpression ansiRegex("\x1B\\[[0-9;]*[mK]");
        msg.replace(ansiRegex, "");
        emit logMessage(msg);
    }
}

void ConsoleLogger::readStderr() {
    char buffer[1024];
    ssize_t bytesRead = read(m_stderrPipe[0], buffer, sizeof(buffer) - 1);
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';
        QString msg = QString::fromLocal8Bit(buffer);
        // Remove ANSI escape codes
        static QRegularExpression ansiRegex("\x1B\\[[0-9;]*[mK]");
        msg.replace(ansiRegex, "");
        emit logMessage(msg);
    }
}
