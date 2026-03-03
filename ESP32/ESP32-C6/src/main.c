/*
 * ESP32-C6 — Serre connectee
 *
 * 3 roles possibles (compile-time via SENSOR_ROLE dans platformio.ini):
 *   ROLE_TEMP_HUM (1) : Recoit T/H via UART depuis Arduino Mega (GPIO4 RX, GPIO5 TX)
 *   ROLE_LUM      (2) : Lit photoresistance directement sur GPIO4 (ADC)
 *   ROLE_MOTION   (3) : Lit capteur HC-SR501 directement sur GPIO4 (GPIO digital)
 *
 * Chaque role transmet ses donnees en BLE notify vers le S3.
 * BLE : NimBLE GATT server, service 0x00FF, char 0xFF01
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

/* Roles possibles */
#define ROLE_TEMP_HUM  1
#define ROLE_LUM       2
#define ROLE_MOTION    3

/* Defaults (overridden par build_flags dans platformio.ini) */
#ifndef NODE_ID
#define NODE_ID 1
#endif
#ifndef SENSOR_ROLE
#define SENSOR_ROLE ROLE_TEMP_HUM
#endif

/* Includes specifiques au role */
#if SENSOR_ROLE == ROLE_TEMP_HUM
#include "driver/uart.h"
#elif SENSOR_ROLE == ROLE_LUM
#include "esp_adc/adc_oneshot.h"
#elif SENSOR_ROLE == ROLE_MOTION
#include "driver/gpio.h"
#endif

static const char *TAG = "SERRE";

/* ===================== CONFIG CAPTEURS ===================== */
#if SENSOR_ROLE == ROLE_TEMP_HUM
  /* UART1 vers Arduino Mega */
  #define UART_PORT  UART_NUM_1
  #define TX_PIN     5
  #define RX_PIN     4
  #define BAUD       9600
  #define BUF        256

#elif SENSOR_ROLE == ROLE_LUM
  /* Photoresistance sur GPIO4 (ADC1_CH4) */
  #define LDR_CHANNEL    ADC_CHANNEL_4
  #define LUM_THRESHOLD  1000    /* Seuil LUMINEUX/SOMBRE (12-bit: 0-4095) */
  #define LUM_PERIOD_MS  2000    /* Lecture toutes les 2 secondes */

#elif SENSOR_ROLE == ROLE_MOTION 
  /* HC-SR501 sur GPIO4 */
  #define PIR_GPIO         4
  #define PIR_POLL_MS      200    /* Polling toutes les 200ms */
  #define PIR_LOW_HOLD_MS  3000   /* GPIO doit rester LOW 3s avant MOTION=0 */
#endif

/* ===================== BLE ===================== */
static const ble_uuid16_t svc_uuid  = BLE_UUID16_INIT(0x00FF);
static const ble_uuid16_t uuid_data = BLE_UUID16_INIT(0xFF01);

static uint16_t h_data;
static uint16_t g_conn = 0;
static bool     g_connected = false;
static uint8_t  g_addr_type = 0;
static char     g_last_payload[128] = "";

static int  gap_event_cb(struct ble_gap_event *event, void *arg);
static void start_advertising(void);

static int on_read(uint16_t conn, uint16_t attr,
                   struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) return BLE_ATT_ERR_UNLIKELY;
    os_mbuf_append(ctxt->om, g_last_payload, strlen(g_last_payload));
    return 0;
}

static const struct ble_gatt_svc_def services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            { .uuid = &uuid_data.u, .access_cb = on_read, .val_handle = &h_data,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY },
            { 0 }
        },
    },
    { 0 }
};

static void start_advertising(void)
{
    struct ble_hs_adv_fields fields = {0};
    fields.flags            = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name             = (uint8_t *)ble_svc_gap_device_name();
    fields.name_len         = strlen(ble_svc_gap_device_name());
    fields.name_is_complete = 1;
    fields.uuids16          = (ble_uuid16_t[]){ svc_uuid };
    fields.num_uuids16      = 1;
    fields.uuids16_is_complete = 1;

    if (ble_gap_adv_set_fields(&fields) != 0) {
        fields.name = NULL; fields.name_len = 0; fields.name_is_complete = 0;
        ble_gap_adv_set_fields(&fields);
    }

    struct ble_gap_adv_params params = {0};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    ble_gap_adv_start(g_addr_type, NULL, BLE_HS_FOREVER, &params, gap_event_cb, NULL);
    ESP_LOGI(TAG, "Advertising...");
}

