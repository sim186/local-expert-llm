#ifndef CONSOLELOGGER_H
#define CONSOLELOGGER_H

#include <QMutex>
#include <QObject>
#include <atomic>
#include <thread>


#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

class ConsoleLogger : public QObject {
  Q_OBJECT
public:
  static ConsoleLogger &instance();

  // Start capturing output
  void install();

signals:
  // Signal to update UI
  void logMessage(const QString &message);

private:
  ConsoleLogger();
  ~ConsoleLogger();

  // Qt Message Handler
  static void messageHandler(QtMsgType type, const QMessageLogContext &context,
                             const QString &msg);

  // Stdout/Stderr handling
  void readPipe(int pipeFd);

  int m_stdoutPipe[2];
  int m_stderrPipe[2];
  int m_savedStdout;
  int m_savedStderr;

  std::thread m_stdoutThread;
  std::thread m_stderrThread;
  std::atomic<bool> m_stopThreads;
};

#endif // CONSOLELOGGER_H
