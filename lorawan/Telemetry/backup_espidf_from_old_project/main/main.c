#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"

static const char *TAG = "UART_RX";

// =====================
// CONFIGURATION UART — ADAPTER SELON TON CABLAGE
// =====================
// GPIO connecté au TX1 de l'Arduino Mega (pin 18)
#define UART_RX_GPIO        7
// GPIO TX (optionnel)
#define UART_TX_GPIO        UART_PIN_NO_CHANGE
// Même baud que Serial1 sur l'Arduino
#define UART_BAUD_RATE      9600
// UART port (UART1, car UART0 = USB/console)
#define UART_PORT           UART_NUM_1

#define UART_BUF_SIZE       256

// Test rapide : lit le niveau logique du GPIO RX pendant 1 seconde
// Si le pin est flottant (rien de branché) -> toujours 0 ou toujours 1
// Si l'Arduino envoie des données -> on verra des transitions 0/1
static void gpio_level_test(int gpio_num)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_num),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    int high = 0, low = 0;
    for (int i = 0; i < 1000; i++) {
        if (gpio_get_level(gpio_num))
            high++;
        else
            low++;
        esp_rom_delay_us(1000); // 1ms
    }

    ESP_LOGI(TAG, "=== TEST GPIO%d raw : HIGH=%d  LOW=%d (sur 1000 lectures) ===", gpio_num, high, low);
    if (high == 1000)
        ESP_LOGW(TAG, "GPIO%d TOUJOURS HIGH -> pin flottant avec pull-up OU pas de signal", gpio_num);
    else if (low == 1000)
        ESP_LOGW(TAG, "GPIO%d TOUJOURS LOW -> rien de branche OU pas de signal", gpio_num);
    else
        ESP_LOGI(TAG, "GPIO%d reçoit des transitions -> signal detecte !", gpio_num);

    // Reset le pin pour l'UART
    gpio_reset_pin(gpio_num);
}

void app_main(void)
{
    ESP_LOGI(TAG, "==========================================");
    ESP_LOGI(TAG, "Test de signal sur GPIO%d avant init UART...", UART_RX_GPIO);
    ESP_LOGI(TAG, "==========================================");

    // D'abord tester si on voit un signal sur le GPIO
    gpio_level_test(UART_RX_GPIO);

    // Configuration UART
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_GPIO, UART_RX_GPIO,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "UART%d init OK — RX=GPIO%d @ %d baud", UART_PORT, UART_RX_GPIO, UART_BAUD_RATE);
    ESP_LOGI(TAG, "  Cablage attendu : TX1 Arduino (pin 18) --> GPIO%d ESP32-C6", UART_RX_GPIO);
    ESP_LOGI(TAG, "  GND Arduino <--> GND ESP32-C6");
    ESP_LOGI(TAG, "En attente de donnees...");

    uint8_t buf[UART_BUF_SIZE];
    int tick = 0;

    while (1) {
        int len = uart_read_bytes(UART_PORT, buf, sizeof(buf) - 1, pdMS_TO_TICKS(1000));
        if (len > 0) {
            buf[len] = '\0';
            printf("%s", (char *)buf);
            tick = 0;
        } else {
            tick++;
            if (tick % 10 == 0) {
                ESP_LOGW(TAG, "Rien recu depuis %d sec — verifier cablage", tick);
                // Refaire le test GPIO pour voir si quelque chose a changé
                uart_driver_delete(UART_PORT);
                gpio_level_test(UART_RX_GPIO);
                ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
                ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
                ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_GPIO, UART_RX_GPIO,
                                             UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
            }
        }
    }
}
