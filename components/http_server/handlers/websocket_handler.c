/**
 * @file websocket_handler.c
 * @brief WebSocket handler for live sensor updates
 */

#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "websocket_handler";

// External functions from http_server.c
extern void http_server_ws_add_client(int fd);
extern void http_server_ws_remove_client(int fd);

// WebSocket handler
esp_err_t handle_websocket(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        // This is the handshake
        ESP_LOGI(TAG, "WebSocket handshake");
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    // Get frame length first
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get frame length: %s", esp_err_to_name(ret));
        return ret;
    }

    if (ws_pkt.len) {
        buf = calloc(1, ws_pkt.len + 1);
        if (!buf) {
            ESP_LOGE(TAG, "Failed to allocate memory");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;

        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to receive frame: %s", esp_err_to_name(ret));
            free(buf);
            return ret;
        }
    }

    // Handle different frame types
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
        ESP_LOGD(TAG, "Received text: %s", ws_pkt.payload);
        // Could parse JSON commands here
    } else if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        ESP_LOGI(TAG, "WebSocket close requested");
        int fd = httpd_req_to_sockfd(req);
        http_server_ws_remove_client(fd);
    } else if (ws_pkt.type == HTTPD_WS_TYPE_PING) {
        // Respond with PONG
        ws_pkt.type = HTTPD_WS_TYPE_PONG;
        httpd_ws_send_frame(req, &ws_pkt);
    }

    // Track this client for broadcasts
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT || ws_pkt.len == 0) {
        int fd = httpd_req_to_sockfd(req);
        http_server_ws_add_client(fd);
    }

    free(buf);
    return ESP_OK;
}
