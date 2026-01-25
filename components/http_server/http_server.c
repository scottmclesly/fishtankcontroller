/**
 * @file http_server.c
 * @brief HTTP Server with REST API and WebSocket Support
 */

#include "http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "http_server";

// External handler declarations
extern esp_err_t handle_sensors_get(httpd_req_t *req);
extern esp_err_t handle_history_get(httpd_req_t *req);
extern esp_err_t handle_calibration_status_get(httpd_req_t *req);
extern esp_err_t handle_calibration_raw_get(httpd_req_t *req);
extern esp_err_t handle_calibration_ph_1point(httpd_req_t *req);
extern esp_err_t handle_calibration_ph_2point(httpd_req_t *req);
extern esp_err_t handle_calibration_ec(httpd_req_t *req);
extern esp_err_t handle_calibration_clear(httpd_req_t *req);
extern esp_err_t handle_mqtt_config_get(httpd_req_t *req);
extern esp_err_t handle_mqtt_config_post(httpd_req_t *req);
extern esp_err_t handle_mqtt_status_get(httpd_req_t *req);
extern esp_err_t handle_settings_tank_get(httpd_req_t *req);
extern esp_err_t handle_settings_tank_post(httpd_req_t *req);
extern esp_err_t handle_settings_warnings_get(httpd_req_t *req);
extern esp_err_t handle_settings_warnings_post(httpd_req_t *req);
extern esp_err_t handle_wifi_config_get(httpd_req_t *req);
extern esp_err_t handle_wifi_config_post(httpd_req_t *req);
extern esp_err_t handle_wifi_scan_get(httpd_req_t *req);
extern esp_err_t handle_ota_status_get(httpd_req_t *req);
extern esp_err_t handle_ota_update_post(httpd_req_t *req);
extern esp_err_t handle_ota_upload_post(httpd_req_t *req);
extern esp_err_t handle_ota_confirm_post(httpd_req_t *req);
extern esp_err_t handle_ota_rollback_post(httpd_req_t *req);
extern esp_err_t handle_ota_reboot_post(httpd_req_t *req);
extern esp_err_t handle_websocket(httpd_req_t *req);

// WebSocket client tracking
#define WS_MAX_CLIENTS 4
static int s_ws_fds[WS_MAX_CLIENTS] = {-1, -1, -1, -1};
static httpd_handle_t s_server_handle = NULL;

// Shared sensor data
static struct {
    float temp_c;
    float orp_mv;
    float ph;
    float ec_ms_cm;
    float tds_ppm;
    float co2_ppm;
    float nh3_ppm;
    float max_do_mg_l;
    uint8_t temp_warning;
    uint8_t ph_warning;
    uint8_t orp_warning;
    uint8_t ec_warning;
    uint8_t nh3_warning;
    uint8_t do_warning;
    bool valid;
} s_sensor_data = {0};

void http_server_update_sensor_data(float temp_c, float orp_mv, float ph, float ec_ms_cm,
                                     float tds_ppm, float co2_ppm, float nh3_ppm, float max_do_mg_l,
                                     uint8_t temp_w, uint8_t ph_w, uint8_t orp_w, uint8_t ec_w,
                                     uint8_t nh3_w, uint8_t do_w, bool valid)
{
    s_sensor_data.temp_c = temp_c;
    s_sensor_data.orp_mv = orp_mv;
    s_sensor_data.ph = ph;
    s_sensor_data.ec_ms_cm = ec_ms_cm;
    s_sensor_data.tds_ppm = tds_ppm;
    s_sensor_data.co2_ppm = co2_ppm;
    s_sensor_data.nh3_ppm = nh3_ppm;
    s_sensor_data.max_do_mg_l = max_do_mg_l;
    s_sensor_data.temp_warning = temp_w;
    s_sensor_data.ph_warning = ph_w;
    s_sensor_data.orp_warning = orp_w;
    s_sensor_data.ec_warning = ec_w;
    s_sensor_data.nh3_warning = nh3_w;
    s_sensor_data.do_warning = do_w;
    s_sensor_data.valid = valid;
}

