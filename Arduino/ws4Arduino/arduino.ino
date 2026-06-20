#include <Arduino.h>
#include <Arduino_GigaDisplayTouch.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <time.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "sparkplug_b.pb.h"
#include "config.h"
#include "ui.h"

#define TOUCH_MODE 1

#define NICLA_BAUD 9600
#define SERIAL_BAUD 115200

Arduino_GigaDisplayTouch touchDetector;
WiFiClient gigaClient;
WiFiUDP ntpUDP;
PubSubClient client(gigaClient);

const char *NTP_SERVER = "pool.ntp.org";
const unsigned int NTP_LOCAL_PORT = 2390;
const int NTP_PACKET_SIZE = 48;
byte ntpPacketBuffer[NTP_PACKET_SIZE];

bool ntpTimeValid = false;
uint64_t ntpEpochAtSync = 0;
unsigned long ntpMillisAtSync = 0;
const unsigned long NTP_RESYNC_INTERVAL = 6UL * 60UL * 60UL * 1000UL;

const char *dcmd_topic = "spBv1.0/" GROUP_ID "/DCMD/" NODE_ID "/" DEVICE_ID;
const char *ddata_topic = "spBv1.0/" GROUP_ID "/DDATA/" NODE_ID "/" DEVICE_ID;

void updateQualityState();
void confirmPiece();
void rejectDetectedClass();
void finishRejectedCorrection(String correctedClass, bool isRejectedByClass, String defectCode = "");
void finishDecision(bool isRejectedByClass, String statusText, uint16_t statusColor, String defectCode = "");

String inferArticleColorFromProduct(String productName);
void clearReceivedWorkData();
void clearCurrentWorkOrderData();

void readNiclaUART();
void checkNiclaTimeout();
void processNiclaLine(String line);
void startEvaluationWindow();
void updateEvaluationWindow();
void resetClassStats();
void addClassSample(String label, float score, int x, int y);
int getWinningClassIndex();

void handleTouch();
void waitTouchRelease();
void mapTouch(int rawX, int rawY, int &x, int &y);
void handleHomeTouch(int x, int y);
void handleEvaluateTouch(int x, int y);
void handleRejectReasonTouch(int x, int y);
void handleColorSelectTouch(int x, int y);

void setup_wifi();
void setup_mqtt();
void reconnect_mqtt();
void mqttCallback(char *topic, byte *payload, unsigned int length);
void sendStateUpdate(const String &newState);
void checkPendingAckTimeout();
void requestStart();
void requestStop();
void sendDefectDetected(const String &defectCode);
String defectCodeFromClass(const String &label);
bool isColorMismatch(const String &selectedColor);
String normalizeText(String value);

int currentPage = PAGE_HOME;

bool started = false;
String currentState = "Initializing";
String pendingState = "";
unsigned long pendingSince = 0;
const unsigned long ACK_TIMEOUT = 8000;

String manufacturingOrder = "";
String workOrder = "";
String productCode = "";
int productId = -1;
String articleColor = "";
String operationName = "";
String workCenterName = "";

float availability = 0.0;
float performance = 0.0;
float quality = 0.0;
float oee = 0.0;

int qualityState = 3;

String currentLabel = "Sem detecao";
float currentScore = 0.0;
int detectedX = 0;
int detectedY = 0;

String serialBuffer = "";
bool niclaConnected = false;
unsigned long lastNiclaMessage = 0;

unsigned long lastTouchTime = 0;
const unsigned long TOUCH_COOLDOWN = 350;

bool evaluating = false;
bool evaluationFinished = false;
unsigned long evaluationStartTime = 0;
const unsigned long EVALUATION_DURATION = 5000;

struct ClassStats {
  String label;
  int count;
  float scoreSum;
  float bestScore;
  int bestX;
  int bestY;
};

const int MAX_EVALUATION_CLASSES = 10;

const int MIN_WINNING_CLASS_SAMPLES = 2;

ClassStats classStats[MAX_EVALUATION_CLASSES];
int classStatsCount = 0;
int samplesReceived = 0;

bool finalResultLocked = false;

String inferArticleColorFromProduct(String productName) {
  String p = productName;
  p.toLowerCase();

  if (p.indexOf("azul") >= 0) return "Azul";
  if (p.indexOf("verde") >= 0) return "Verde";
  if (p.indexOf("vermelh") >= 0) return "Vermelha";
  if (p.indexOf("amarel") >= 0) return "Amarela";

  return "";
}

void updateQualityState() {
  String label = currentLabel;
  label.toLowerCase();

  bool isDefect = false;
  if (label.indexOf("defeito") >= 0) isDefect = true;
  if (label.indexOf("defect") >= 0) isDefect = true;
  if (label.indexOf("scrap") >= 0) isDefect = true;
  if (label.indexOf("rejeitado") >= 0) isDefect = true;

  if (isDefect) {
    if (currentScore >= 0.75) qualityState = 2;
    else qualityState = 1;
  } else {
    qualityState = 0;
  }
}

void resetClassStats() {
  classStatsCount = 0;

  for (int i = 0; i < MAX_EVALUATION_CLASSES; i++) {
    classStats[i].label = "";
    classStats[i].count = 0;
    classStats[i].scoreSum = 0.0;
    classStats[i].bestScore = 0.0;
    classStats[i].bestX = 0;
    classStats[i].bestY = 0;
  }
}

void addClassSample(String label, float score, int x, int y) {
  String normalizedLabel = normalizeText(label);

  for (int i = 0; i < classStatsCount; i++) {
    if (normalizeText(classStats[i].label) == normalizedLabel) {
      classStats[i].count++;
      classStats[i].scoreSum += score;

      if (score > classStats[i].bestScore) {
        classStats[i].bestScore = score;
        classStats[i].bestX = x;
        classStats[i].bestY = y;
      }

      return;
    }
  }

  if (classStatsCount < MAX_EVALUATION_CLASSES) {
    classStats[classStatsCount].label = label;
    classStats[classStatsCount].count = 1;
    classStats[classStatsCount].scoreSum = score;
    classStats[classStatsCount].bestScore = score;
    classStats[classStatsCount].bestX = x;
    classStats[classStatsCount].bestY = y;
    classStatsCount++;
  } else {
    Serial.println("Aviso: limite de classes na avaliacao atingido. Amostra ignorada.");
  }
}

