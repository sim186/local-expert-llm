#ifndef LLAMAWORKER_H
#define LLAMAWORKER_H

#include <QString>
#include <QThread>

/**
 * @brief Worker thread for running llama.cpp inference
 *
 * This class runs LLM text generation in a separate thread to avoid
 * blocking the UI during inference.
 */
class LlamaWorker : public QThread {
  Q_OBJECT

public:
  /**
   * @brief Constructor
   * @param modelPath Path to the GGUF model file
   * @param prompt Input prompt for text generation
   * @param parent Parent QObject
   */
  explicit LlamaWorker(const QString &modelPath, const QString &prompt,
                       QObject *parent = nullptr);

  /**
   * @brief Destructor
   */
  ~LlamaWorker();

signals:
  /**
   * @brief Signal emitted when text generation is complete
   * @param result Generated text
   */
  void finished(const QString &result);

  /**
   * @brief Signal emitted when an error occurs
   * @param error Error message
   */
  void error(const QString &error);

protected:
  /**
   * @brief Thread run method - performs LLM inference
   */
  void run() override;

private:
  QString m_modelPath; ///< Path to model file
  QString m_prompt;    ///< Input prompt
};

#endif // LLAMAWORKER_H
