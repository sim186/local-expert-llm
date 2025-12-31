#include "llamaworker.h"
#include <QDebug>
#include <QFile>
#include <QElapsedTimer>
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

LlamaWorker::LlamaWorker(QObject *parent)
    : QObject(parent), m_model(nullptr), m_ctx(nullptr), m_stopRequested(false) {
    // Initialize backend once
    llama_backend_init();
}

LlamaWorker::~LlamaWorker() {
    freeModel();
    llama_backend_free();
}

bool LlamaWorker::isModelLoaded() const {
    return m_model != nullptr;
}

void LlamaWorker::freeModel() {
    if (m_ctx) {
        llama_free(m_ctx);
        m_ctx = nullptr;
    }
    if (m_model) {
        llama_model_free(m_model);
        m_model = nullptr;
    }
}

void LlamaWorker::stop() {
    m_stopRequested = true;
}

void LlamaWorker::loadModel(const LlamaParams &params) {
    // Check if we need to reload
    if (m_model && params.modelPath == m_currentParams.modelPath && 
        params.contextSize == m_currentParams.contextSize &&
        params.gpuLayers == m_currentParams.gpuLayers) {
        // Model already loaded with same critical params
        emit modelLoaded();
        return;
    }

    emit statusUpdate("Loading model...");
    freeModel();

    m_currentParams = params;
    std::string model_path = params.modelPath.toStdString();

    if (!QFile::exists(params.modelPath)) {
        emit error("Model file not found: " + params.modelPath);
        return;
    }

    try {
        llama_model_params model_params = llama_model_default_params();
        model_params.n_gpu_layers = params.gpuLayers;

        m_model = llama_model_load_from_file(model_path.c_str(), model_params);
        if (!m_model) {
            emit error("Failed to load model from file.");
            return;
        }

        // Create context immediately to reserve memory
        llama_context_params ctx_params = llama_context_default_params();
        ctx_params.n_ctx = params.contextSize;
        ctx_params.n_batch = 2048;
        ctx_params.n_threads = params.threads;

        m_ctx = llama_init_from_model(m_model, ctx_params);
        if (!m_ctx) {
            llama_model_free(m_model);
            m_model = nullptr;
            emit error("Failed to create llama context.");
            return;
        }

        emit modelLoaded();
        emit statusUpdate("Model loaded successfully.");

    } catch (...) {
        emit error("Exception while loading model.");
    }
}

void LlamaWorker::generate(const QString &prompt, const LlamaParams &genParams) {
    // Ensure correct model is loaded
    loadModel(genParams);
    if (!m_model) return; // Error already emitted

    m_stopRequested = false;
    emit statusUpdate("Generating...");

    QElapsedTimer timer;
    timer.start();

    try {
        std::string promptStr = prompt.toStdString();
        const struct llama_vocab *vocab = llama_model_get_vocab(m_model);

        // 1. Tokenize
        std::vector<llama_token> tokens_list(promptStr.size() + 1);
        int n_tokens = llama_tokenize(vocab, promptStr.c_str(), promptStr.size(), 
                                     tokens_list.data(), tokens_list.size(), true, true);
        if (n_tokens < 0) {
            tokens_list.resize(-n_tokens);
             n_tokens = llama_tokenize(vocab, promptStr.c_str(), promptStr.size(), 
                                     tokens_list.data(), tokens_list.size(), true, true);
        }
        tokens_list.resize(n_tokens);

        // Check context size
        int n_ctx = llama_n_ctx(m_ctx);
        if (n_tokens >= n_ctx) {
            emit error(QString("Prompt too long (%1 tokens) for context size (%2). Increase context size.")
                       .arg(n_tokens).arg(n_ctx));
            return;
        }

        // 2. Clear KV cache for new prompt
        llama_memory_t mem = llama_get_memory(m_ctx);
        llama_memory_seq_rm(mem, -1, -1, -1);

        // 3. Decode Prompt
        int n_batch = 2048; // Should match context params
        for (int i = 0; i < n_tokens; i += n_batch) {
            if (m_stopRequested) { emit finished(""); return; }
            
            int n_eval = std::min(n_batch, n_tokens - i);
            if (llama_decode(m_ctx, llama_batch_get_one(tokens_list.data() + i, n_eval)) != 0) {
                emit error("Failed to decode prompt.");
                return;
            }
        }

        // 4. Sampler Setup
        llama_sampler *sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
        llama_sampler_chain_add(sampler, llama_sampler_init_penalties(64, 1.1f, 0.0f, 0.0f));
        llama_sampler_chain_add(sampler, llama_sampler_init_top_p(genParams.topP, 1));
        llama_sampler_chain_add(sampler, llama_sampler_init_temp(genParams.temperature));
        llama_sampler_chain_add(sampler, llama_sampler_init_dist(time(NULL)));

        // 5. Generation Loop
        std::string result;
        int n_generated = 0;
        int max_tokens = 1024; // Could be a param

        while (n_generated < max_tokens && !m_stopRequested) {
            llama_token new_token = llama_sampler_sample(sampler, m_ctx, -1);

            if (llama_vocab_is_eog(vocab, new_token)) {
                break;
            }

            char buf[128];
            int n = llama_token_to_piece(vocab, new_token, buf, sizeof(buf), 0, true);
            if (n < 0) {
                // Buffer too small or error
                break; 
            }
            
            std::string piece(buf, n);
            // Simple fix for leading space on first token if needed, 
            // though usually handled by tokenizer/detokenizer logic
            if (n_generated == 0 && piece[0] == ' ') piece = piece.substr(1);
            
            result += piece;

            if (llama_decode(m_ctx, llama_batch_get_one(&new_token, 1)) != 0) {
                break;
            }
            n_generated++;
        }

        llama_sampler_free(sampler);

        if (m_stopRequested) {
            emit statusUpdate("Generation stopped.");
        } else {
            emit finished(QString::fromStdString(result));
            emit statsReady(timer.elapsed() / 1000.0f);
            emit statusUpdate("Ready");
        }

    } catch (const std::exception &e) {
        emit error(QString("Error during generation: %1").arg(e.what()));
    } catch (...) {
        emit error("Unknown error during generation.");
    }
}
