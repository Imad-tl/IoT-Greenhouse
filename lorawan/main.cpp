#include "mbed.h"
#include "lorawan/LoRaWANInterface.h"
#include "lorawan/system/lorawan_data_structures.h"
#include "events/EventQueue.h"
#include "trace_helper.h"
#include "lora_radio_helper.h"

// ============================================================
// Constantes
// ============================================================
#define CONFIRMED_MSG_RETRY_COUNTER     3
#define APP_TX_DUTYCYCLE                10000   // ms entre deux envois (duty-cycle OFF seulement)

// ============================================================
// UART debug PC + UART ESP32
// ============================================================
static BufferedSerial pc(USBTX, USBRX, 115200);
static BufferedSerial esp32(PA_9, PA_10, 115200);
static DigitalOut led(LED1);

// ============================================================
// LoRaWAN  (radio vient de lora_radio_helper.h)
// ============================================================
static EventQueue ev_queue(16 * EVENTS_EVENT_SIZE);
static LoRaWANInterface lorawan(radio);
static lorawan_app_callbacks_t callbacks;

// Buffers TX/RX
static uint8_t tx_buffer[64];
static uint8_t rx_buffer[64];
static int     tx_len = 0;         // nb d'octets à envoyer
static bool    tx_ready = false;   // un payload est prêt

// ============================================================
// Prototypes
// ============================================================
static void send_message();
static void receive_message();
static void lora_event_handler(lorawan_event_t event);

// ============================================================
// Lecture UART ESP32 — appelée depuis l'EventQueue
// ============================================================
static void read_esp32()
{
    if (!esp32.readable()) return;

    led = 1;
    int idx = 0;
    while (idx < (int)(sizeof(tx_buffer) - 1)) {
        char c;
        if (esp32.read(&c, 1) == 1) {
            tx_buffer[idx++] = (uint8_t)c;
            if (c == '\n') break;
        } else {
            break;
        }
    }

    if (idx > 0) {
        tx_len   = idx;
        tx_ready = true;

        // Log PC
        char msg[80];
        int n = snprintf(msg, sizeof(msg), "[UART] %d octets reçus, envoi LoRa...\r\n", idx);
        pc.write(msg, n);

        // Déclencher l'envoi LoRa
        ev_queue.call(send_message);
    }
    led = 0;
}

// ============================================================
// Envoi LoRaWAN
// ============================================================
static void send_message()
{
    if (!tx_ready || tx_len == 0) return;

    int16_t retcode = lorawan.send(MBED_CONF_LORA_APP_PORT,
                                   tx_buffer,
                                   tx_len,
                                   MSG_UNCONFIRMED_FLAG);

    if (retcode == LORAWAN_STATUS_WOULD_BLOCK) {
        pc.write("[LORA] Duty-cycle : réessai dans 3s\r\n", 38);
        ev_queue.call_in(3000, send_message);
        return;
    }

    if (retcode < 0) {
        char msg[60];
        int n = snprintf(msg, sizeof(msg), "[LORA] Erreur send() code=%d\r\n", retcode);
        pc.write(msg, n);
        return;
    }

    char msg[60];
    int n = snprintf(msg, sizeof(msg), "[LORA] %d octets planifiés\r\n", retcode);
    pc.write(msg, n);

    tx_ready = false;
    memset(tx_buffer, 0, sizeof(tx_buffer));
}

// ============================================================
// Réception downlink (optionnel, pour compléter l'exemple)
// ============================================================
static void receive_message()
{
    uint8_t port;
    int flags;
    int16_t retcode = lorawan.receive(rx_buffer, sizeof(rx_buffer), port, flags);

    if (retcode < 0) {
        char msg[50];
        int n = snprintf(msg, sizeof(msg), "[LORA] receive() erreur %d\r\n", retcode);
        pc.write(msg, n);
        return;
    }

    char msg[80];
    int n = snprintf(msg, sizeof(msg), "[LORA] RX port %u (%d octets) : ", port, retcode);
    pc.write(msg, n);
    for (int i = 0; i < retcode; i++) {
        char hex[4];
        int h = snprintf(hex, sizeof(hex), "%02x ", rx_buffer[i]);
        pc.write(hex, h);
    }
    pc.write("\r\n", 2);
    memset(rx_buffer, 0, sizeof(rx_buffer));
}

// ============================================================
// Gestionnaire d'événements LoRaWAN  (copié de l'exemple ARM)
// ============================================================
static void lora_event_handler(lorawan_event_t event)
{
    switch (event) {

        case CONNECTED:
            pc.write("[LORA] Connecté ! En attente de données ESP32...\r\n", 50);
            // Dès la connexion, on arme la lecture UART périodique
            ev_queue.call_every(200ms, read_esp32);
            break;

        case DISCONNECTED:
            pc.write("[LORA] Déconnecté\r\n", 20);
            ev_queue.break_dispatch();
            break;

        case TX_DONE:
            pc.write("[LORA] Trame envoyée avec succès\r\n", 34);
            break;

        case TX_TIMEOUT:
        case TX_ERROR:
        case TX_CRYPTO_ERROR:
        case TX_SCHEDULING_ERROR:
            pc.write("[LORA] Erreur TX\r\n", 19);
            break;

        case RX_DONE:
            pc.write("[LORA] Downlink reçu\r\n", 22);
            receive_message();
            break;

        case RX_TIMEOUT:
        case RX_ERROR:
            break;

        case JOIN_FAILURE:
            pc.write("[LORA] JOIN échoué - vérifiez vos clés mbed_app.json\r\n", 54);
            break;

        case UPLINK_REQUIRED:
            send_message();   // stack demande un uplink pour maintenir la session
            break;

        default:
            MBED_ASSERT("lora_event_handler : événement inconnu");
            break;
    }
}

// ============================================================
// Main
// ============================================================
int main(void)
{
    setup_trace();  // active les traces Mbed (optionnel, voir mbed_app.json)

    pc.write("=== STM32 LoRaWAN Bridge ESP32 -> TTN ===\r\n", 44);

    lorawan_status_t retcode;

    // Init stack
    if (lorawan.initialize(&ev_queue) != LORAWAN_STATUS_OK) {
        pc.write("[LORA] Erreur init stack\r\n", 26);
        return -1;
    }
    pc.write("[LORA] Stack initialisée\r\n", 26);

    // Callbacks
    callbacks.events = mbed::callback(lora_event_handler);
    lorawan.add_app_callbacks(&callbacks);

    // Retries pour messages CONFIRMED
    if (lorawan.set_confirmed_msg_retries(CONFIRMED_MSG_RETRY_COUNTER)
            != LORAWAN_STATUS_OK) {
        pc.write("[LORA] set_confirmed_msg_retries échoué\r\n", 41);
        return -1;
    }

    // ADR activé (recommandé TTN)
    if (lorawan.enable_adaptive_datarate() != LORAWAN_STATUS_OK) {
        pc.write("[LORA] enable_adaptive_datarate échoué\r\n", 40);
        return -1;
    }
    pc.write("[LORA] ADR activé, JOIN en cours...\r\n", 37);

    // Connexion OTAA (clés lues depuis mbed_app.json)
    retcode = lorawan.connect();
    if (retcode != LORAWAN_STATUS_OK &&
        retcode != LORAWAN_STATUS_CONNECT_IN_PROGRESS) {
        char msg[60];
        int n = snprintf(msg, sizeof(msg), "[LORA] connect() échoué code=%d\r\n", retcode);
        pc.write(msg, n);
        return -1;
    }

    // Boucle principale — tout passe par l'EventQueue
    ev_queue.dispatch_forever();
}