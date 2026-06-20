#include "arduino_secrets.h"
#include <WiFi.h>
#include <ArduinoMqttClient.h>
#include <RPLidar.h>
#include <BasicTag.h>
#include <SparkplugNode.h>
#include "mbed.h"

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;
const char broker[] = "158.179.220.228";
int port = 1883;

const char* topico_lidar          = "arduino_emissor/lidar/scan";
const char* topico_workorder_DCMD = "spBv1.0/ESTG_Fabrica/DCMD/node_red_edge/WS2_Armazenamento";

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);
RPLidar    lidar;

#define RPLIDAR_MOTOR 3

// ── Sparkplug ─────────────────────────────────────────────
SparkplugNodeConfig* spNode = nullptr;

int32_t tag_roda1       = 0;
int32_t tag_roda2       = 0;
bool    tag_map_request = false;
int32_t tag_status      = 0;
bool    tag_obstaculo   = false;
int32_t tag_workstation = 0;

char    s1buf[64];
uint8_t s1idx = 0;

uint64_t getTimestampMs() {
    return (uint64_t)millis();
}

void publishMQTT(SparkplugNodeConfig* node) {
    mqttClient.beginMessage(node->mqtt_message.topic);
    mqttClient.write(node->mqtt_message.payload->buffer,
                     node->mqtt_message.payload->written_length);
    mqttClient.endMessage();
}

// ── Leitura do Serial1 (EV3 → GIGA) ───────────────────────
// Protocolo esperado do EV3:  V:left,right\n
char    s1buf[64];
uint8_t s1idx = 0;

void setStatus(const char* novo) {
    strncpy(statusBuf, novo, sizeof(statusBuf) - 1);
    statusBuf[sizeof(statusBuf) - 1] = '\0';   // garante terminador
    statusLen = strlen(statusBuf);
}

void processarTramaEv3(char* trama) {
    if (strncmp(trama, "MET,", 4) != 0) return;   // não é trama de telemetria

    char* campos[6];
    int n = 0;
    char* tok = strtok(trama, ",");
    while (tok != NULL && n < 6) {
        campos[n++] = tok;
        tok = strtok(NULL, ",");
    }
    if (n < 6) return;   // trama incompleta → ignora, não atualiza nada

    // [0]=MET [1]=status [2]=velEsq [3]=velDir [4]=obstaculo [5]=workstation
    tag_status      = atoi(campos[1]);
    tag_roda1       = atoi(campos[2]);
    tag_roda2       = atoi(campos[3]);
    tag_obstaculo   = (atoi(campos[4]) != 0);
    tag_workstation = wsParaCodigo(campos[5]);
}

int wsParaCodigo(const char* ws) {
    if (strcmp(ws, "WS1_Pre-expansao")  == 0) return 1;
    if (strcmp(ws, "WS2_Armazenamento") == 0) return 2;
    return 0;   // "nenhuma" ou desconhecido
}

void lerSerial1() {
    while (Serial1.available()) {
        char c = Serial1.read();
        if (c == '<') {
            s1idx = 0;                  // início de trama
        } else if (c == '>') {
            s1buf[s1idx] = '\0';
            processarTramaEv3(s1buf);
            s1idx = 0;
        } else if (s1idx < sizeof(s1buf) - 1) {
            s1buf[s1idx++] = c;
        }
    }
}
// ──────────────────────────────────────────────────────────