int getWinningClassIndex() {
  if (classStatsCount == 0) return -1;

  int winner = 0;

  for (int i = 1; i < classStatsCount; i++) {
    float avgScore = classStats[i].scoreSum / (float)classStats[i].count;
    float winnerAvgScore = classStats[winner].scoreSum / (float)classStats[winner].count;

    if (classStats[i].count > classStats[winner].count) {
      winner = i;
    } else if (classStats[i].count == classStats[winner].count && avgScore > winnerAvgScore) {
      winner = i;
    } else if (classStats[i].count == classStats[winner].count && avgScore == winnerAvgScore && classStats[i].bestScore > classStats[winner].bestScore) {
      winner = i;
    }
  }

  return winner;
}

void confirmPiece() {
  bool isRejectedByClass = (qualityState == 1 || qualityState == 2);
  String defectCode = isRejectedByClass ? defectCodeFromClass(currentLabel) : "";

  finishDecision(
    isRejectedByClass,
    isRejectedByClass ? "STATUS: Classe confirmada como defeito." : "STATUS: Classe confirmada como OK.",
    isRejectedByClass ? RED : GREEN,
    defectCode
  );

  Serial.print("CONFIRMAR pressionado. Classe aceite: ");
  Serial.println(currentLabel);

  if (isRejectedByClass) {
    Serial.print("Defeito aceite enviado como: ");
    Serial.println(defectCode);
  }
}

void rejectDetectedClass() {
  drawRejectReason();
  drawStatus("STATUS: Indique a classe verdadeira.", YELLOW);

  Serial.print("REJEITAR pressionado. Classe detetada rejeitada: ");
  Serial.println(currentLabel);
}

void finishRejectedCorrection(String correctedClass, bool isRejectedByClass, String defectCode) {
  Serial.print("Classe corrigida pelo operador: ");
  Serial.println(correctedClass);

  if (isRejectedByClass && defectCode.length() == 0) {
    defectCode = defectCodeFromClass(correctedClass);
  }

  finishDecision(
    isRejectedByClass,
    isRejectedByClass ? "STATUS: Correcao registada como defeito." : "STATUS: Correcao registada como cor verdadeira.",
    isRejectedByClass ? RED : GREEN,
    defectCode
  );
}

void finishDecision(bool isRejectedByClass, String statusText, uint16_t statusColor, String defectCode) {

  evaluationFinished = false;
  finalResultLocked = false;
  evaluating = false;
  currentPage = PAGE_HOME;

  if (started) {
    if (isRejectedByClass) {
      if (defectCode.length() == 0) {
        defectCode = defectCodeFromClass(currentLabel);
      }
      sendDefectDetected(defectCode);
    }

    requestStop();

    if (pendingState == "Stop") {
      drawStatus(
        isRejectedByClass ? "STATUS: Defeito enviado. STOP enviado ao MES." : "STATUS: Decisao OK. STOP enviado ao MES.",
        YELLOW
      );
    }
  } else {
    drawHome();
    drawStatus(statusText, statusColor);
  }
}

static uint64_t bdSeq = 0;

bool encode_str_field(pb_ostream_t *stream, const pb_field_t *field, void *const *arg) {
  const char *s = (const char *)(*arg);
  if (!pb_encode_tag_for_field(stream, field)) return false;
  return pb_encode_string(stream, (const pb_byte_t *)s, strlen(s));
}

static bool encode_one_metric(pb_ostream_t *stream, const pb_field_t *field, const char *name, const char *value) {
  org_eclipse_tahu_protobuf_Payload_Metric m = org_eclipse_tahu_protobuf_Payload_Metric_init_zero;
  m.name.funcs.encode = &encode_str_field;
  m.name.arg = (void *)name;
  m.datatype = 12;
  m.has_datatype = true;
  m.which_value = org_eclipse_tahu_protobuf_Payload_Metric_string_value_tag;
  m.value.string_value.funcs.encode = &encode_str_field;
  m.value.string_value.arg = (void *)value;

  if (!pb_encode_tag_for_field(stream, field)) return false;
  return pb_encode_submessage(stream, org_eclipse_tahu_protobuf_Payload_Metric_fields, &m);
}

static bool encode_one_metric_int(pb_ostream_t *stream, const pb_field_t *field, const char *name, int value) {
  org_eclipse_tahu_protobuf_Payload_Metric m = org_eclipse_tahu_protobuf_Payload_Metric_init_zero;
  m.name.funcs.encode = &encode_str_field;
  m.name.arg = (void *)name;
  m.datatype = 3;
  m.has_datatype = true;
  m.which_value = org_eclipse_tahu_protobuf_Payload_Metric_int_value_tag;
  m.value.int_value = value;

  if (!pb_encode_tag_for_field(stream, field)) return false;
  return pb_encode_submessage(stream, org_eclipse_tahu_protobuf_Payload_Metric_fields, &m);
}

struct DDataArg {
  const char *eventType;
  const char *workOrderId;
  const char *manufacturingOrderId;
  const char *timestamp;
  const char *pauseReason;
  const char *pauseReasonType;
  const char *productName;
  int productId;
  const char *defectCode;
  int defectQuantity;
  bool hasProductId;
  bool hasDefectQuantity;
};

bool encode_metrics_callback(pb_ostream_t *stream, const pb_field_t *field, void *const *arg) {
  DDataArg *d = (DDataArg *)(*arg);

  if (!encode_one_metric(stream, field, "Event/Type", d->eventType)) return false;
  if (!encode_one_metric(stream, field, "WorkOrder/Id", d->workOrderId)) return false;
  if (!encode_one_metric(stream, field, "ManufacturingOrder/Id", d->manufacturingOrderId)) return false;
  if (!encode_one_metric(stream, field, "Event/Timestamp", d->timestamp)) return false;

  if (strlen(d->pauseReason) > 0) {
    if (!encode_one_metric(stream, field, "WorkOrder/PauseReason", d->pauseReason)) return false;
  }

  if (strlen(d->pauseReasonType) > 0) {
    if (!encode_one_metric(stream, field, "WorkOrder/PauseReasonType", d->pauseReasonType)) return false;
  }

  if (strlen(d->productName) > 0) {
    if (!encode_one_metric(stream, field, "Product/Name", d->productName)) return false;
  }

  if (d->hasProductId) {
    if (!encode_one_metric_int(stream, field, "Product/Id", d->productId)) return false;
  }

  if (strlen(d->defectCode) > 0) {
    if (!encode_one_metric(stream, field, "Production/DefectCode", d->defectCode)) return false;
  }

  if (d->hasDefectQuantity) {
    if (!encode_one_metric_int(stream, field, "Production/DefectQuantity", d->defectQuantity)) return false;
  }

  return true;
}