void http_server_get_sensor_data(float *temp_c, float *orp_mv, float *ph, float *ec_ms_cm,
                                  float *tds_ppm, float *co2_ppm, float *nh3_ppm, float *max_do_mg_l,
                                  uint8_t *warnings, bool *valid)
{
    if (temp_c) *temp_c = s_sensor_data.temp_c;
    if (orp_mv) *orp_mv = s_sensor_data.orp_mv;
    if (ph) *ph = s_sensor_data.ph;
    if (ec_ms_cm) *ec_ms_cm = s_sensor_data.ec_ms_cm;
    if (tds_ppm) *tds_ppm = s_sensor_data.tds_ppm;
    if (co2_ppm) *co2_ppm = s_sensor_data.co2_ppm;
    if (nh3_ppm) *nh3_ppm = s_sensor_data.nh3_ppm;
    if (max_do_mg_l) *max_do_mg_l = s_sensor_data.max_do_mg_l;
    if (warnings) {
        warnings[0] = s_sensor_data.temp_warning;
        warnings[1] = s_sensor_data.ph_warning;
        warnings[2] = s_sensor_data.orp_warning;
        warnings[3] = s_sensor_data.ec_warning;
        warnings[4] = s_sensor_data.nh3_warning;
        warnings[5] = s_sensor_data.do_warning;
    }
    if (valid) *valid = s_sensor_data.valid;
}

// Embedded Dashboard HTML
static const char *DASHBOARD_HTML =
"<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Aquarium Controller</title><style>"
"*{box-sizing:border-box;margin:0;padding:0}body{font-family:system-ui;background:#1a1a2e;color:#eee;padding:20px}"
".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:15px;max-width:800px;margin:0 auto}"
".card{background:#16213e;border-radius:12px;padding:20px;text-align:center}"
".card h3{font-size:14px;color:#888;margin-bottom:8px}"
".card .value{font-size:32px;font-weight:bold}"
".card .unit{font-size:14px;color:#888}"
".ok{color:#4ade80}.warn{color:#fbbf24}.alert{color:#f87171}"
"h1{text-align:center;margin-bottom:20px;color:#818cf8}"
".status{text-align:center;color:#888;margin-top:20px;font-size:12px}"
"nav{text-align:center;margin-bottom:20px}nav a{color:#818cf8;margin:0 10px}"
"</style></head><body>"
"<h1>Aquarium Controller</h1>"
"<nav><a href='/'>Dashboard</a><a href='/calibration'>Calibration</a></nav>"
"<div class='grid'>"
"<div class='card'><h3>Temperature</h3><div class='value' id='temp'>--</div><div class='unit'>C</div></div>"
"<div class='card'><h3>pH</h3><div class='value' id='ph'>--</div></div>"
"<div class='card'><h3>ORP</h3><div class='value' id='orp'>--</div><div class='unit'>mV</div></div>"
"<div class='card'><h3>EC</h3><div class='value' id='ec'>--</div><div class='unit'>mS/cm</div></div>"
"<div class='card'><h3>TDS</h3><div class='value' id='tds'>--</div><div class='unit'>ppm</div></div>"
"<div class='card'><h3>CO2</h3><div class='value' id='co2'>--</div><div class='unit'>ppm</div></div>"
"</div>"
"<div class='status' id='status'>Connecting...</div>"
"<script>"
"const ws=new WebSocket('ws://'+location.host+'/ws');"
"ws.onmessage=e=>{const d=JSON.parse(e.data);"
"if(d.type==='sensor_data'){"
"document.getElementById('temp').textContent=d.data.temp_c.toFixed(1);"
"document.getElementById('ph').textContent=d.data.ph.toFixed(2);"
"document.getElementById('orp').textContent=d.data.orp_mv.toFixed(0);"
"document.getElementById('ec').textContent=d.data.ec_ms_cm.toFixed(3);"
"document.getElementById('tds').textContent=d.data.tds_ppm.toFixed(0);"
"document.getElementById('co2').textContent=d.data.co2_ppm.toFixed(0);"
"document.getElementById('status').textContent='Live - '+new Date().toLocaleTimeString();}};"
"ws.onclose=()=>document.getElementById('status').textContent='Disconnected';"
"</script></body></html>";

static esp_err_t handle_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, DASHBOARD_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// =============================================================================
// Server Start/Stop
// =============================================================================
esp_err_t http_server_start(http_server_t *server)
{
    if (!server) return ESP_ERR_INVALID_ARG;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = HTTP_SERVER_PORT;
    config.max_open_sockets = HTTP_SERVER_MAX_CLIENTS;
    config.max_uri_handlers = 30;
    config.stack_size = HTTP_SERVER_STACK_SIZE;

    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);

    esp_err_t ret = httpd_start(&server->handle, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start server: %s", esp_err_to_name(ret));
        return ret;
    }

    s_server_handle = server->handle;
    server->running = true;
    server->request_count = 0;

    // Register root
    httpd_uri_t root_uri = { .uri = "/", .method = HTTP_GET, .handler = handle_root };
    httpd_register_uri_handler(server->handle, &root_uri);

    // Register all routes
    http_server_register_sensor_routes(server->handle);
    http_server_register_calibration_routes(server->handle);
    http_server_register_mqtt_routes(server->handle);
    http_server_register_settings_routes(server->handle);
    http_server_register_ota_routes(server->handle);
    http_server_register_websocket(server->handle);

    ESP_LOGI(TAG, "HTTP server started");
    return ESP_OK;
}

