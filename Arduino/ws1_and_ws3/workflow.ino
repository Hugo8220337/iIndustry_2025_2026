#include <Arduino_GigaDisplay_GFX.h>
#include <Arduino_GigaDisplayTouch.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "sparkplug_b.pb.h"
#include "config.h"
#include "ui.h"

const char *dcmd_topic = "spBv1.0/" GROUP_ID "/DCMD/" NODE_ID "/" DEVICE_ID;
const char *ddata_topic = "spBv1.0/" GROUP_ID "/DDATA/" NODE_ID "/" DEVICE_ID;

WiFiClient gigaClient;
PubSubClient client(gigaClient);
GigaDisplay_GFX display;
Arduino_GigaDisplayTouch touch;

String currentState    = "Initializing";
String pendingState    = "";
String workOrder       = "";   // WorkOrder/Id
String currentMoId     = "";   // ManufacturingOrder/Id
String currentMoName   = "";   // ManufacturingOrder/Name
String currentOpName   = "";   // Operation/Name
String currentWcName   = "";   // WorkCenter/Name
String currentProdName = "";   // Product/Name
float  currentKpiAvail = 0.0f;
float  currentKpiPerf  = 0.0f;
float  currentKpiQual  = 0.0f;
float  currentKpiOee   = 0.0f;
String pendingPauseReason = "";
uint64_t bdSeq            = 0;
bool choosingPauseReason  = false;
bool choosingPauseType    = false;

// --- Touch debounce ---
unsigned long lastTouchTime = 0;
#define TOUCH_DEBOUNCE_MS 400

// =========================================================
// NANOPB ENCODING (DDATA → MES)
// =========================================================

// Generic string encoder — stores a const char* in .arg
bool encode_str_field(pb_ostream_t *stream, const pb_field_t *field, void *const *arg)
{
    const char *s = (const char *)(*arg);
    if (!pb_encode_tag_for_field(stream, field))
        return false;
    return pb_encode_string(stream, (const pb_byte_t *)s, strlen(s));
}

static bool encode_one_float_metric(pb_ostream_t *stream, const pb_field_t *field,
                                    const char *name, float value)
{
    org_eclipse_tahu_protobuf_Payload_Metric m = org_eclipse_tahu_protobuf_Payload_Metric_init_zero;
    m.name.funcs.encode = &encode_str_field;
    m.name.arg = (void *)name;
    m.datatype = 9;
    m.has_datatype = true;
    m.which_value = org_eclipse_tahu_protobuf_Payload_Metric_float_value_tag;
    m.value.float_value = value;
    if (!pb_encode_tag_for_field(stream, field))
        return false;
    return pb_encode_submessage(stream, org_eclipse_tahu_protobuf_Payload_Metric_fields, &m);
}

static bool encode_one_metric(pb_ostream_t *stream, const pb_field_t *field,
                              const char *name, const char *value)
{
    org_eclipse_tahu_protobuf_Payload_Metric m = org_eclipse_tahu_protobuf_Payload_Metric_init_zero;
    m.name.funcs.encode = &encode_str_field;
    m.name.arg = (void *)name;
    m.datatype = 12;
    m.has_datatype = true;
    m.which_value = org_eclipse_tahu_protobuf_Payload_Metric_string_value_tag;
    m.value.string_value.funcs.encode = &encode_str_field;
    m.value.string_value.arg = (void *)value;
    if (!pb_encode_tag_for_field(stream, field))
        return false;
    return pb_encode_submessage(stream, org_eclipse_tahu_protobuf_Payload_Metric_fields, &m);
}

struct DDataArg
{
    const char *eventType;
    const char *woId;
    const char *moId;
    const char *pauseReason;
    const char *pauseReasonType;
    const char *moName;
    const char *opName;
    const char *wcName;
    const char *prodName;
    float kpiAvail;
    float kpiPerf;
    float kpiQual;
    float kpiOee;
};

