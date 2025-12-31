#ifndef LLAMAWORKER_H
#define LLAMAWORKER_H

#include <QString>
#include <QObject>
#include <atomic>

// Forward declarations for llama.cpp types
struct llama_model;
struct llama_context;

struct LlamaParams {
    QString modelPath;
    float temperature = 0.7f;
    float topP = 0.9f;
    int contextSize = 4096;
    int threads = 4;
    int gpuLayers = 0; // Added for GPU support
};

/**
 * @brief Worker object for running llama.cpp inference
 *
 * This class runs LLM text generation. It is designed to be moved to a 
 * separate QThread. It maintains the model state to avoid reloading.
 */
class LlamaWorker : public QObject {
  Q_OBJECT

public:
  explicit LlamaWorker(QObject *parent = nullptr);
  ~LlamaWorker();

  bool isModelLoaded() const;

public slots:
  /**
   * @brief Load the model if params have changed or model is not loaded
   */
  void loadModel(const LlamaParams &params);

  /**
   * @brief Generate text based on prompt
   */
  void generate(const QString &prompt, const LlamaParams &genParams);

  /**
   * @brief Request to stop current generation
   */
  void stop();

signals:
  void modelLoaded();
  void finished(const QString &result);
  void error(const QString &error);
  void statsReady(float elapsedSec);
  void statusUpdate(const QString &status);

private:
  void freeModel();

  llama_model *m_model = nullptr;
  llama_context *m_ctx = nullptr;
  LlamaParams m_currentParams;
  std::atomic<bool> m_stopRequested;
};

#endif // LLAMAWORKER_H
