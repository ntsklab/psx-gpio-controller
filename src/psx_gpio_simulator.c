#include <stdio.h>
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "controller_simulator.h"
#include "psxSPI.pio.h"

// シンプルなGPIO→PSXコントローラエミュレータ
// - 指定したGPIOピンを読み取り、PSXのボタンビットにマッピングします
// - 取得した入力は `PSXInputState` 構造体に格納され、`controller_simulator` が参照します
// - コントローラシミュレータはcore1で起動してPIOトランザクションに応答します

// --- ボタンGPIOマッピング ---
#define NUM_BUTTON_PINS 14
#include "psx_definitions.h"

static PSXInputState state;
static uint8_t btn_pins[NUM_BUTTON_PINS];
static int btn_count = 0;

// GPIOを読み取り `state.buttons1` / `state.buttons2` を更新します。
// ボタンはアクティブロー（押下時にGPIOが0）で配線されている想定です。
static void poll_buttons(void) {
    uint8_t b1 = 0x00; // buttons1 (higher bits: UP/RIGHT/DOWN/LEFT)
    uint8_t b2 = 0x00; // buttons2 (TRI/CIR/X/SQU + L1/R1/L2/R2 bits are separate)

    // 本プロジェクトのボット定義では、押下を1で表す箇所と0で表す箇所が混在します。
    // ここではPSXの典型的なフォーマット（アクティブロー）に合わせ、
    // 押下時に対応ビットを0にクリアする形でボタンバイトを構築します。

    // まず全ビットを1で初期化（未押下）
    b1 = 0xFF;
    b2 = 0xFF;

    // btn_pins[] のインデックスとPSXボタンの対応（下記は本ソースで採用している固定割当）:
    // 0: CIRCLE, 1: CROSS, 2: TRIANGLE, 3: SQUARE,
    // 4: L1, 5: R1, 6: L2, 7: R2,
    // 8: UP, 9: DOWN, 10: LEFT, 11: RIGHT,
    // 12: START, 13: SELECT

    // Face buttons
    if (btn_count > 0 && gpio_get(btn_pins[0]) == 0) b2 &= ~PSX_GAMEPAD_CIRCLE;
    if (btn_count > 1 && gpio_get(btn_pins[1]) == 0) b2 &= ~PSX_GAMEPAD_CROSS;
    if (btn_count > 2 && gpio_get(btn_pins[2]) == 0) b2 &= ~PSX_GAMEPAD_TRIANGLE;
    if (btn_count > 3 && gpio_get(btn_pins[3]) == 0) b2 &= ~PSX_GAMEPAD_SQUARE;

    // Shoulder buttons
    if (btn_count > 4 && gpio_get(btn_pins[4]) == 0) b2 &= ~PSX_GAMEPAD_L1;
    if (btn_count > 5 && gpio_get(btn_pins[5]) == 0) b2 &= ~PSX_GAMEPAD_R1;

    // Triggers (mapped into l2/r2 fields and also low nibble)
    if (btn_count > 6 && gpio_get(btn_pins[6]) == 0) state.l2 = 0xFF; else state.l2 = 0x00;
    if (btn_count > 7 && gpio_get(btn_pins[7]) == 0) state.r2 = 0xFF; else state.r2 = 0x00;
    if (btn_count > 6 && gpio_get(btn_pins[6]) == 0) b2 &= ~PSX_GAMEPAD_L2;
    if (btn_count > 7 && gpio_get(btn_pins[7]) == 0) b2 &= ~PSX_GAMEPAD_R2;

    // D-pad
    if (btn_count > 8 && gpio_get(btn_pins[8]) == 0)  b1 &= ~PSX_GAMEPAD_DPAD_UP;
    if (btn_count > 9 && gpio_get(btn_pins[9]) == 0)  b1 &= ~PSX_GAMEPAD_DPAD_DOWN;
    if (btn_count > 10 && gpio_get(btn_pins[10]) == 0) b1 &= ~PSX_GAMEPAD_DPAD_LEFT;
    if (btn_count > 11 && gpio_get(btn_pins[11]) == 0) b1 &= ~PSX_GAMEPAD_DPAD_RIGHT;

    // Start / Select
    if (btn_count > 12 && gpio_get(btn_pins[12]) == 0) b1 &= ~PSX_GAMEPAD_START;
    if (btn_count > 13 && gpio_get(btn_pins[13]) == 0) b1 &= ~PSX_GAMEPAD_SELECT;

    state.buttons1 = b1;
    state.buttons2 = b2;

    // アナログスティックは中央（0x80）に固定設定。GPIO/ADCでマッピングする場合はここを差し替えてください。
    state.lx = 0x80;
    state.ly = 0x80;
    state.rx = 0x80;
    state.ry = 0x80;
}

int main(void) {
    stdio_init_all();
    
    // 固定ボタンGPIO割当（ユーザー指定）
    // Mapping indexes: 0:CIRCLE,1:CROSS,2:TRIANGLE,3:SQUARE,
    // 4:L1,5:R1,6:L2,7:R2,
    // 8:UP,9:DOWN,10:LEFT,11:RIGHT,
    // 12:START,13:SELECT
    btn_pins[0] = 22; // Circle
    btn_pins[1] = 21; // Cross
    btn_pins[2] = 20; // Triangle
    btn_pins[3] = 19; // Square
    // L1/L2/R1/R2 mapping: GP14..GP11 -> L1=14, L2=13, R1=12, R2=11
    btn_pins[4] = 14; // L1
    btn_pins[5] = 12; // R1
    btn_pins[6] = 13; // L2
    btn_pins[7] = 11; // R2
    // D-pad: UP..RIGHT = GP18..GP15
    btn_pins[8]  = 18; // UP
    btn_pins[9]  = 17; // DOWN
    btn_pins[10] = 16; // LEFT
    btn_pins[11] = 15; // RIGHT
    // Start / Select
    btn_pins[12] = 26; // START
    btn_pins[13] = 27; // SELECT

    btn_count = NUM_BUTTON_PINS;
    for (int i = 0; i < btn_count; ++i) {
        gpio_init(btn_pins[i]);
        gpio_set_dir(btn_pins[i], GPIO_IN);
        gpio_pull_up(btn_pins[i]);
    }

    // 状態の初期化（ボタン未押下）
    state.buttons1 = 0xFF;
    state.buttons2 = 0xFF;
    state.lx = state.ly = state.rx = state.ry = 0x80;
    state.l2 = state.r2 = 0x00;

    // PSXデバイスを初期化（pio 0 を使用）。
    // 第3引数に `psx_device_main` を渡すことで、必要時にコントローラシミュレータが core1 を再起動できます。
    psx_device_init(0, &state, psx_device_main);

    // コントローラシミュレータを core1 上で起動。core1 はブロックして PIO トランザクションに応答します。
    multicore_launch_core1(psx_device_main);

    // メインコアはボタンをポーリングして共有状態を更新します。
    while (true) {
        poll_buttons();
    // 簡易デバウンス: 1 msごとにサンプリング
        sleep_ms(1);
    }

    return 0;
}
