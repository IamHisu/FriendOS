#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize FriendOS user interface.
 */
esp_err_t friend_ui_init(void);

#ifdef __cplusplus
}
#endif