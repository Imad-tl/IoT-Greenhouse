#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* NimBLE */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "host/ble_att.h"

/* Crypto */
#include "mbedtls/aes.h"

static const char *TAG = "S3_GATEWAY";

// Configuration
#define SVC_UUID_GREENHOUSE 0x00FF
#define CHR_UUID_GREENHOUSE 0xFF01
#define MAX_NODES           3

static uint8_t PSK[16] = { 0x53, 0x65, 0x72, 0x72, 0x65, 0x49, 0x6F, 0x54, 0x32, 0x30, 0x32, 0x36, 0x4B, 0x65, 0x79, 0x21 };

typedef struct {
    char last_data[128];
    bool connected;
} node_data_t;

static node_data_t nodes[MAX_NODES];
static uint8_t g_own_addr_type;

// Prototypes
static void ble_app_scan(void);
static int ble_gap_event_handler(struct ble_gap_event *event, void *arg);

/* --- DÉCHIFFREMENT AES-128-CBC --- */
void decrypt_and_process(const uint8_t *payload, int len) {
    ESP_LOGI(TAG, "Notification reçue : %d octets", len);

    // Seuil de 32 octets pour l'AES (16 IV + 16 Ciphertext min)
    if (len >= 32) {
        uint8_t iv[16];
        memcpy(iv, payload, 16); // 16 premiers octets = IV
        
        int cipher_len = len - 16;
        uint8_t *decrypted = malloc(cipher_len + 1);
        
        mbedtls_aes_context aes;
        mbedtls_aes_init(&aes);
        mbedtls_aes_setkey_dec(&aes, PSK, 128);
        
        // Déchiffrement du bloc (le ciphertext commence à l'octet 16)
        int ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, cipher_len, iv, payload + 16, decrypted);
        mbedtls_aes_free(&aes);

        if (ret == 0) {
            // Retrait du padding PKCS7
            uint8_t pad = decrypted[cipher_len - 1];
            if (pad > 0 && pad <= 16) {
                int plain_len = cipher_len - pad;
                decrypted[plain_len] = '\0';
                char *msg = (char*)decrypted;

                ESP_LOGI(TAG, "Déchiffré : %s", msg);

                // Identification du nœud
                int node_idx = -1;
                if (strstr(msg, "ID=1")) node_idx = 0;
                else if (strstr(msg, "ID=2")) node_idx = 1;
                else if (strstr(msg, "ID=3")) node_idx = 2;

                if (node_idx != -1) {
                    strncpy(nodes[node_idx].last_data, msg, 127);
                }
            } else {
                ESP_LOGE(TAG, "Erreur Padding PKCS7 (val: %d)", pad);
            }
        } else {
            ESP_LOGE(TAG, "Erreur mbedtls : %d", ret);
        }
        free(decrypted);
    } 
    else {
        // Fallback texte clair si < 32 octets
        char raw_text[64] = {0};
        memcpy(raw_text, payload, (len < 63) ? len : 63);
        ESP_LOGW(TAG, "Texte clair reçu : %s", raw_text);
    }
}

/* --- LOGIQUE GATT --- */
static int on_disc_cccd(uint16_t conn_handle, const struct ble_gatt_error *error,
                        uint16_t service_handle, const struct ble_gatt_dsc *dsc, void *arg) {
    if (error->status == 0 && dsc != NULL) {
        uint8_t value[2] = {0x01, 0x00};
        ble_gattc_write_flat(conn_handle, dsc->handle, value, 2, NULL, NULL);
        ESP_LOGI(TAG, "Notifications activées");
    }
    return 0;
}

static int on_disc_char(uint16_t conn_handle, const struct ble_gatt_error *error,
                        const struct ble_gatt_chr *chr, void *arg) {
    if (error->status == 0 && chr != NULL) {
        ble_gattc_disc_all_dscs(conn_handle, chr->val_handle, 0xFFFF, on_disc_cccd, NULL);
    }
    return 0;
}

static int on_disc_svc(uint16_t conn_handle, const struct ble_gatt_error *error,
                       const struct ble_gatt_svc *svc, void *arg) {
    if (error->status == 0 && svc != NULL) {
        ble_gattc_disc_chrs_by_uuid(conn_handle, svc->start_handle, svc->end_handle, 
                                    BLE_UUID16_DECLARE(CHR_UUID_GREENHOUSE), on_disc_char, NULL);
    }
    return 0;
}

/* --- GESTIONNAIRE D'ÉVÉNEMENTS GAP --- */
static int ble_gap_event_handler(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_DISC:
            struct ble_hs_adv_fields fields;
            ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
            if (fields.name_len >= 13 && strncmp((char *)fields.name, "GreenHouse-C6-", 14) == 0) {
                ble_gap_disc_cancel();
                ble_gap_connect(g_own_addr_type, &event->disc.addr, 30000, NULL, ble_gap_event_handler, NULL);
            }
            break;

        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ESP_LOGI(TAG, "Connecté ! Négociation MTU...");
                // Négociation MTU
                ble_gattc_exchange_mtu(event->connect.conn_handle, NULL, NULL);
                // Découverte des services
                ble_gattc_disc_svc_by_uuid(event->connect.conn_handle, BLE_UUID16_DECLARE(SVC_UUID_GREENHOUSE), on_disc_svc, NULL);
            } else {
                ble_app_scan();
            }
            break;

        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "MTU négocié : %d octets (handle: %d)", event->mtu.value, event->mtu.conn_handle);
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGW(TAG, "Déconnecté. Scan relancé.");
            ble_app_scan();
            break;

        case BLE_GAP_EVENT_NOTIFY_RX:
            uint8_t buf[256];
            uint16_t len;
            ble_hs_mbuf_to_flat(event->notify_rx.om, buf, sizeof(buf), &len);
            decrypt_and_process(buf, len);
            break;
    }
    return 0;
}

static void ble_app_scan(void) {
    struct ble_gap_disc_params d_params = { .filter_duplicates = 1, .passive = 0 };
    ble_gap_disc(g_own_addr_type, BLE_HS_FOREVER, &d_params, ble_gap_event_handler, NULL);
}

static void on_sync(void) {
    ble_hs_id_infer_auto(0, &g_own_addr_type);
    ble_app_scan();
}

void ble_host_task(void *param) {
    nimble_port_run();
}

void monitor_task(void *pv) {
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        printf("\n--- RÉSUMÉ CAPTEURS ---\n");
        for(int i=0; i<MAX_NODES; i++) {
            printf("Node %d: %s\n", i+1, nodes[i].last_data[0] ? nodes[i].last_data : "---");
        }
    }
}

void app_main(void) {
    nvs_flash_init();
    nimble_port_init();

    // Configuration MTU Préféré (Important !)
    ble_att_set_preferred_mtu(256);

    ble_hs_cfg.sync_cb = on_sync;
    
    for(int i=0; i<MAX_NODES; i++) memset(nodes[i].last_data, 0, 128);

    nimble_port_freertos_init(ble_host_task);
    xTaskCreate(monitor_task, "monitor", 4096, NULL, 5, NULL);
}