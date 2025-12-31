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
  try {
    std::string model_path = m_modelPath.toStdString();
    std::string prompt = m_prompt.toStdString();

    llama_backend_init();

    // 1. Model & Context Setup
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = 0; // Set to >0 if you have a GPU

    llama_model *model =
        llama_load_model_from_file(model_path.c_str(), model_params);
    if (!model) {
      emit error("Model load failed.");
      return;
    }

    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 4096;
    ctx_params.n_batch =
        2048; // Faster for modern CPUs, avoids frequent batch splits
    ctx_params.n_threads = 8; // Adjust based on your CPU cores

    llama_context *ctx = llama_new_context_with_model(model, ctx_params);
    const struct llama_vocab *vocab = llama_model_get_vocab(model);

    // 2. Tokenize with special tokens (True) to handle the Llama tags
    std::vector<llama_token> tokens_list(prompt.size() + 1);
    int n_tokens =
        llama_tokenize(vocab, prompt.c_str(), prompt.size(), tokens_list.data(),
                       tokens_list.size(), true, true);
    tokens_list.resize(n_tokens);

    // 3. Robust Sampler Chain
    llama_sampler *sampler =
        llama_sampler_chain_init(llama_sampler_chain_default_params());
    // Penalty: Prevents loops
    llama_sampler_chain_add(sampler,
                            llama_sampler_init_penalties(64, 1.1f, 0.0f, 0.0f));
    // Top-P: Cuts out the "garbage" tokens
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.95f, 1));
    // Temp: Controls creativity (low = professional)
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(0.2f));
    // Dist: Final selection
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(time(NULL)));

    // 4. Decode Prompt in chunks (to satisfy GGML_ASSERT(n_tokens_all <=
    // cparams.n_batch))
    for (int i = 0; i < n_tokens; i += ctx_params.n_batch) {
      int n_eval = std::min((int)ctx_params.n_batch, n_tokens - i);
      if (llama_decode(
              ctx, llama_batch_get_one(tokens_list.data() + i, n_eval)) != 0) {
        emit error("Failed to decode prompt chunk.");
        return;
      }
    }

    // 5. Generation Loop
    std::string result;
    llama_token new_token;
    int n_generated = 0;

    while (n_generated < 1024) {
      new_token = llama_sampler_sample(sampler, ctx, -1);

      // Stop if End of Generation (EOG)
      if (llama_vocab_is_eog(vocab, new_token))
        break;

      char buf[128];
      int n = llama_token_to_piece(vocab, new_token, buf, sizeof(buf), 0, true);
      if (n < 0)
        break;

      std::string piece(buf, n);

      // Clean leading space from first token
      if (n_generated == 0 && piece[0] == ' ')
        piece = piece.substr(1);

      result += piece;

      if (llama_decode(ctx, llama_batch_get_one(&new_token, 1)) != 0)
        break;
      n_generated++;
    }

    // 6. Cleanup & Exit
    llama_sampler_free(sampler);
    llama_free(ctx);
    llama_model_free(model);
    llama_backend_free();

    emit finished(QString::fromStdString(result));

  } catch (...) {
    emit error("An unknown error occurred during inference.");
  }
}