void sendNtpPacket(const char *address) {
  memset(ntpPacketBuffer, 0, NTP_PACKET_SIZE);

  ntpPacketBuffer[0] = 0b11100011;
  ntpPacketBuffer[1] = 0;
  ntpPacketBuffer[2] = 6;
  ntpPacketBuffer[3] = 0xEC;
  ntpPacketBuffer[12] = 49;
  ntpPacketBuffer[13] = 0x4E;
  ntpPacketBuffer[14] = 49;
  ntpPacketBuffer[15] = 52;

  ntpUDP.beginPacket(address, 123);
  ntpUDP.write(ntpPacketBuffer, NTP_PACKET_SIZE);
  ntpUDP.endPacket();
}

bool syncNtpOnce(unsigned long timeoutMs) {
  sendNtpPacket(NTP_SERVER);

  unsigned long startWait = millis();
  while (millis() - startWait < timeoutMs) {
    int packetSize = ntpUDP.parsePacket();

    if (packetSize >= NTP_PACKET_SIZE) {
      ntpUDP.read(ntpPacketBuffer, NTP_PACKET_SIZE);

      uint32_t secondsSince1900 =
        ((uint32_t)ntpPacketBuffer[40] << 24) |
        ((uint32_t)ntpPacketBuffer[41] << 16) |
        ((uint32_t)ntpPacketBuffer[42] << 8)  |
        ((uint32_t)ntpPacketBuffer[43]);

      const uint32_t seventyYears = 2208988800UL;
      ntpEpochAtSync = (uint64_t)(secondsSince1900 - seventyYears);
      ntpMillisAtSync = millis();
      ntpTimeValid = true;

      return true;
    }

    delay(10);
  }

  return false;
}

void setup_time() {

  ntpUDP.begin(NTP_LOCAL_PORT);

  Serial.print("A sincronizar hora NTP");

  while (!syncNtpOnce(3000)) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nHora NTP sincronizada.");
}

void update_time() {
  if (!ntpTimeValid) {
    setup_time();
    return;
  }

  if (millis() - ntpMillisAtSync >= NTP_RESYNC_INTERVAL) {
    syncNtpOnce(1000);
  }
}

uint64_t epochNow() {
  if (!ntpTimeValid) {
    setup_time();
  }

  return ntpEpochAtSync + ((uint64_t)(millis() - ntpMillisAtSync) / 1000ULL);
}

uint64_t timestampMillisNow() {

  return epochNow() * 1000ULL;
}

String timestampNow() {

  time_t now = (time_t)epochNow();

  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);

  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);

  return String(buffer);
}

void sendStateUpdate(const String &newState) {
  if (pendingState.length() > 0) {
    Serial.print("Pedido MQTT ignorado: ainda existe estado pendente: ");
    Serial.println(pendingState);
    return;
  }

  pendingState = newState;
  pendingSince = millis();
  drawHome();

  String eventType = "";

  if (newState == "Start") {
    eventType = "Start_WorkOrder";
  } else if (newState == "Stop") {
    eventType = "Stop_WorkOrder";
  } else if (newState == "Pause") {
    eventType = "Pause_WorkOrder";
  } else {
    eventType = newState;
  }

  String workOrderId = workOrder;
  String manufacturingOrderId = manufacturingOrder;
  String ts = timestampNow();

  if (workOrderId.length() == 0) {
    Serial.println("Erro: WorkOrder/Id vazio.");
    pendingState = "";
    pendingSince = 0;
    drawHome();
    drawStatus("STATUS: WorkOrder vazia. Aguarde atribuicao do Odoo.", RED);
    return;
  }

  if (manufacturingOrderId.length() == 0) {
    Serial.println("Erro: ManufacturingOrder/Id vazio.");
    pendingState = "";
    pendingSince = 0;
    drawHome();
    drawStatus("STATUS: MO vazia. Aguarde atribuicao do Odoo.", RED);
    return;
  }

  org_eclipse_tahu_protobuf_Payload payload = org_eclipse_tahu_protobuf_Payload_init_zero;
  payload.timestamp = timestampMillisNow();
  payload.has_timestamp = true;
  payload.seq = bdSeq++;
  payload.has_seq = true;

  DDataArg ddataArg = {
    eventType.c_str(),
    workOrderId.c_str(),
    manufacturingOrderId.c_str(),
    ts.c_str(),
    "",
    "",
    "",
    0,
    "",
    0,
    false,
    false
  };

  payload.metrics.funcs.encode = &encode_metrics_callback;
  payload.metrics.arg = &ddataArg;

  byte buffer[512];
  pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

  if (pb_encode(&stream, org_eclipse_tahu_protobuf_Payload_fields, &payload)) {
    Serial.print("A publicar MQTT em: ");
    Serial.print(ddata_topic);
    Serial.print(" | bytes: ");
    Serial.println(stream.bytes_written);

    bool published = client.publish(ddata_topic, buffer, stream.bytes_written);

    if (!published) {
      Serial.print("MQTT publish FALHOU. client.state() = ");
      Serial.println(client.state());

      pendingState = "";
      pendingSince = 0;
      drawHome();
      drawStatus("STATUS: Falha MQTT ao enviar evento.", RED);
      return;
    }

    Serial.print("Evento -> MES: ");
    Serial.print(eventType);
    Serial.print(" | WorkOrder/Id: ");
    Serial.print(workOrderId);
    Serial.print(" | ManufacturingOrder/Id: ");
    Serial.println(manufacturingOrderId);
  } else {
    Serial.print("Failed to encode DDATA payload: ");
    Serial.println(PB_GET_ERROR(&stream));
    pendingState = "";
    pendingSince = 0;
    drawHome();
  }
}

