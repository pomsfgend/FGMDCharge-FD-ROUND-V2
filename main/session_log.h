#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_http_server.h"

#define SESSION_LOG_UPDATE 0
#define SESSION_LOG_PROTO  1
#define SESSION_LOG_STOP   2

void session_log_init(void);
void session_log_request(int seal, int reason);
void session_log_service(void);
void session_log_on_second(void);
int session_log_count(void);
int session_log_info(int newest_index, int *seq, int *nbytes, int *protocol, int *reason);
esp_err_t session_log_send_last(httpd_req_t *req);
esp_err_t session_log_send_one(httpd_req_t *req, int newest_index);
esp_err_t session_log_send_zip(httpd_req_t *req);