bool encode_metrics_callback(pb_ostream_t *stream, const pb_field_t *field, void *const *arg)
{
    DDataArg *d = (DDataArg *)(*arg);
    if (!encode_one_metric(stream, field, "Event/Type", d->eventType))
        return false;
    if (d->moId && strlen(d->moId) > 0)
        if (!encode_one_metric(stream, field, "ManufacturingOrder/Id", d->moId))
            return false;
    if (d->woId && strlen(d->woId) > 0)
        if (!encode_one_metric(stream, field, "WorkOrder/Id", d->woId))
            return false;
    if (d->pauseReason && strlen(d->pauseReason) > 0) {
        if (!encode_one_metric(stream, field, "WorkOrder/PauseReason", d->pauseReason))
            return false;
        if (d->pauseReasonType && strlen(d->pauseReasonType) > 0)
            if (!encode_one_metric(stream, field, "WorkOrder/PauseReasonType", d->pauseReasonType))
                return false;
    }
    if (d->moName && strlen(d->moName) > 0)
        if (!encode_one_metric(stream, field, "ManufacturingOrder/Name", d->moName))
            return false;
    if (d->opName && strlen(d->opName) > 0)
        if (!encode_one_metric(stream, field, "Operation/Name", d->opName))
            return false;
    if (d->wcName && strlen(d->wcName) > 0)
        if (!encode_one_metric(stream, field, "WorkCenter/Name", d->wcName))
            return false;
    if (d->prodName && strlen(d->prodName) > 0)
        if (!encode_one_metric(stream, field, "Product/Name", d->prodName))
            return false;
    if (!encode_one_float_metric(stream, field, "KPI/Availability", d->kpiAvail))
        return false;
    if (!encode_one_float_metric(stream, field, "KPI/Performance", d->kpiPerf))
        return false;
    if (!encode_one_float_metric(stream, field, "KPI/Quality", d->kpiQual))
        return false;
    if (!encode_one_float_metric(stream, field, "KPI/OEE", d->kpiOee))
        return false;
    return true;
}

void sendStateUpdate(const String &newState, const String &reason = "", const String &reasonType = "")
{
    pendingState = newState;
    redrawStateLabel();

    const char *eventType;
    if (newState == "Start")      eventType = "Start_WorkOrder";
    else if (newState == "Pause") eventType = "Pause_WorkOrder";
    else                          eventType = "Stop_WorkOrder";

    const char *pauseReasonType = reasonType.c_str();

    org_eclipse_tahu_protobuf_Payload payload = org_eclipse_tahu_protobuf_Payload_init_zero;
    payload.timestamp = millis();
    payload.has_timestamp = true;
    payload.seq = bdSeq++;
    payload.has_seq = true;

    DDataArg ddataArg = {
        eventType,
        workOrder.c_str(),
        currentMoId.c_str(),
        reason.c_str(),
        pauseReasonType,
        currentMoName.c_str(),
        currentOpName.c_str(),
        currentWcName.c_str(),
        currentProdName.c_str(),
        currentKpiAvail,
        currentKpiPerf,
        currentKpiQual,
        currentKpiOee
    };
    payload.metrics.funcs.encode = &encode_metrics_callback;
    payload.metrics.arg = &ddataArg;

    byte buffer[1024];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    if (pb_encode(&stream, org_eclipse_tahu_protobuf_Payload_fields, &payload))
    {
        client.publish(ddata_topic, buffer, stream.bytes_written);
        Serial.print("Event → MES: ");
        Serial.print(eventType);
        if (workOrder.length() > 0) {
            Serial.print("  WO: ");
            Serial.print(workOrder);
        }
        Serial.println();
    }
    else
    {
        Serial.println("Failed to encode DDATA payload.");
    }
}

// =========================================================
// NANOPB DECODING (DCMD ← MES)
// =========================================================
enum MetricType
{
    METRIC_NONE,
    METRIC_COMMAND_TYPE,
    METRIC_MO_ID,
    METRIC_WO_ID,
    METRIC_ACK,
    METRIC_MO_NAME,
    METRIC_OP_NAME,
    METRIC_WC_NAME,
    METRIC_PROD_NAME,
    METRIC_KPI_AVAIL,
    METRIC_KPI_PERF,
    METRIC_KPI_QUAL,
    METRIC_KPI_OEE
};

String receivedCommandType = "";
String receivedMoId        = "";
String receivedWoId        = "";
String receivedAck         = "";
String receivedMoName      = "";
String receivedOpName      = "";
String receivedWcName      = "";
String receivedProdName    = "";
float  receivedKpiAvail    = 0.0f;
float  receivedKpiPerf     = 0.0f;
float  receivedKpiQual     = 0.0f;
float  receivedKpiOee      = 0.0f;

bool decode_str_field(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    char buf[128] = {0};
    if (stream->bytes_left >= sizeof(buf))
        return false;
    if (!pb_read(stream, (pb_byte_t *)buf, stream->bytes_left))
        return false;
    MetricType *type = (MetricType *)(*arg);
    if (*type == METRIC_COMMAND_TYPE) receivedCommandType = String(buf);
    if (*type == METRIC_MO_ID)        receivedMoId        = String(buf);
    if (*type == METRIC_WO_ID)        receivedWoId        = String(buf);
    if (*type == METRIC_ACK)          receivedAck         = String(buf);
    if (*type == METRIC_MO_NAME)      receivedMoName      = String(buf);
    if (*type == METRIC_OP_NAME)      receivedOpName      = String(buf);
    if (*type == METRIC_WC_NAME)      receivedWcName      = String(buf);
    if (*type == METRIC_PROD_NAME)    receivedProdName    = String(buf);
    return true;
}