String normalizeText(String value) {
  value.trim();
  value.toLowerCase();

  value.replace("á", "a");
  value.replace("à", "a");
  value.replace("ã", "a");
  value.replace("â", "a");
  value.replace("é", "e");
  value.replace("ê", "e");
  value.replace("í", "i");
  value.replace("ó", "o");
  value.replace("ô", "o");
  value.replace("õ", "o");
  value.replace("ú", "u");
  value.replace("ç", "c");

  return value;
}

String defectCodeFromClass(const String &label) {
  String l = normalizeText(label);

  if (l.indexOf("borda") >= 0 || l.indexOf("edge") >= 0) return "Defect_Edge";
  if (l.indexOf("risco") >= 0 || l.indexOf("scratch") >= 0 || l.indexOf("trace") >= 0) return "Defect_Trace";
  if (l.indexOf("mistura") >= 0 || l.indexOf("mixture") >= 0) return "Defect_Mixture";
  if (l.indexOf("cor") >= 0 || l.indexOf("color") >= 0 || l.indexOf("colour") >= 0) return "Defect_Mixture";

  return "Defect_Unknown";
}

bool isColorMismatch(const String &selectedColor) {
  String selected = normalizeText(selectedColor);
  String expected = normalizeText(articleColor);

  if (expected.length() == 0) return false;

  return selected != expected;
}

void sendDefectDetected(const String &defectCode) {
  String workOrderId = workOrder;
  String manufacturingOrderId = manufacturingOrder;
  String ts = timestampNow();
  String productName = productCode;

  if (workOrderId.length() == 0) {
    Serial.println("Erro: WorkOrder/Id vazia. Defeito nao enviado.");
    drawStatus("STATUS: WorkOrder vazia. Defeito nao enviado.", RED);
    return;
  }

  if (manufacturingOrderId.length() == 0) {
    Serial.println("Erro: ManufacturingOrder/Id vazia. Defeito nao enviado.");
    drawStatus("STATUS: MO vazia. Defeito nao enviado.", RED);
    return;
  }

  org_eclipse_tahu_protobuf_Payload payload = org_eclipse_tahu_protobuf_Payload_init_zero;
  payload.timestamp = timestampMillisNow();
  payload.has_timestamp = true;
  payload.seq = bdSeq++;
  payload.has_seq = true;

  DDataArg ddataArg = {
    "Defect_Detected",
    workOrderId.c_str(),
    manufacturingOrderId.c_str(),
    ts.c_str(),
    "",
    "",
    productName.c_str(),
    productId,
    defectCode.c_str(),
    1,
    productId > 0,
    true
  };

  payload.metrics.funcs.encode = &encode_metrics_callback;
  payload.metrics.arg = &ddataArg;

  byte buffer[512];
  pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

  if (pb_encode(&stream, org_eclipse_tahu_protobuf_Payload_fields, &payload)) {
    Serial.print("A publicar defeito MQTT em: ");
    Serial.print(ddata_topic);
    Serial.print(" | bytes: ");
    Serial.println(stream.bytes_written);

    bool published = client.publish(ddata_topic, buffer, stream.bytes_written);

    if (!published) {
      Serial.print("MQTT publish Defect_Detected FALHOU. client.state() = ");
      Serial.println(client.state());
      drawStatus("STATUS: Falha MQTT ao enviar defeito.", RED);
      return;
    }

    Serial.print("Evento -> MES: Defect_Detected");
    Serial.print(" | WorkOrder/Id: ");
    Serial.print(workOrderId);
    Serial.print(" | ManufacturingOrder/Id: ");
    Serial.print(manufacturingOrderId);
    Serial.print(" | Product/Id: ");
    Serial.print(productId);
    Serial.print(" | Production/DefectCode: ");
    Serial.println(defectCode);
  } else {
    Serial.print("Failed to encode Defect_Detected payload: ");
    Serial.println(PB_GET_ERROR(&stream));
    drawStatus("STATUS: Falha ao codificar defeito.", RED);
  }
}

void checkPendingAckTimeout() {
  if (pendingState.length() == 0) return;
  if (pendingSince == 0) return;
  if (millis() - pendingSince < ACK_TIMEOUT) return;

  Serial.print("Timeout ACK do MES/Node-RED. Estado pendente libertado: ");
  Serial.println(pendingState);

  pendingState = "";
  pendingSince = 0;
  drawHome();
  drawStatus("STATUS: Timeout ACK do MES/Node-RED.", RED);
}

void requestStart() {
  if (started || pendingState.length() > 0) return;

  if (workOrder.length() == 0 || manufacturingOrder.length() == 0) {
    drawStatus("STATUS: Ainda nao foi atribuida uma WorkOrder.", YELLOW);
    Serial.println("START bloqueado: sem WorkOrder/MO atribuida.");
    return;
  }

  sendStateUpdate("Start");
}

void requestStop() {
  if (!started || pendingState.length() > 0) return;
  sendStateUpdate("Stop");
}

enum MetricType {
  METRIC_NONE,
  METRIC_COMMAND,
  METRIC_WORKORDER_ID,
  METRIC_MANUFACTURING_ORDER_ID,
  METRIC_MANUFACTURING_ORDER_NAME,
  METRIC_OPERATION_NAME,
  METRIC_WORKCENTER_NAME,
  METRIC_PRODUCT_NAME,
  METRIC_PRODUCT_CODE,
  METRIC_PRODUCT_ID,
  METRIC_ARTICLE_COLOR,
  METRIC_AVAILABILITY,
  METRIC_PERFORMANCE,
  METRIC_QUALITY,
  METRIC_OEE,
  METRIC_ACK
};

String receivedCommand = "";
String receivedWorkOrder = "";
String receivedManufacturingOrder = "";
String receivedManufacturingOrderName = "";
String receivedOperationName = "";
String receivedWorkCenterName = "";
String receivedProductName = "";
String receivedProductCode = "";
int receivedProductId = -1;
String receivedArticleColor = "";
String receivedAck = "";

float receivedAvailability = -1.0;
float receivedPerformance = -1.0;
float receivedQuality = -1.0;
float receivedOEE = -1.0;

void clearReceivedWorkData() {
  receivedCommand = "";
  receivedWorkOrder = "";
  receivedManufacturingOrder = "";
  receivedManufacturingOrderName = "";
  receivedOperationName = "";
  receivedWorkCenterName = "";
  receivedProductName = "";
  receivedProductCode = "";
  receivedProductId = -1;
  receivedArticleColor = "";
  receivedAck = "";

  receivedAvailability = -1.0;
  receivedPerformance = -1.0;
  receivedQuality = -1.0;
  receivedOEE = -1.0;
}

