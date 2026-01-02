#include "consolelogger.h"
#include <QDateTime>
#include <QRegularExpression>
#include <QTextStream>
#include <cstdio>
#include <iostream>


#ifdef _WIN32
#include <fcntl.h>
#define pipe(fds) _pipe(fds, 4096, _O_BINARY)
#define dup _dup
#define dup2 _dup2
#define read _read
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

ConsoleLogger &ConsoleLogger::instance() {
  static ConsoleLogger instance;
  return instance;
}

ConsoleLogger::ConsoleLogger()
    : m_savedStdout(-1), m_savedStderr(-1), m_stopThreads(false) {
  // Initialize pipes
  if (pipe(m_stdoutPipe) != 0 || pipe(m_stderrPipe) != 0) {
    perror("pipe");
  }
}

ConsoleLogger::~ConsoleLogger() {
  m_stopThreads = true;

  // Close write ends to unblock reads if possible, or just detach
  // In a real app we might want to handle this more gracefully
  if (m_stdoutThread.joinable())
    m_stdoutThread.detach();
  if (m_stderrThread.joinable())
    m_stderrThread.detach();

  // Restore original stdout/stderr if needed
  if (m_savedStdout != -1)
    dup2(m_savedStdout, STDOUT_FILENO);
  if (m_savedStderr != -1)
    dup2(m_savedStderr, STDERR_FILENO);
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

  // 4. Start threads to read from pipes
  m_stdoutThread = std::thread(&ConsoleLogger::readPipe, this, m_stdoutPipe[0]);
  m_stderrThread = std::thread(&ConsoleLogger::readPipe, this, m_stderrPipe[0]);
}

void ConsoleLogger::messageHandler(QtMsgType type,
                                   const QMessageLogContext &context,
                                   const QString &msg) {
  // Format the message
  QString txt;
  switch (type) {
  case QtDebugMsg:
    txt = QString("[Debug] %1").arg(msg);
    break;
  case QtWarningMsg:
    txt = QString("[Warning] %1").arg(msg);
    break;
  case QtCriticalMsg:
    txt = QString("[Critical] %1").arg(msg);
    break;
  case QtFatalMsg:
    txt = QString("[Fatal] %1").arg(msg);
    break;
  case QtInfoMsg:
    txt = QString("[Info] %1").arg(msg);
    break;
  }

  // Emit signal (Thread-safe: Qt queues this to the main thread)
  emit instance().logMessage(txt);
}

void ConsoleLogger::readPipe(int pipeFd) {
  char buffer[1024];
  while (!m_stopThreads) {
    ssize_t bytesRead = read(pipeFd, buffer, sizeof(buffer) - 1);
    if (bytesRead > 0) {
      buffer[bytesRead] = '\0';
      QString msg = QString::fromLocal8Bit(buffer);
      // Remove ANSI escape codes
      static QRegularExpression ansiRegex("\x1B\\[[0-9;]*[mK]");
      msg.replace(ansiRegex, "");
      emit logMessage(msg);
    } else {
      // Error or EOF
      break;
    }
  }
}
