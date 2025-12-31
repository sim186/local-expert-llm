# LLM Concepts for C++ Developers

This document explains the core concepts of Large Language Models (LLMs) and how they are implemented in `llama.cpp`.

## 1. The Model (`llama_model`)
The model is the static "brain" loaded from a file (e.g., `.gguf`). It contains the weights (parameters) trained on massive amounts of text.
- **GGUF**: A file format optimized for fast loading and mapping to memory.
- **Parameters**: The number of weights (e.g., 3B, 7B). More parameters = smarter but slower.
- **Quantization**: Reducing precision (e.g., from 16-bit float to 4-bit integer) to save RAM with minimal quality loss.

## 2. The Context (`llama_context`)
The context is the "working memory" of the model.
- **KV Cache**: Stores the computed keys and values for previous tokens so they don't need to be recomputed.
- **Context Size (`n_ctx`)**: The maximum number of tokens the model can remember at once (prompt + generation). If you exceed this, the model "forgets" the beginning.

## 3. Tokens
LLMs don't read text; they read tokens. A token is a chunk of text (word, part of a word, or character).
- **Tokenization**: Converting "Hello world" -> `[123, 456]`.
- **Detokenization**: Converting `[123, 456]` -> "Hello world".
- **Special Tokens**:
    - `<|begin_of_text|>`: Starts the sequence.
    - `<|eot_id|>`: End of turn (in chat).
    - `<|end_of_text|>`: Stops generation.

## 4. Logits & Sampling
When the model predicts the next token, it outputs a probability score (logit) for *every* possible token in its vocabulary (e.g., 32,000 tokens).
- **Greedy Sampling**: Picking the token with the highest probability. (Boring, repetitive).
- **Temperature**: Flattens or sharpens the probability distribution.
    - Low (0.1): Predictable, focused.
    - High (1.0): Creative, random.
- **Top-P (Nucleus Sampling)**: Considers only the top X% cumulative probability. Removes "garbage" low-probability tokens.

## 5. The Loop (Inference)
1.  **Prompt Processing**: The prompt is tokenized and fed into the model (Prefill).
2.  **Generation**:
    - The model predicts the next token.
    - The selected token is added to the context.
    - The process repeats until a stop token is found or the limit is reached.

## 6. C++ Implementation in this Project
- **`llama_load_model_from_file`**: Loads the `.gguf` file.
- **`llama_new_context_with_model`**: Allocates RAM for the KV cache.
- **`llama_decode`**: Runs the math to process tokens.
- **`llama_sampler_sample`**: Picks the next token based on Temp/Top-P.