void clearCurrentWorkOrderData() {
  manufacturingOrder = "";
  workOrder = "";
  productCode = "";
  productId = -1;
  articleColor = "";
  operationName = "";
  workCenterName = "";

  currentLabel = "Sem detecao";
  currentScore = 0.0;
  detectedX = 0;
  detectedY = 0;
  qualityState = 3;

  evaluating = false;
  evaluationFinished = false;
  finalResultLocked = false;
  currentPage = PAGE_HOME;

  currentState = "Waiting";

}

bool decode_str_field(pb_istream_t *stream, const pb_field_t *field, void **arg) {
  char buf[128] = {0};
  if (stream->bytes_left >= sizeof(buf)) return false;
  if (!pb_read(stream, (pb_byte_t *)buf, stream->bytes_left)) return false;

  MetricType *type = (MetricType *)(*arg);
  String value = String(buf);

  if (*type == METRIC_COMMAND) receivedCommand = value;
  else if (*type == METRIC_WORKORDER_ID) receivedWorkOrder = value;
  else if (*type == METRIC_MANUFACTURING_ORDER_ID) receivedManufacturingOrder = value;
  else if (*type == METRIC_MANUFACTURING_ORDER_NAME) receivedManufacturingOrderName = value;
  else if (*type == METRIC_OPERATION_NAME) receivedOperationName = value;
  else if (*type == METRIC_WORKCENTER_NAME) receivedWorkCenterName = value;
  else if (*type == METRIC_PRODUCT_NAME) receivedProductName = value;
  else if (*type == METRIC_PRODUCT_CODE) receivedProductCode = value;
  else if (*type == METRIC_PRODUCT_ID) receivedProductId = value.toInt();
  else if (*type == METRIC_ARTICLE_COLOR) receivedArticleColor = value;
  else if (*type == METRIC_ACK) receivedAck = value;

  return true;
}

bool decode_name_field(pb_istream_t *stream, const pb_field_t *field, void **arg) {
  char buf[128] = {0};
  if (stream->bytes_left >= sizeof(buf)) return false;
  if (!pb_read(stream, (pb_byte_t *)buf, stream->bytes_left)) return false;

  MetricType *type = (MetricType *)(*arg);

  if      (strcmp(buf, "Command/Type") == 0) *type = METRIC_COMMAND;
  else if (strcmp(buf, "WorkOrder/Id") == 0) *type = METRIC_WORKORDER_ID;
  else if (strcmp(buf, "ManufacturingOrder/Id") == 0) *type = METRIC_MANUFACTURING_ORDER_ID;
  else if (strcmp(buf, "ManufacturingOrder/Name") == 0) *type = METRIC_MANUFACTURING_ORDER_NAME;
  else if (strcmp(buf, "Operation/Name") == 0) *type = METRIC_OPERATION_NAME;
  else if (strcmp(buf, "WorkCenter/Name") == 0) *type = METRIC_WORKCENTER_NAME;
  else if (strcmp(buf, "Product/Name") == 0) *type = METRIC_PRODUCT_NAME;
  else if (strcmp(buf, "Product/Code") == 0) *type = METRIC_PRODUCT_CODE;
  else if (strcmp(buf, "Product/Id") == 0) *type = METRIC_PRODUCT_ID;
  else if (strcmp(buf, "Product/id") == 0) *type = METRIC_PRODUCT_ID;
  else if (strcmp(buf, "Product/ID") == 0) *type = METRIC_PRODUCT_ID;
  else if (strcmp(buf, "product/id") == 0) *type = METRIC_PRODUCT_ID;
  else if (strcmp(buf, "Article/Color") == 0) *type = METRIC_ARTICLE_COLOR;
  else if (strcmp(buf, "KPI/Availability") == 0) *type = METRIC_AVAILABILITY;
  else if (strcmp(buf, "KPI/Performance") == 0) *type = METRIC_PERFORMANCE;
  else if (strcmp(buf, "KPI/Quality") == 0) *type = METRIC_QUALITY;
  else if (strcmp(buf, "KPI/OEE") == 0) *type = METRIC_OEE;

  else if (strcmp(buf, "Machine/Command") == 0) *type = METRIC_COMMAND;
  else if (strcmp(buf, "Work/Order") == 0) *type = METRIC_WORKORDER_ID;
  else if (strcmp(buf, "Work/ManufacturingOrder") == 0) *type = METRIC_MANUFACTURING_ORDER_ID;
  else if (strcmp(buf, "Work/ProductCode") == 0) *type = METRIC_PRODUCT_CODE;
  else if (strcmp(buf, "Work/ProductId") == 0) *type = METRIC_PRODUCT_ID;
  else if (strcmp(buf, "Work/ProductID") == 0) *type = METRIC_PRODUCT_ID;
  else if (strcmp(buf, "Work/Product/id") == 0) *type = METRIC_PRODUCT_ID;
  else if (strcmp(buf, "Work/ArticleColor") == 0) *type = METRIC_ARTICLE_COLOR;
  else if (strcmp(buf, "Machine/Ack") == 0) *type = METRIC_ACK;

  else if (strcmp(buf, "Ack") == 0) *type = METRIC_ACK;
  else if (strcmp(buf, "ACK") == 0) *type = METRIC_ACK;
  else if (strcmp(buf, "Command/Ack") == 0) *type = METRIC_ACK;
  else if (strcmp(buf, "Event/Ack") == 0) *type = METRIC_ACK;
  else if (strcmp(buf, "WorkOrder/Ack") == 0) *type = METRIC_ACK;
  else if (strcmp(buf, "Command/ACK") == 0) *type = METRIC_ACK;
  else if (strcmp(buf, "Event/ACK") == 0) *type = METRIC_ACK;
  else if (strcmp(buf, "WorkOrder/ACK") == 0) *type = METRIC_ACK;
  else *type = METRIC_NONE;

  return true;
}

