#ifndef NEEDLE_FFI_H
#define NEEDLE_FFI_H

// Subset of the Needle 3 engine's C API that this project uses, redeclared
// here rather than including the needle.h shipped beside libneedle.a. That
// header uses the same NEEDLE_H include guard as our src/needle.h, and
// `#include <needle.h>` already resolves to ours, so the two cannot both be
// included. The declarations below must stay byte-compatible with the
// upstream header in https://huggingface.co/Cactus-Compute/needle3.
//
// The engine holds one process-global, non-thread-safe text model and
// conversation. Negative returns indicate failure; needle_last_error() then
// says why.

extern "C" {

int needle_load(const unsigned char* cact, unsigned long long n);

const char* needle_last_error(void);

int needle_init(
    const char* system_prompt,
    const char* tools_json,
    const char* tool_index_path);

int needle_complete(
    const char* input,
    const float* pcm,
    int samples,
    int max_new_tokens,
    char* out,
    int out_capacity);

void needle_reset(void);

}  // extern "C"

#endif
