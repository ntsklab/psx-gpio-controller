#pragma once

#include "pico/stdlib.h"
#include "hardware/gpio.h"

// Pico のオンボード LED は GPIO 25
#define PICO_LED_PIN 25

// LED ステータスの状態パターン
typedef enum {
    PSX_LED_IDLE = 0,          // LED off - デバイス初期化待ち
    PSX_LED_READY = 1,         // 1回点滅 - PSXコントローラとして準備完了
    PSX_LED_POLL = 2,          // 2回点滅 - POLL受信（通常動作）
    PSX_LED_CONFIG = 3,        // 3回点滅 - CONFIG受信
    PSX_LED_ERROR = 4,         // 高速点滅 - エラー状態
} psx_led_status_t;

// LED 状態管理構造体
typedef struct {
    uint32_t last_update_ms;    // 最後に状態更新した時刻 (ms)
    psx_led_status_t current_status;
    uint8_t flash_count;        // 現在の点滅回数
    uint8_t flash_index;        // 点滅内のインデックス（ON/OFF判定用）
    bool led_on;                // LED 現在状態
} psx_led_context_t;

// グローバル LED コンテキスト
extern psx_led_context_t g_led_ctx;

// --- 公開関数 ---

// LED を初期化（GPIO 25 をセットアップ）
static inline void psx_led_init(void) {
    gpio_init(PICO_LED_PIN);
    gpio_set_dir(PICO_LED_PIN, GPIO_OUT);
    gpio_put(PICO_LED_PIN, 0);  // 初期状態 OFF
    
    g_led_ctx.last_update_ms = 0;
    g_led_ctx.current_status = PSX_LED_IDLE;
    g_led_ctx.flash_count = 0;
    g_led_ctx.flash_index = 0;
    g_led_ctx.led_on = false;
}

// LED ステータスを更新（通信イベント時に呼び出す）
static inline void psx_led_set_status(psx_led_status_t status) {
    if (g_led_ctx.current_status != status) {
        g_led_ctx.current_status = status;
        g_led_ctx.flash_count = 0;
        g_led_ctx.flash_index = 0;
        g_led_ctx.led_on = false;
        gpio_put(PICO_LED_PIN, 0);  // reset LED
        g_led_ctx.last_update_ms = time_us_32() / 1000;  // reset timer
    }
}

// LED を更新（メインループから定期的に呼び出す）
// パターン：
//   IDLE: LED off
//   READY: 1回点滅（200ms on, 200ms off）
//   POLL: 2回点滅（100ms on/off x2, 300ms pause）
//   CONFIG: 3回点滅（100ms on/off x3, 300ms pause）
//   ERROR: 高速点滅（50ms on/off）
static inline void psx_led_update(void) {
    uint32_t now_ms = time_us_32() / 1000;
    uint32_t elapsed = now_ms - g_led_ctx.last_update_ms;
    
    bool should_toggle = false;
    
    switch (g_led_ctx.current_status) {
        case PSX_LED_IDLE:
            // LED off - no update
            if (g_led_ctx.led_on) {
                gpio_put(PICO_LED_PIN, 0);
                g_led_ctx.led_on = false;
            }
            break;
            
        case PSX_LED_READY:
            // 1回点滅：200ms on, 200ms off
            if (elapsed >= 200) {
                g_led_ctx.led_on = !g_led_ctx.led_on;
                gpio_put(PICO_LED_PIN, g_led_ctx.led_on ? 1 : 0);
                g_led_ctx.last_update_ms = now_ms;
                if (!g_led_ctx.led_on && ++g_led_ctx.flash_count >= 1) {
                    g_led_ctx.flash_count = 0;  // repeat pattern
                }
            }
            break;
            
        case PSX_LED_POLL:
            // 2回点滅：100ms on/off x2, 300ms pause
            if (g_led_ctx.flash_count < 2) {
                // flashing phase
                if (elapsed >= 100) {
                    g_led_ctx.led_on = !g_led_ctx.led_on;
                    gpio_put(PICO_LED_PIN, g_led_ctx.led_on ? 1 : 0);
                    g_led_ctx.last_update_ms = now_ms;
                    if (!g_led_ctx.led_on) {
                        g_led_ctx.flash_count++;
                    }
                }
            } else {
                // pause phase
                if (elapsed >= 300) {
                    g_led_ctx.flash_count = 0;
                    g_led_ctx.led_on = false;
                    gpio_put(PICO_LED_PIN, 0);
                    g_led_ctx.last_update_ms = now_ms;
                }
            }
            break;
            
        case PSX_LED_CONFIG:
            // 3回点滅：100ms on/off x3, 300ms pause
            if (g_led_ctx.flash_count < 3) {
                // flashing phase
                if (elapsed >= 100) {
                    g_led_ctx.led_on = !g_led_ctx.led_on;
                    gpio_put(PICO_LED_PIN, g_led_ctx.led_on ? 1 : 0);
                    g_led_ctx.last_update_ms = now_ms;
                    if (!g_led_ctx.led_on) {
                        g_led_ctx.flash_count++;
                    }
                }
            } else {
                // pause phase
                if (elapsed >= 300) {
                    g_led_ctx.flash_count = 0;
                    g_led_ctx.led_on = false;
                    gpio_put(PICO_LED_PIN, 0);
                    g_led_ctx.last_update_ms = now_ms;
                }
            }
            break;
            
        case PSX_LED_ERROR:
            // 高速点滅：50ms on/off
            if (elapsed >= 50) {
                g_led_ctx.led_on = !g_led_ctx.led_on;
                gpio_put(PICO_LED_PIN, g_led_ctx.led_on ? 1 : 0);
                g_led_ctx.last_update_ms = now_ms;
            }
            break;
    }
}
