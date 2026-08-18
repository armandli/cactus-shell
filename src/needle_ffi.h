#ifndef NEEDLE_FFI_H
#define NEEDLE_FFI_H

#include <stddef.h>
#include <stdint.h>

// Subset of cactus-engine's C FFI that this project uses, redeclared here
// rather than including <cactus_engine.h>. That header pulls in cactus_graph.h,
// which includes <arm_neon.h> unconditionally and declares its own C++
// `namespace cactus` that would collide with ours. The declarations below must
// stay byte-compatible with cactus-engine/cactus_engine.h upstream.

extern "C" {

typedef void* cactus_model_t;

typedef void (*cactus_token_callback)(
    const char* token,
    uint32_t token_id,
    void* user_data);

cactus_model_t cactus_init(
    const char* model_path,
    const char* corpus_dir,
    bool cache_index);

void cactus_destroy(cactus_model_t model);
void cactus_reset(cactus_model_t model);
void cactus_stop(cactus_model_t model);

int cactus_complete(
    cactus_model_t model,
    const char* messages_json,
    char* response_buffer,
    size_t buffer_size,
    const char* options_json,
    const char* tools_json,
    cactus_token_callback callback,
    void* user_data,
    const uint8_t* pcm_buffer,
    size_t pcm_buffer_size);

const char* cactus_get_last_error(void);

}  // extern "C"

#endif