esp_err_t http_server_stop(http_server_t *server)
{
    if (server && server->handle) {
        httpd_stop(server->handle);
        server->handle = NULL;
        server->running = false;
        s_server_handle = NULL;
    }
    return ESP_OK;
}

bool http_server_is_running(http_server_t *server)
{
    return server && server->running;
}

// =============================================================================
// Route Registration
// =============================================================================
void http_server_register_sensor_routes(httpd_handle_t server)
{
    httpd_uri_t sensors = { .uri = "/api/sensors", .method = HTTP_GET, .handler = handle_sensors_get };
    httpd_uri_t history = { .uri = "/api/history", .method = HTTP_GET, .handler = handle_history_get };
    httpd_register_uri_handler(server, &sensors);
    httpd_register_uri_handler(server, &history);
    ESP_LOGI(TAG, "Sensor routes registered");
}

void http_server_register_calibration_routes(httpd_handle_t server)
{
    httpd_uri_t status = { .uri = "/api/calibration/status", .method = HTTP_GET, .handler = handle_calibration_status_get };
    httpd_uri_t raw = { .uri = "/api/calibration/raw", .method = HTTP_GET, .handler = handle_calibration_raw_get };
    httpd_uri_t ph1 = { .uri = "/api/calibration/ph/1point", .method = HTTP_POST, .handler = handle_calibration_ph_1point };
    httpd_uri_t ph2 = { .uri = "/api/calibration/ph/2point", .method = HTTP_POST, .handler = handle_calibration_ph_2point };
    httpd_uri_t ec = { .uri = "/api/calibration/ec", .method = HTTP_POST, .handler = handle_calibration_ec };
    httpd_uri_t clear = { .uri = "/api/calibration/clear", .method = HTTP_POST, .handler = handle_calibration_clear };
    httpd_register_uri_handler(server, &status);
    httpd_register_uri_handler(server, &raw);
    httpd_register_uri_handler(server, &ph1);
    httpd_register_uri_handler(server, &ph2);
    httpd_register_uri_handler(server, &ec);
    httpd_register_uri_handler(server, &clear);
    ESP_LOGI(TAG, "Calibration routes registered");
}

void http_server_register_mqtt_routes(httpd_handle_t server)
{
    httpd_uri_t cfg_get = { .uri = "/api/mqtt/config", .method = HTTP_GET, .handler = handle_mqtt_config_get };
    httpd_uri_t cfg_post = { .uri = "/api/mqtt/config", .method = HTTP_POST, .handler = handle_mqtt_config_post };
    httpd_uri_t status = { .uri = "/api/mqtt/status", .method = HTTP_GET, .handler = handle_mqtt_status_get };
    httpd_register_uri_handler(server, &cfg_get);
    httpd_register_uri_handler(server, &cfg_post);
    httpd_register_uri_handler(server, &status);
    ESP_LOGI(TAG, "MQTT routes registered");
}

void http_server_register_settings_routes(httpd_handle_t server)
{
    httpd_uri_t tank_get = { .uri = "/api/settings/tank", .method = HTTP_GET, .handler = handle_settings_tank_get };
    httpd_uri_t tank_post = { .uri = "/api/settings/tank", .method = HTTP_POST, .handler = handle_settings_tank_post };
    httpd_uri_t warn_get = { .uri = "/api/settings/warnings", .method = HTTP_GET, .handler = handle_settings_warnings_get };
    httpd_uri_t warn_post = { .uri = "/api/settings/warnings", .method = HTTP_POST, .handler = handle_settings_warnings_post };
    httpd_uri_t wifi_get = { .uri = "/api/wifi/config", .method = HTTP_GET, .handler = handle_wifi_config_get };
    httpd_uri_t wifi_post = { .uri = "/api/wifi/config", .method = HTTP_POST, .handler = handle_wifi_config_post };
    httpd_uri_t wifi_scan = { .uri = "/api/wifi/scan", .method = HTTP_GET, .handler = handle_wifi_scan_get };
    httpd_register_uri_handler(server, &tank_get);
    httpd_register_uri_handler(server, &tank_post);
    httpd_register_uri_handler(server, &warn_get);
    httpd_register_uri_handler(server, &warn_post);
    httpd_register_uri_handler(server, &wifi_get);
    httpd_register_uri_handler(server, &wifi_post);
    httpd_register_uri_handler(server, &wifi_scan);
    ESP_LOGI(TAG, "Settings routes registered");
}

