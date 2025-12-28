#include "llamaworker.h"
#include <QDebug>
#include <QFile>
#include <ctime>

// Include llama.cpp headers
#ifdef __cplusplus
extern "C" {
#endif
#include "llama.h"
#ifdef __cplusplus
}
#endif

#include "common.h"
#include <string>
#include <vector>

/**
 * @brief Constructor
 */
LlamaWorker::LlamaWorker(const QString &modelPath, const QString &prompt,
                         QObject *parent)
    : QThread(parent), m_modelPath(modelPath), m_prompt(prompt) {}

/**
 * @brief Destructor
 */
LlamaWorker::~LlamaWorker() {}

/**
 * @brief Run LLM inference in background thread
 */
void LlamaWorker::run() {
  qDebug() << "LlamaWorker: Starting inference...";
  qDebug() << "Model path:" << m_modelPath;
  qDebug() << "Prompt:" << m_prompt;

  try {
    // Convert QString to std::string
    std::string model_path = m_modelPath.toStdString();
    std::string prompt = m_prompt.toStdString();

    // Initialize llama backend
    llama_backend_init();

    // Set up model parameters
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = 0; // CPU only (0 GPU layers)

    // Load model
    qDebug() << "Loading model...";
    llama_model *model =
        llama_load_model_from_file(model_path.c_str(), model_params);

    if (!model) {
      emit error("Failed to load model from: " + m_modelPath);
      llama_backend_free();
      return;
    }

    qDebug() << "Model loaded successfully";

    // Set up context parameters
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 2048;   // Context size
    ctx_params.n_batch = 2048; // Batch size for prompt processing (increased
                               // for larger context templates)
    ctx_params.n_threads = 4;  // Number of threads to use

    // Create context
    llama_context *ctx = llama_new_context_with_model(model, ctx_params);

    if (!ctx) {
      emit error("Failed to create llama context");
      llama_model_free(model);
      llama_backend_free();
      return;
    }

    qDebug() << "Context created successfully";

    // Get the vocabulary from the model
    const struct llama_vocab *vocab = llama_model_get_vocab(model);

    // Tokenize the prompt
    std::vector<llama_token> tokens_list;
    tokens_list.resize(prompt.size() + 1);

    int n_tokens = llama_tokenize(vocab, prompt.c_str(), prompt.size(),
                                  tokens_list.data(), tokens_list.size(),
                                  true, // add_bos (beginning of sequence)
                                  false // special tokens
    );

    if (n_tokens < 0) {
      tokens_list.resize(-n_tokens);
      n_tokens =
          llama_tokenize(vocab, prompt.c_str(), prompt.size(),
                         tokens_list.data(), tokens_list.size(), true, false);
    }

    tokens_list.resize(n_tokens);

    qDebug() << "Tokenized prompt:" << n_tokens << "tokens";

    // Prepare sampling parameters
    llama_sampler *sampler =
        llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(sampler,
                            llama_sampler_init_temp(0.7f)); // Temperature
    llama_sampler_chain_add(
        sampler, llama_sampler_init_top_p(0.9f, 1)); // Top-p sampling
    llama_sampler_chain_add(
        sampler, llama_sampler_init_dist(time(
                     NULL))); // Random distributions sampling (final selector)

    // Evaluate the prompt
    if (llama_decode(ctx, llama_batch_get_one(tokens_list.data(), n_tokens)) !=
        0) {
      emit error("Failed to evaluate prompt");
      llama_sampler_free(sampler);
      llama_free(ctx);
      llama_model_free(model);
      llama_backend_free();
      return;
    }

    qDebug() << "Starting generation...";

    // Generate tokens
    std::string result;
    const int max_tokens = 256; // Maximum tokens to generate
    int n_generated = 0;

    while (n_generated < max_tokens) {
      // Sample next token
      llama_token new_token = llama_sampler_sample(sampler, ctx, -1);

      // Check for end of sequence
      if (llama_vocab_is_eog(vocab, new_token)) {
        break;
      }

      // Convert token to text
      char buf[128];
      int n =
          llama_token_to_piece(vocab, new_token, buf, sizeof(buf), 0, false);

      if (n < 0) {
        emit error("Failed to convert token to text");
        break;
      }

      std::string piece(buf, n);
      result += piece;

      // Evaluate the new token
      if (llama_decode(ctx, llama_batch_get_one(&new_token, 1)) != 0) {
        emit error("Failed to evaluate token");
        break;
      }

      n_generated++;
    }

    qDebug() << "Generated" << n_generated << "tokens";
    qDebug() << "Result:" << QString::fromStdString(result);

    // Clean up
    llama_sampler_free(sampler);
    llama_free(ctx);
    llama_model_free(model);
    llama_backend_free();

    // Emit result
    if (!result.empty()) {
      emit finished(QString::fromStdString(result));
    } else {
      emit error("Generated empty result");
    }

  } catch (const std::exception &e) {
    emit error(QString("Exception during inference: %1").arg(e.what()));
  } catch (...) {
    emit error("Unknown error during inference");
  }

  qDebug() << "LlamaWorker: Inference complete";
}