static int gap_event_cb(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            g_conn = event->connect.conn_handle;
            g_connected = true;
            ESP_LOGI(TAG, "Client BLE connecte");
        } else {
            ESP_LOGW(TAG, "Connexion BLE echouee");
            start_advertising();
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        g_connected = false;
        ESP_LOGI(TAG, "Client BLE deconnecte");
        start_advertising();
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        start_advertising();
        break;
    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "Notifications BLE activees");
        break;
    default: break;
    }
    return 0;
}

static void on_ble_ready(void)
{
    ble_hs_id_infer_auto(0, &g_addr_type);
    ESP_LOGI(TAG, "BLE pret");
    start_advertising();
}

static void on_ble_error(int reason)
{
    ESP_LOGE(TAG, "Erreur BLE (code %d)", reason);
}

static void ble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void ble_notify(const char *payload)
{
    strncpy(g_last_payload, payload, sizeof(g_last_payload) - 1);
    g_last_payload[sizeof(g_last_payload) - 1] = '\0';
    if (!g_connected) return;
    struct os_mbuf *om = ble_hs_mbuf_from_flat(payload, strlen(payload));
    if (om) ble_gatts_notify_custom(g_conn, h_data, om);
}

static void ble_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    nimble_port_init();
    ble_hs_cfg.sync_cb  = on_ble_ready;
    ble_hs_cfg.reset_cb = on_ble_error;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_gatts_count_cfg(services);
    ble_gatts_add_svcs(services);
    static char ble_name[32];
    snprintf(ble_name, sizeof(ble_name), "GreenHouse-C6-%d", NODE_ID);
    ble_svc_gap_device_name_set(ble_name);

    nimble_port_freertos_init(ble_host_task);
    ESP_LOGI(TAG, "BLE init (nom: %s)", ble_name);
}

/* ============================================================
 *  CAPTEUR : code specifique a chaque role
 * ============================================================ */

#if SENSOR_ROLE == ROLE_TEMP_HUM
/* ---------- TEMPERATURE / HUMIDITE via UART Arduino ---------- */

static float g_temp = 0;
static float g_hum  = 0;

static void sensor_init(void)
{
    const uart_config_t cfg = {
        .baud_rate = BAUD, .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_PORT, BUF * 2, BUF * 2, 0, NULL, 0);
    uart_param_config(UART_PORT, &cfg);
    uart_set_pin(UART_PORT, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    ESP_LOGI(TAG, "UART1 init (RX=GPIO%d TX=GPIO%d %d baud)", RX_PIN, TX_PIN, BAUD);
}

static void parse_line(const char *line)
{
    const char *p;
    if ((p = strstr(line, "T=")))  g_temp = strtof(p + 2, NULL);
    if ((p = strstr(line, ";H="))) g_hum  = strtof(p + 3, NULL);
}

static void sensor_task(void *arg)
{
    uint8_t buf[BUF];
    char line[BUF];
    int idx = 0;

    while (1) {
        int len = uart_read_bytes(UART_PORT, buf, sizeof(buf) - 1, pdMS_TO_TICKS(100));
        for (int i = 0; i < len; i++) {
            char c = buf[i];
            if (c == '\n' || c == '\r') {
                if (idx > 0) {
                    line[idx] = '\0';
                    parse_line(line);

                    char payload[128];
                    snprintf(payload, sizeof(payload),
                             "ID=%d;T=%.1f;H=%.0f",
                             NODE_ID, g_temp, g_hum);
                    ESP_LOGI(TAG, "%s [BLE:%s]", payload,
                             g_connected ? "connecte" : "en attente");
                    ble_notify(payload);
                    idx = 0;
                }
            } else if (idx < BUF - 1) {
                line[idx++] = c;
            }
        }
    }
}

#elif SENSOR_ROLE == ROLE_LUM
/* ---------- LUMINOSITE via photoresistance (ADC direct) ---------- */

static adc_oneshot_unit_handle_t adc_handle;
static uint16_t g_seq = 0;

static void sensor_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&unit_cfg, &adc_handle);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    adc_oneshot_config_channel(adc_handle, LDR_CHANNEL, &chan_cfg);
    ESP_LOGI(TAG, "ADC init (GPIO4, CH4, 12-bit, seuil=%d)", LUM_THRESHOLD);
}