bool metrics_callback(pb_istream_t *stream, const pb_field_t *field, void **arg) {
  org_eclipse_tahu_protobuf_Payload_Metric metric = org_eclipse_tahu_protobuf_Payload_Metric_init_zero;
  MetricType metricType = METRIC_NONE;

  metric.name.funcs.decode = &decode_name_field;
  metric.name.arg = &metricType;
  metric.value.string_value.funcs.decode = &decode_str_field;
  metric.value.string_value.arg = &metricType;

  if (!pb_decode(stream, org_eclipse_tahu_protobuf_Payload_Metric_fields, &metric)) {
    return false;
  }

  if (metric.which_value == org_eclipse_tahu_protobuf_Payload_Metric_float_value_tag) {
    if (metricType == METRIC_AVAILABILITY) receivedAvailability = metric.value.float_value;
    else if (metricType == METRIC_PERFORMANCE) receivedPerformance = metric.value.float_value;
    else if (metricType == METRIC_QUALITY) receivedQuality = metric.value.float_value;
    else if (metricType == METRIC_OEE) receivedOEE = metric.value.float_value;
  }

  if (metric.which_value == org_eclipse_tahu_protobuf_Payload_Metric_int_value_tag) {
    if (metricType == METRIC_PRODUCT_ID) receivedProductId = metric.value.int_value;
  }

  return true;
}

static bool ackSuccess(const String &ack) {
  return (ack == "OK") || (ack.charAt(0) == '2');
}

static void applyAck() {
  if (receivedAck.length() == 0 || pendingState.length() == 0) return;

  Serial.print("Ack received: ");
  Serial.println(receivedAck);

  Serial.print("PendingState no momento do ACK: ");
  Serial.println(pendingState);

  if (ackSuccess(receivedAck)) {
    currentState = pendingState;

    if (pendingState == "Start") {
      started = true;
      Serial.println("START confirmado pelo MES.");

    } else if (pendingState == "Stop") {
      started = false;
      clearCurrentWorkOrderData();
      Serial.println("STOP confirmado pelo MES. WorkOrder limpa.");
    }

  } else {
    Serial.print("Estado rejeitado, a reverter. Codigo: ");
    Serial.println(receivedAck);
  }

  pendingState = "";
  pendingSince = 0;
  drawHome();
}

