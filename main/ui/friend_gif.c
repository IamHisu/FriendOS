#include "friend_gif.h"

#include <limits.h>
#include <string.h>

#include "libs/gif/AnimatedGIF/src/AnimatedGIF.h"
#include "misc/cache/instance/lv_image_cache.h"

typedef struct
{
    GIFIMAGE gif;
    lv_image_dsc_t image;
    lv_timer_t *timer;
    lv_obj_t *object;
    uint8_t *framebuffer;
    bool has_frame;
    int32_t loops_remaining;
} friend_gif_t;

static void clear_previous_frame(friend_gif_t *player)
{
    GIFIMAGE *gif = &player->gif;
    if (!player->has_frame || ((gif->ucGIFBits >> 2) & 7) != 2) return;

    uint32_t width = gif->iCanvasWidth;
    uint32_t height = gif->iCanvasHeight;
    uint32_t x = gif->iX;
    uint32_t y = gif->iY;
    uint32_t w = gif->iWidth;
    uint32_t h = gif->iHeight;
    if (x >= width || y >= height) return;
    if (w > width - x) w = width - x;
    if (h > height - y) h = height - y;

    uint8_t *indices = player->framebuffer;
    uint8_t *pixels = indices + width * height;
    for (uint32_t row = y; row < y + h; row++)
    {
        size_t offset = (size_t)row * width + x;
        memset(indices + offset, gif->ucBackground, w);
        memset(pixels + offset * 4, 0, w * 4);
    }
}

static void next_frame(lv_timer_t *timer)
{
    friend_gif_t *player = lv_timer_get_user_data(timer);
    GIFIMAGE *gif = &player->gif;

    clear_previous_frame(player);
    if (player->has_frame && gif->GIFFile.iPos >= gif->GIFFile.iSize - 1)
    {
        memset(player->framebuffer, 0, player->image.header.w * player->image.header.h * 5);
    }

    int delay_ms = 0;
    int has_next = GIF_playFrame(gif, &delay_ms, player);
    if (GIF_getLastError(gif) != GIF_SUCCESS)
    {
        lv_timer_pause(timer);
        return;
    }

    player->has_frame = true;
    lv_image_cache_drop(&player->image);
    lv_obj_invalidate(player->object);

    if (has_next == 0 && player->loops_remaining > 0)
    {
        player->loops_remaining--;
        if (player->loops_remaining == 0)
        {
            lv_timer_pause(timer);
            return;
        }
    }
    lv_timer_set_period(timer, delay_ms > 0 ? delay_ms : 10);
}

static void player_deleted(lv_event_t *event)
{
    friend_gif_t *player = lv_event_get_user_data(event);
    lv_timer_delete(player->timer);
    lv_image_cache_drop(&player->image);
    GIF_close(&player->gif);
    lv_free(player->framebuffer);
    lv_free(player);
}

lv_obj_t *friend_gif_create(lv_obj_t *parent, uint8_t *data, size_t size)
{
    if (size > INT_MAX) return NULL;

    friend_gif_t *player = lv_malloc(sizeof(*player));
    if (player == NULL) return NULL;
    memset(player, 0, sizeof(*player));

    GIF_begin(&player->gif, GIF_PALETTE_RGB8888);
    if (!GIF_openRAM(&player->gif, data, (int)size, NULL)) goto fail;

    uint32_t width = GIF_getCanvasWidth(&player->gif);
    uint32_t height = GIF_getCanvasHeight(&player->gif);
    if (width == 0 || height == 0 || width > 240 || height > 240) goto fail_close;

    size_t pixel_count = (size_t)width * height;
    player->framebuffer = lv_malloc(pixel_count * 5);
    if (player->framebuffer == NULL) goto fail_close;
    memset(player->framebuffer, 0, pixel_count * 5);
    player->gif.pFrameBuffer = player->framebuffer;
    player->gif.ucDrawType = GIF_DRAW_COOKED;

    player->image = (lv_image_dsc_t){
        .header.magic = LV_IMAGE_HEADER_MAGIC,
        .header.flags = LV_IMAGE_FLAGS_MODIFIABLE,
        .header.cf = LV_COLOR_FORMAT_ARGB8888,
        .header.w = width,
        .header.h = height,
        .header.stride = width * 4,
        .data_size = pixel_count * 4,
        .data = player->framebuffer + pixel_count,
    };
    player->loops_remaining = GIF_getLoopCount(&player->gif);

    player->object = lv_image_create(parent);
    if (player->object == NULL) goto fail_buffer;
    lv_image_set_src(player->object, &player->image);
    player->timer = lv_timer_create(next_frame, 10, player);
    if (player->timer == NULL) goto fail_object;
    lv_obj_add_event_cb(player->object, player_deleted, LV_EVENT_DELETE, player);
    next_frame(player->timer);
    if (!player->has_frame)
    {
        lv_obj_delete(player->object);
        return NULL;
    }
    return player->object;

fail_object:
    lv_image_cache_drop(&player->image);
    lv_obj_delete(player->object);
fail_buffer:
    lv_free(player->framebuffer);
fail_close:
    GIF_close(&player->gif);
fail:
    lv_free(player);
    return NULL;
}
