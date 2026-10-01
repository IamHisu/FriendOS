#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t board_audio_init(void);

esp_err_t board_microphone_read_stats(int32_t *min_sample, int32_t *max_sample, uint32_t *peak);

#ifdef __cplusplus
}
#endif