bool decode_name_field(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    char buf[64] = {0};
    if (stream->bytes_left >= sizeof(buf))
        return false;
    if (!pb_read(stream, (pb_byte_t *)buf, stream->bytes_left))
        return false;
    MetricType *type = (MetricType *)(*arg);
    if      (strcmp(buf, "Command/Type")           == 0) *type = METRIC_COMMAND_TYPE;
    else if (strcmp(buf, "ManufacturingOrder/Id")  == 0) *type = METRIC_MO_ID;
    else if (strcmp(buf, "WorkOrder/Id")           == 0) *type = METRIC_WO_ID;
    else if (strcmp(buf, "Machine/Ack")            == 0) *type = METRIC_ACK;
    else if (strcmp(buf, "ManufacturingOrder/Name")== 0) *type = METRIC_MO_NAME;
    else if (strcmp(buf, "Operation/Name")         == 0) *type = METRIC_OP_NAME;
    else if (strcmp(buf, "WorkCenter/Name")        == 0) *type = METRIC_WC_NAME;
    else if (strcmp(buf, "Product/Name")           == 0) *type = METRIC_PROD_NAME;
    else if (strcmp(buf, "KPI/Availability")       == 0) *type = METRIC_KPI_AVAIL;
    else if (strcmp(buf, "KPI/Performance")        == 0) *type = METRIC_KPI_PERF;
    else if (strcmp(buf, "KPI/Quality")            == 0) *type = METRIC_KPI_QUAL;
    else if (strcmp(buf, "KPI/OEE")                == 0) *type = METRIC_KPI_OEE;
    return true;
}

bool metrics_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    org_eclipse_tahu_protobuf_Payload_Metric metric = org_eclipse_tahu_protobuf_Payload_Metric_init_zero;
    MetricType metricType = METRIC_NONE;

    metric.name.funcs.decode = &decode_name_field;
    metric.name.arg = &metricType;
    metric.value.string_value.funcs.decode = &decode_str_field;
    metric.value.string_value.arg = &metricType;

    if (!pb_decode(stream, org_eclipse_tahu_protobuf_Payload_Metric_fields, &metric))
        return false;

    if (metric.which_value == org_eclipse_tahu_protobuf_Payload_Metric_float_value_tag) {
        float v = metric.value.float_value;
        if (metricType == METRIC_KPI_AVAIL) receivedKpiAvail = v;
        if (metricType == METRIC_KPI_PERF)  receivedKpiPerf  = v;
        if (metricType == METRIC_KPI_QUAL)  receivedKpiQual  = v;
        if (metricType == METRIC_KPI_OEE)   receivedKpiOee   = v;
    }
    return true;
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    if (!strstr(topic, "/DCMD/"))
        return;

    org_eclipse_tahu_protobuf_Payload spb_payload = org_eclipse_tahu_protobuf_Payload_init_zero;
    spb_payload.metrics.funcs.decode = &metrics_callback;

    receivedCommandType = "";
    receivedMoId        = "";
    receivedWoId        = "";
    receivedAck         = "";
    receivedMoName      = "";
    receivedOpName      = "";
    receivedWcName      = "";
    receivedProdName    = "";
    receivedKpiAvail    = 0.0f;
    receivedKpiPerf     = 0.0f;
    receivedKpiQual     = 0.0f;
    receivedKpiOee      = 0.0f;
    pb_istream_t stream = pb_istream_from_buffer(payload, length);

    if (!pb_decode(&stream, org_eclipse_tahu_protobuf_Payload_fields, &spb_payload))
    {
        Serial.print("Decode error: ");
        Serial.println(PB_GET_ERROR(&stream));
        return;
    }

    if (receivedAck.length() > 0 && pendingState.length() > 0)
    {
        Serial.print("Ack received: ");
        Serial.println(receivedAck);
        bool success = (receivedAck.charAt(0) == '2');
        if (success) {
            currentState = pendingState;
            Serial.print("State confirmed: ");
            Serial.println(currentState);
            if (currentState == "Stop") {
                workOrder       = "";
                currentMoId     = "";
                currentMoName   = "";
                currentOpName   = "";
                currentWcName   = "";
                currentProdName = "";
                currentKpiAvail = 0.0f;
                currentKpiPerf  = 0.0f;
                currentKpiQual  = 0.0f;
                currentKpiOee   = 0.0f;
                currentState    = "Waiting";
                Serial.println("Work order cleared, waiting for next assignment.");
                redrawTitle();
            }
        } else {
            Serial.print("State rejected, reverting. Code: ");
            Serial.println(receivedAck);
        }
        pendingState = "";
        redrawStateLabel();
        return;
    }

    if (receivedCommandType == "ASSIGN_WORKORDER")
    {
        if (receivedWoId.length() > 0) {
            workOrder = receivedWoId;
            Serial.print("WorkOrder assigned: ");
            Serial.println(workOrder);
        }
        if (receivedMoId.length() > 0)     currentMoId     = receivedMoId;
        if (receivedMoName.length() > 0)   currentMoName   = receivedMoName;
        if (receivedOpName.length() > 0)   currentOpName   = receivedOpName;
        if (receivedWcName.length() > 0)   currentWcName   = receivedWcName;
        if (receivedProdName.length() > 0) currentProdName = receivedProdName;
        currentKpiAvail = receivedKpiAvail;
        currentKpiPerf  = receivedKpiPerf;
        currentKpiQual  = receivedKpiQual;
        currentKpiOee   = receivedKpiOee;
        if (currentState == "Initializing" || currentState == "Waiting")
            currentState = "Ready";
        redrawTitle();
        redrawStateLabel();
    }
}