void http_server_register_ota_routes(httpd_handle_t server)
{
    httpd_uri_t status = { .uri = "/api/ota/status", .method = HTTP_GET, .handler = handle_ota_status_get };
    httpd_uri_t update = { .uri = "/api/ota/update", .method = HTTP_POST, .handler = handle_ota_update_post };
    httpd_uri_t upload = { .uri = "/api/ota/upload", .method = HTTP_POST, .handler = handle_ota_upload_post };
    httpd_uri_t confirm = { .uri = "/api/ota/confirm", .method = HTTP_POST, .handler = handle_ota_confirm_post };
    httpd_uri_t rollback = { .uri = "/api/ota/rollback", .method = HTTP_POST, .handler = handle_ota_rollback_post };
    httpd_uri_t reboot = { .uri = "/api/ota/reboot", .method = HTTP_POST, .handler = handle_ota_reboot_post };
    httpd_register_uri_handler(server, &status);
    httpd_register_uri_handler(server, &update);
    httpd_register_uri_handler(server, &upload);
    httpd_register_uri_handler(server, &confirm);
    httpd_register_uri_handler(server, &rollback);
    httpd_register_uri_handler(server, &reboot);
    ESP_LOGI(TAG, "OTA routes registered");
}

void http_server_register_static_routes(httpd_handle_t server)
{
    ESP_LOGI(TAG, "Static routes: using embedded HTML");
}

// =============================================================================
// WebSocket
// =============================================================================
void http_server_register_websocket(httpd_handle_t server)
{
    httpd_uri_t ws = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = handle_websocket,
        .is_websocket = true,
        .handle_ws_control_frames = true
    };
    httpd_register_uri_handler(server, &ws);
    ESP_LOGI(TAG, "WebSocket registered");
}

void http_server_ws_add_client(int fd)
{
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        if (s_ws_fds[i] == -1) {
            s_ws_fds[i] = fd;
            ESP_LOGI(TAG, "WS client added: fd=%d", fd);
            return;
        }
    }
}

void http_server_ws_remove_client(int fd)
{
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        if (s_ws_fds[i] == fd) {
            s_ws_fds[i] = -1;
            ESP_LOGI(TAG, "WS client removed: fd=%d", fd);
            return;
        }
    }
}

int http_server_ws_broadcast(const char *json_data)
{
    if (!s_server_handle || !json_data) return 0;

    httpd_ws_frame_t pkt = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)json_data,
        .len = strlen(json_data)
    };

    int sent = 0;
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        if (s_ws_fds[i] != -1) {
            if (httpd_ws_send_frame_async(s_server_handle, s_ws_fds[i], &pkt) == ESP_OK) {
                sent++;
            } else {
                s_ws_fds[i] = -1;
            }
        }
    }
    return sent;
}

int http_server_ws_get_client_count(void)
{
    int count = 0;
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        if (s_ws_fds[i] != -1) count++;
    }
    return count;
}

void http_server_broadcast_sensor_data(void)
{
    if (!s_sensor_data.valid || http_server_ws_get_client_count() == 0) return;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "sensor_data");

    cJSON *data = cJSON_AddObjectToObject(root, "data");
    cJSON_AddNumberToObject(data, "temp_c", s_sensor_data.temp_c);
    cJSON_AddNumberToObject(data, "orp_mv", s_sensor_data.orp_mv);
    cJSON_AddNumberToObject(data, "ph", s_sensor_data.ph);
    cJSON_AddNumberToObject(data, "ec_ms_cm", s_sensor_data.ec_ms_cm);
    cJSON_AddNumberToObject(data, "tds_ppm", s_sensor_data.tds_ppm);
    cJSON_AddNumberToObject(data, "co2_ppm", s_sensor_data.co2_ppm);
    cJSON_AddNumberToObject(data, "nh3_ppm", s_sensor_data.nh3_ppm);
    cJSON_AddNumberToObject(data, "max_do_mg_l", s_sensor_data.max_do_mg_l);
    cJSON_AddBoolToObject(data, "valid", true);

    cJSON *warnings = cJSON_AddObjectToObject(data, "warnings");
    cJSON_AddNumberToObject(warnings, "temp", s_sensor_data.temp_warning);
    cJSON_AddNumberToObject(warnings, "ph", s_sensor_data.ph_warning);
    cJSON_AddNumberToObject(warnings, "orp", s_sensor_data.orp_warning);
    cJSON_AddNumberToObject(warnings, "ec", s_sensor_data.ec_warning);
    cJSON_AddNumberToObject(warnings, "nh3", s_sensor_data.nh3_warning);
    cJSON_AddNumberToObject(warnings, "do", s_sensor_data.do_warning);

    char *json = cJSON_PrintUnformatted(root);
    if (json) {
        http_server_ws_broadcast(json);
        free(json);
    }
    cJSON_Delete(root);
}