void setup() {
    Serial2.begin(115200);   // LiDAR
    Serial1.begin(115200);   // EV3

    lidar.begin(Serial2);
    pinMode(RPLIDAR_MOTOR, OUTPUT);

    // ── Cria o nó Sparkplug ANTES da ligação MQTT ─────────
    spNode = createSparkplugNode("ESTG_Fabrica",
                                 "WS2_Armazenamento",
                                 512,
                                 getTimestampMs);

    createInt32Tag("Robot/Roda1",           &tag_roda1,       1, true, true);
    createInt32Tag("Robot/Roda2",           &tag_roda2,       2, true, true);
    createBoolTag ("Robot/MapRequest",      &tag_map_request, 3, true, false);
    createInt32Tag("Robot/StatusCode",      &tag_status,      4, true, false);
    createBoolTag ("Robot/Obstaculo",       &tag_obstaculo,   5, true, false);
    createInt32Tag("Robot/WorkstationCode", &tag_workstation, 6, true, false);

    makeNDEATHPayload(spNode);

    // ── WiFi ──────────────────────────────────────────────
    while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
        delay(1000);
    }

    // ── LWT — ANTES de connect() ──────────────────────────
    mqttClient.beginWill(spNode->mqtt_message.topic,
                         spNode->mqtt_message.payload->written_length,
                         false, 0);
    mqttClient.write(spNode->mqtt_message.payload->buffer,
                     spNode->mqtt_message.payload->written_length);
    mqttClient.endWill();

    if (!mqttClient.connect(broker, port)) {
        while (1);
    }

    mqttClient.onMessage(onMessage);
    mqttClient.subscribe(spNode->topics.NCMD);
    mqttClient.subscribe(topico_workorder_DCMD);

    spnOnMQTTConnected(spNode);
}

void loop() {
    // Perda de rede → reinicia a placa (recuperação robusta no GIGA)
    if (WiFi.status() != WL_CONNECTED || !mqttClient.connected()) {
        NVIC_SystemReset();
    }

    mqttClient.poll();
    lerSerial1();          // EV3 → tags

    SparkplugNodeState state = tickSparkplugNode(spNode);
    switch (state) {
        case spn_NBIRTH_PL_READY:
            publishMQTT(spNode);
            spnOnPublishNBIRTH(spNode);
            break;
        case spn_NDATA_PL_READY:
            publishMQTT(spNode);
            spnOnPublishNDATA(spNode);
            break;
        default:
            break;
    }

    // ── LiDAR → MQTT (não vai para o Serial1) ─────────────
    if (IS_OK(lidar.waitPoint())) {
        float distance = lidar.getCurrentPoint().distance;
        float angle    = lidar.getCurrentPoint().angle;
        byte  quality  = lidar.getCurrentPoint().quality;
        bool  startBit = lidar.getCurrentPoint().startBit;

        if (distance > 0 && quality >= 15) {
            String payload = "{";
            payload += "\"start\":" + String(startBit) + ",";
            payload += "\"ang\":"   + String(angle, 2) + ",";
            payload += "\"dist\":"  + String(distance, 2) + ",";
            payload += "\"qual\":"  + String(quality);
            payload += "}";

            mqttClient.beginMessage(topico_lidar);
            mqttClient.print(payload);
            mqttClient.endMessage();

            Serial1.print("<");
            Serial1.print((int)angle);
            Serial1.print(",");
            Serial1.print((int)distance);
            Serial1.print(">");
        }
    } else {
        analogWrite(RPLIDAR_MOTOR, 0);
        rplidar_response_device_info_t info;
        if (IS_OK(lidar.getDeviceInfo(info, 255))) {
            lidar.startScan();
            analogWrite(RPLIDAR_MOTOR, 255);
            delay(1000);
        }
    }
}

void onMessage(int messageSize) {
    String topicoRecebido = mqttClient.messageTopic();

    if (topicoRecebido == spNode->topics.NCMD) {
        uint8_t buf[256];
        int i = 0;
        while (mqttClient.available() && i < 256) {
            buf[i++] = mqttClient.read();
        }
        processIncomingNCMDPayload(spNode, buf, i);

    } else if (topicoRecebido == topico_workorder_DCMD) {
        processWorkOrders(messageSize);
    }
}

// ── Comandos do broker → EV3 ──────────────────────────────
void processWorkOrders(int messageSize) {
    String mensagem = "";
    mensagem.reserve(messageSize);
    while (mqttClient.available()) {
        mensagem += (char)mqttClient.read();
    }
    mensagem.trim();

    if (mensagem == "MAP") {
        tag_map_request = true;
        Serial1.print("CMD:MAP\n");
    } else if (mensagem == "START") {
        tag_map_request = false;
        Serial1.print("CMD:START\n");
    }
    // qualquer outra coisa é ignorada — não mexe no robô
}