// =========================================================
// WIFI AND MQTT
// =========================================================
void setup_wifi()
{
    int status = WL_IDLE_STATUS;
    while (status != WL_CONNECTED)
    {
        Serial.print("Connecting to ");
        Serial.println(WIFI_SSID);
        status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        delay(1000);
    }
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
}

void reconnect()
{
    while (!client.connected())
    {
        String deathTopic = String("spBv1.0/") + GROUP_ID + "/NDEATH/" + NODE_ID;
        if (client.connect(NODE_ID, deathTopic.c_str(), 0, true, "offline"))
        {
            Serial.println("MQTT connected");
            client.subscribe(dcmd_topic);
            if (currentState == "Initializing") {
                currentState = "Waiting";
                redrawStateLabel();
            }
        }
        else
        {
            Serial.print("MQTT failed, rc=");
            Serial.print(client.state());
            Serial.println(" — retrying in 5s");
            delay(5000);
        }
    }
}

// =========================================================
// TOUCH HANDLING
// =========================================================
void handleTouch()
{
    GDTpoint_t points[GT911_MAX_CONTACTS];
    uint8_t contacts = touch.getTouchPoints(points);
    if (contacts == 0)
        return;

    unsigned long now = millis();
    if (now - lastTouchTime < TOUCH_DEBOUNCE_MS)
        return;
    lastTouchTime = now;

    // GT911 native portrait coords (x:0-479, y:0-799) → rotation-1 landscape
    int tx = points[0].y;
    int ty = 479 - points[0].x;

    if (choosingPauseReason) {
        for (int i = 0; i < NUM_PAUSE_REASONS; i++) {
            if (hitTest(BTN_REASONS[i], tx, ty)) {
                choosingPauseReason = false;
                pendingPauseReason  = String(PAUSE_REASON_VALUES[i]);
                choosingPauseType   = true;
                drawPauseTypeMenu();
                return;
            }
        }
        if (hitTest(BTN_CANCEL, tx, ty)) {
            choosingPauseReason = false;
            drawUI();
        }
        return;
    }

    if (choosingPauseType) {
        if (hitTest(BTN_PLANNED, tx, ty)) {
            choosingPauseType = false;
            sendStateUpdate("Pause", pendingPauseReason, "PLANNED");
            pendingPauseReason = "";
            drawUI();
            return;
        }
        if (hitTest(BTN_UNPLANNED, tx, ty)) {
            choosingPauseType = false;
            sendStateUpdate("Pause", pendingPauseReason, "UNPLANNED");
            pendingPauseReason = "";
            drawUI();
            return;
        }
        if (hitTest(BTN_CANCEL, tx, ty)) {
            choosingPauseType  = false;
            pendingPauseReason = "";
            drawUI();
        }
        return;
    }

    if (currentState == "Initializing" || currentState == "Waiting") return;
    if (pendingState.length() > 0) return;

    if (hitTest(BTN_START, tx, ty) && currentState != "Start")
        sendStateUpdate("Start");
    else if (hitTest(BTN_PAUSE, tx, ty) && currentState != "Pause") {
        choosingPauseReason = true;
        drawPauseReasonMenu();
    }
    else if (hitTest(BTN_STOP, tx, ty) && currentState != "Stop")
        sendStateUpdate("Stop");
}

// =========================================================
// SETUP / LOOP
// =========================================================
void setup()
{
    Serial.begin(115200);
    while (!Serial && millis() < 3000)
        ;

    display.begin();
    touch.begin();
    drawUI();

    setup_wifi();
    client.setServer(MQTT_SERVER, MQTT_PORT);
    client.setCallback(mqttCallback);
    client.setBufferSize(1024);
}

void loop()
{
    if (!client.connected())
        reconnect();
    client.loop();
    handleTouch();
}
