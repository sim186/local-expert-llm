#ifndef LLAMAWORKER_H
#define LLAMAWORKER_H

#include <QString>
#include <QThread>

struct LlamaParams {
    QString modelPath;
    float temperature = 0.7f;
    float topP = 0.9f;
    int contextSize = 4096;
    int threads = 4;
};

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
   * @param params Configuration parameters for the model
   * @param prompt Input prompt for text generation
   * @param parent Parent QObject
   */
  explicit LlamaWorker(const LlamaParams &params, const QString &prompt,
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

  /**
   * @brief Signal emitted with generation statistics
   * @param elapsedSec Time taken for generation in seconds
   */
  void statsReady(float elapsedSec);

protected:
  /**
   * @brief Thread run method - performs LLM inference
   */
  void run() override;

private:
    LlamaParams m_params;
    QString m_prompt;
};

#endif // LLAMAWORKER_H