static void sensor_task(void *arg)
{
    int raw;

    while (1) {
        adc_oneshot_read(adc_handle, LDR_CHANNEL, &raw);
        g_seq++;

        const char *etat = (raw > LUM_THRESHOLD) ? "LUMINEUX" : "SOMBRE";

        char payload[128];
        snprintf(payload, sizeof(payload),
                 "ID=%d;SEQ=%u;LUM=%d;ETAT=%s",
                 NODE_ID, g_seq, raw, etat);
        ESP_LOGI(TAG, "%s [BLE:%s]", payload,
                 g_connected ? "connecte" : "en attente");
        ble_notify(payload);

        vTaskDelay(pdMS_TO_TICKS(LUM_PERIOD_MS));
    }
}

#elif SENSOR_ROLE == ROLE_MOTION
/* ---------- MOUVEMENT via HC-SR501 (GPIO digital) ----------
 *
 * Le HC-SR501 pulse brievement LOW entre deux detections meme
 * si quelqu'un est toujours devant. Pour eviter les faux MOTION=0 :
 *   - MOTION=1 : des que GPIO passe HIGH (instantane)
 *   - MOTION=0 : seulement si GPIO reste LOW de facon CONTINUE
 *                pendant PIR_LOW_HOLD_MS (3 sec par defaut)
 * Ainsi la main/personne devant le capteur garde MOTION=1.
 */

static uint16_t g_seq = 0;
static int motion_reported = 0;  /* 0 = aucun mouvement signale, 1 = MOTION=1 envoye */

static void sensor_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PIR_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    ESP_LOGI(TAG, "PIR init (GPIO%d)", PIR_GPIO);
}

static void sensor_task(void *arg)
{
    /* Stabilisation du HC-SR501 au demarrage (~10 sec) */
    ESP_LOGI(TAG, "Stabilisation capteur PIR (10 sec)...");
    vTaskDelay(pdMS_TO_TICKS(10000));
    ESP_LOGI(TAG, "PIR pret, en attente de mouvement");

    TickType_t low_since = 0;   /* tick ou le GPIO est passe LOW */
    bool       counting = false; /* true = on compte le temps LOW */

    while (1) {
        int level = gpio_get_level(PIR_GPIO);

        if (level) {
            /* GPIO HIGH : mouvement en cours */
            counting = false;  /* reset du compteur LOW */

            if (!motion_reported) {
                /* Premier front HIGH -> notifier MOTION=1 */
                motion_reported = 1;
                g_seq++;

                char payload[128];
                snprintf(payload, sizeof(payload),
                         "ID=%d;SEQ=%u;MOTION=1",
                         NODE_ID, g_seq);
                ESP_LOGI(TAG, "Mouvement detecte !");
                ESP_LOGI(TAG, "%s [BLE:%s]", payload,
                         g_connected ? "connecte" : "en attente");
                ble_notify(payload);
            }
        } else {
            /* GPIO LOW */
            if (motion_reported) {
                if (!counting) {
                    /* Debut du compteur LOW */
                    counting = true;
                    low_since = xTaskGetTickCount();
                } else if ((xTaskGetTickCount() - low_since) >= pdMS_TO_TICKS(PIR_LOW_HOLD_MS)) {
                    /* LOW stable depuis PIR_LOW_HOLD_MS -> plus de mouvement */
                    motion_reported = 0;
                    counting = false;
                    g_seq++;

                    char payload[128];
                    snprintf(payload, sizeof(payload),
                             "ID=%d;SEQ=%u;MOTION=0",
                             NODE_ID, g_seq);
                    ESP_LOGI(TAG, "Plus de mouvement (LOW stable %d ms)", PIR_LOW_HOLD_MS);
                    ESP_LOGI(TAG, "%s [BLE:%s]", payload,
                             g_connected ? "connecte" : "en attente");
                    ble_notify(payload);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(PIR_POLL_MS));
    }
}

#endif /* SENSOR_ROLE */

/* ===================== MAIN ===================== */
#if SENSOR_ROLE == ROLE_TEMP_HUM
  #define ROLE_STR "TEMP_HUM"
#elif SENSOR_ROLE == ROLE_LUM
  #define ROLE_STR "LUM"
#elif SENSOR_ROLE == ROLE_MOTION
  #define ROLE_STR "MOTION"
#else
  #define ROLE_STR "ALL"
#endif

void app_main(void)
{
    ESP_LOGI(TAG, "=== SERRE CONNECTEE - NODE %d - ROLE %s ===", NODE_ID, ROLE_STR);

    sensor_init();
    xTaskCreate(sensor_task, "sensor", 4096, NULL, 10, NULL);

    ble_init();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}