static void applyWorkData() {
  bool changed = false;

  if (receivedWorkOrder.length() > 0) {
    workOrder = receivedWorkOrder;
    changed = true;
    Serial.print("WorkOrder/Id recebida: ");
    Serial.println(workOrder);
  }

  if (receivedManufacturingOrder.length() > 0) {
    manufacturingOrder = receivedManufacturingOrder;
    changed = true;
    Serial.print("ManufacturingOrder/Id recebida: ");
    Serial.println(manufacturingOrder);
  }

  if (receivedManufacturingOrder.length() == 0 && receivedManufacturingOrderName.length() > 0) {
    manufacturingOrder = receivedManufacturingOrderName;
    changed = true;
    Serial.print("ManufacturingOrder/Name recebida como fallback: ");
    Serial.println(manufacturingOrder);
  }

  if (receivedProductName.length() > 0) {
    productCode = receivedProductName;
    changed = true;

    String inferredColor = inferArticleColorFromProduct(receivedProductName);
    if (inferredColor.length() > 0) {
      articleColor = inferredColor;
    }

    Serial.print("Product/Name recebido: ");
    Serial.println(productCode);
    Serial.print("ArticleColor inferida: ");
    Serial.println(articleColor);
  } else if (receivedProductCode.length() > 0) {
    productCode = receivedProductCode;
    changed = true;
    Serial.print("ProductCode recebido: ");
    Serial.println(productCode);
  }

  if (receivedProductId > 0) {
    productId = receivedProductId;
    changed = true;
    Serial.print("Product/Id recebido: ");
    Serial.println(productId);
  }

  if (receivedArticleColor.length() > 0) {
    articleColor = receivedArticleColor;
    changed = true;
    Serial.print("ArticleColor recebida diretamente: ");
    Serial.println(articleColor);
  }

  if (receivedOperationName.length() > 0) {
    operationName = receivedOperationName;
    changed = true;
    Serial.print("Operation/Name recebida: ");
    Serial.println(operationName);
  }

  if (receivedWorkCenterName.length() > 0) {
    workCenterName = receivedWorkCenterName;
    changed = true;
    Serial.print("WorkCenter/Name recebido: ");
    Serial.println(workCenterName);
  }

  if (receivedAvailability >= 0.0) {
    availability = receivedAvailability;
    changed = true;
  }

  if (receivedPerformance >= 0.0) {
    performance = receivedPerformance;
    changed = true;
  }

  if (receivedQuality >= 0.0) {
    quality = receivedQuality;
    changed = true;
  }

  if (receivedOEE >= 0.0) {
    oee = receivedOEE;
    changed = true;
  }

  if (changed) {
    Serial.println("Dados da ordem aplicados no Arduino.");
    drawHome();
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  if (!strstr(topic, "/DCMD/")) return;

  org_eclipse_tahu_protobuf_Payload spb_payload = org_eclipse_tahu_protobuf_Payload_init_zero;
  spb_payload.metrics.funcs.decode = &metrics_callback;

  clearReceivedWorkData();

  pb_istream_t stream = pb_istream_from_buffer(payload, length);

  if (!pb_decode(&stream, org_eclipse_tahu_protobuf_Payload_fields, &spb_payload)) {
    Serial.print("Decode error: ");
    Serial.println(PB_GET_ERROR(&stream));
    return;
  }

  if (receivedAck.length() > 0 && pendingState.length() > 0) {
    applyAck();
    return;
  }

  applyWorkData();

  if (receivedCommand == "Start" || receivedCommand == "START_WORKORDER") {
    requestStart();
  } else if (receivedCommand == "Stop" || receivedCommand == "STOP_WORKORDER") {
    requestStop();
  } else if (receivedCommand == "ASSIGN_WORKORDER") {
    Serial.println("Comando ASSIGN_WORKORDER recebido.");
    Serial.print("MO atual: ");
    Serial.println(manufacturingOrder);
    Serial.print("WO atual: ");
    Serial.println(workOrder);
    Serial.print("Produto atual: ");
    Serial.println(productCode);
    Serial.print("Product/Id atual: ");
    Serial.println(productId);
    Serial.print("Cor atual: ");
    Serial.println(articleColor);
    Serial.print("Operacao atual: ");
    Serial.println(operationName);
    Serial.print("WorkCenter atual: ");
    Serial.println(workCenterName);
  }
}

void setup_wifi() {
  int status = WL_IDLE_STATUS;

  while (status != WL_CONNECTED) {
    Serial.print("Connecting to ");
    Serial.println(WIFI_SSID);
    status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    delay(1000);
  }

  Serial.print("WiFi connected, IP: ");
  Serial.println(WiFi.localIP());
}

void setup_mqtt() {
  client.setBufferSize(2048);
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(mqttCallback);
}

void reconnect_mqtt() {
  while (!client.connected()) {
    String deathTopic = String("spBv1.0/") + GROUP_ID + "/NDEATH/" + NODE_ID;
    String mqttClientId = String("arduino_giga_") + DEVICE_ID;

    Serial.print("A ligar ao MQTT com Client ID: ");
    Serial.println(mqttClientId);

    if (client.connect(mqttClientId.c_str(), deathTopic.c_str(), 0, true, "offline")) {
      client.subscribe(dcmd_topic);

      if (currentState == "Initializing") {
        currentState = "Waiting";
        drawHome();
      }
    } else {
      Serial.print("MQTT failed, rc=");
      Serial.print(client.state());
      Serial.println(" — retrying in 5s");
      delay(5000);
    }
  }
}

void readNiclaUART() {
  while (Serial1.available()) {
    char c = Serial1.read();

    if (c == '\n') {
      serialBuffer.trim();

      if (serialBuffer.length() > 0) {
        niclaConnected = true;
        lastNiclaMessage = millis();

        processNiclaLine(serialBuffer);
      }

      serialBuffer = "";
    } else if (c != '\r') {
      serialBuffer += c;

      if (serialBuffer.length() > 150) {
        serialBuffer = "";
      }
    }
  }
}

void checkNiclaTimeout() {
  if (!niclaConnected) return;

  if (millis() - lastNiclaMessage > 3000) {
    niclaConnected = false;
    currentLabel = "Sem sinal";
    currentScore = 0.0;
    detectedX = 0;
    detectedY = 0;
    qualityState = 3;

    if (currentPage == PAGE_EVALUATE && !evaluating && !finalResultLocked) {
      drawEvaluate();
      drawStatus("STATUS: Sem sinal da Nicla", RED);
    }

    Serial.println("Sem sinal da Nicla por mais de 3 segundos.");
  }
}

void processNiclaLine(String line) {
  line.trim();

  String lowerLine = line;
  lowerLine.toLowerCase();

  if (lowerLine == "none") {
    if (currentPage == PAGE_EVALUATE && finalResultLocked) return;
    if (currentPage == PAGE_EVALUATE && evaluating) return;

    currentLabel = "Sem detecao";
    currentScore = 0.0;
    detectedX = 0;
    detectedY = 0;
    qualityState = 3;

    if (currentPage == PAGE_EVALUATE) {
      drawEvaluate();
      drawStatus("STATUS: Camara ligada. Sem detecao.", GREY);
    }

    return;
  }

  int p1 = line.indexOf(',');
  int p2 = line.indexOf(',', p1 + 1);
  int p3 = line.indexOf(',', p2 + 1);

  if (p1 < 0 || p2 < 0 || p3 < 0) {
    Serial.print("Linha mal formatada: ");
    Serial.println(line);
    return;
  }

  if (currentPage == PAGE_EVALUATE && finalResultLocked) return;

  currentLabel = line.substring(0, p1);
  detectedX = line.substring(p1 + 1, p2).toInt();
  detectedY = line.substring(p2 + 1, p3).toInt();
  currentScore = line.substring(p3 + 1).toFloat();

  updateQualityState();

  if (currentPage == PAGE_EVALUATE && evaluating) {
    samplesReceived++;
    addClassSample(currentLabel, currentScore, detectedX, detectedY);

    Serial.print("Leitura guardada durante avaliacao: ");
    Serial.print(currentLabel);
    Serial.print(" | Score: ");
    Serial.print(currentScore);
    Serial.print(" | Amostras da janela: ");
    Serial.println(samplesReceived);
    return;
  }

  if (currentPage == PAGE_EVALUATE) {
    drawEvaluate();

    if (qualityState == 0) drawStatus("STATUS: Resultado OK. Confirmar ou rejeitar.", GREEN);
    else if (qualityState == 1) drawStatus("STATUS: Peca duvidosa. Confirmar ou rejeitar.", YELLOW);
    else if (qualityState == 2) drawStatus("STATUS: Defeito detetado. Confirmar ou rejeitar.", RED);
    else drawStatus("STATUS: Sem detecao.", GREY);
  }
}

void startEvaluationWindow() {
  evaluating = true;
  evaluationFinished = false;
  finalResultLocked = false;
  evaluationStartTime = millis();

  resetClassStats();
  samplesReceived = 0;

  currentLabel = "A avaliar...";
  currentScore = 0.0;
  detectedX = 0;
  detectedY = 0;
  qualityState = 3;

  drawEvaluateEvaluating();
  drawStatus("STATUS: A avaliar durante 5 segundos...", YELLOW);
  Serial.println("Inicio da janela de avaliacao de 5 segundos.");
}

void updateEvaluationWindow() {
  if (!evaluating) return;
  if (millis() - evaluationStartTime < EVALUATION_DURATION) return;

  evaluating = false;
  evaluationFinished = true;
  finalResultLocked = true;

  int winner = getWinningClassIndex();

  if (samplesReceived == 0 || winner < 0 || classStats[winner].count < MIN_WINNING_CLASS_SAMPLES) {
    currentLabel = "Sem detecao";
    currentScore = 0.0;
    detectedX = 0;
    detectedY = 0;
    qualityState = 3;

    drawEvaluate();
    drawStatus("STATUS: Avaliacao terminada. Sem detecao suficiente.", GREY);
  } else {
    currentLabel = classStats[winner].label;

    currentScore = classStats[winner].scoreSum / (float)classStats[winner].count;

    detectedX = classStats[winner].bestX;
    detectedY = classStats[winner].bestY;

    updateQualityState();

    drawEvaluate();

    if (qualityState == 0) drawStatus("STATUS: Classe mais frequente: OK. Confirmar ou rejeitar.", GREEN);
    else if (qualityState == 1) drawStatus("STATUS: Classe mais frequente: CHECK. Confirmar ou rejeitar.", YELLOW);
    else if (qualityState == 2) drawStatus("STATUS: Classe mais frequente: DEFEITO. Confirmar ou rejeitar.", RED);
    else drawStatus("STATUS: Avaliacao terminada. Sem sinal/detecao.", GREY);
  }

  Serial.println("Fim da janela de avaliacao.");
  Serial.print("Amostras recebidas: ");
  Serial.println(samplesReceived);

  Serial.println("Resumo por classe:");
  for (int i = 0; i < classStatsCount; i++) {
    float avgScore = classStats[i].scoreSum / (float)classStats[i].count;

    Serial.print(" - ");
    Serial.print(classStats[i].label);
    Serial.print(" | count: ");
    Serial.print(classStats[i].count);
    Serial.print(" | score medio: ");
    Serial.print(avgScore);
    Serial.print(" | melhor score: ");
    Serial.println(classStats[i].bestScore);
  }

  Serial.print("Classe vencedora: ");
  Serial.print(currentLabel);
  Serial.print(" | Score medio: ");
  Serial.println(currentScore);
}

void handleTouch() {
  if (millis() - lastTouchTime < TOUCH_COOLDOWN) return;

  uint8_t contacts;
  GDTpoint_t points[5];
  contacts = touchDetector.getTouchPoints(points);

  if (contacts > 0) {
    int rawX = points[0].x;
    int rawY = points[0].y;
    int x = rawX;
    int y = rawY;

    mapTouch(rawX, rawY, x, y);

    Serial.print("Touch X:");
    Serial.print(x);
    Serial.print(" Y:");
    Serial.println(y);

    lastTouchTime = millis();

    if (currentPage == PAGE_HOME) handleHomeTouch(x, y);
    else if (currentPage == PAGE_EVALUATE) handleEvaluateTouch(x, y);
    else if (currentPage == PAGE_REJECT_REASON) handleRejectReasonTouch(x, y);
    else if (currentPage == PAGE_COLOR_SELECT) handleColorSelectTouch(x, y);

    waitTouchRelease();
  }
}

void waitTouchRelease() {
  GDTpoint_t points[5];
  while (touchDetector.getTouchPoints(points) > 0) {
    delay(20);
  }
  delay(180);
}

void mapTouch(int rawX, int rawY, int &x, int &y) {
  if (TOUCH_MODE == 0) {
    x = rawX;
    y = rawY;
  } else if (TOUCH_MODE == 1) {
    x = rawY;
    y = SCREEN_H - rawX;
  } else if (TOUCH_MODE == 2) {
    x = SCREEN_W - rawY;
    y = rawX;
  } else if (TOUCH_MODE == 3) {
    x = SCREEN_W - rawX;
    y = SCREEN_H - rawY;
  }
}

void handleHomeTouch(int x, int y) {
  if (pendingState.length() > 0) {
    drawStatus("STATUS: Aguarde ACK do MES.", YELLOW);
    return;
  }

  if (isInside(x, y, btnStart)) {
    if (!started) {
      requestStart();
      Serial.println("START pressionado -> pedido MQTT enviado");
    }
    return;
  }

  if (!started) {
    if (isInside(x, y, btnEvaluate)) {
      drawStatus("STATUS: Carregue primeiro em START e aguarde ACK.", YELLOW);
      Serial.println("AVALIAR bloqueado. START ainda nao foi confirmado.");
    }
    return;
  }

  if (isInside(x, y, btnEvaluate)) {
    currentPage = PAGE_EVALUATE;
    startEvaluationWindow();
    Serial.println("AVALIAR pressionado");
  }
}

void handleEvaluateTouch(int x, int y) {
  if (evaluating) {
    drawStatus("STATUS: Aguarde pelo fim da avaliacao.", YELLOW);
    return;
  }

  if (!evaluationFinished) {
    drawStatus("STATUS: Ainda nao existe resultado final.", YELLOW);
    return;
  }

  if (isInside(x, y, btnConfirm)) confirmPiece();
  else if (isInside(x, y, btnReject)) rejectDetectedClass();
}

void handleRejectReasonTouch(int x, int y) {
  if (isInside(x, y, btnBack)) {
    currentPage = PAGE_EVALUATE;
    drawEvaluate();
    drawStatus("STATUS: Resultado mantido. Pode confirmar ou rejeitar.", CYAN);
    return;
  }

  if (isInside(x, y, btnRejectColor)) {
    drawColorSelect();
    return;
  }

  if (isInside(x, y, btnRejectDefectColor)) {

    finishRejectedCorrection("Defeito_Cor", true, "Defect_Mixture");
    return;
  }

  if (isInside(x, y, btnRejectDefectEdge)) {
    finishRejectedCorrection("Defeito_Borda", true, "Defect_Edge");
    return;
  }

  if (isInside(x, y, btnRejectDefectScratch)) {
    finishRejectedCorrection("Defeito_Risco", true, "Defect_Trace");
    return;
  }

}

void handleColorSelectTouch(int x, int y) {
  if (isInside(x, y, btnBack)) {
    drawRejectReason();
    return;
  }

  for (int i = 0; i < COLOR_BUTTON_COUNT; i++) {
    if (isInside(x, y, colorButtons[i])) {
      String selectedColor = String(colorButtons[i].label);

      if (isColorMismatch(selectedColor)) {
        finishRejectedCorrection("Cor: " + selectedColor, true, "Defect_Color");
      } else {
        finishRejectedCorrection("Cor: " + selectedColor, false);
      }

      return;
    }
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  while (!Serial && millis() < 3000) {
    ;
  }

  Serial1.begin(NICLA_BAUD);
  delay(1500);

  display.begin();
  touchDetector.begin();
  display.setRotation(1);

  drawHome();

  Serial.println("=================================");
  Serial.println("Sistema iniciado.");
  Serial.println("A aguardar ASSIGN_WORKORDER do Odoo via SparkplugB...");
  Serial.println("A aguardar dados da Nicla por UART Serial1...");
  Serial.println("Formato esperado: label,x,y,score ou none");
  Serial.println("=================================");

  setup_wifi();
  setup_time();
  setup_mqtt();
}

void loop() {
  if (!client.connected()) {
    reconnect_mqtt();
  }

  client.loop();
  update_time();
  checkPendingAckTimeout();

  readNiclaUART();
  checkNiclaTimeout();

  updateEvaluationWindow();
  handleTouch();
}