// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Implementação e Modificação Hênio Reis
// Canal Maker IoT 15/03/2020

#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "Arduino.h"

#include "webcam.h"

typedef struct
{
	httpd_req_t *req;
	size_t len;
} jpg_chunking_t;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

static size_t jpg_encode_stream(void *arg, size_t index, const void *data, size_t len)
{
	jpg_chunking_t *j = (jpg_chunking_t *)arg;
	if (!index)
	{
		j->len = 0;
	}
	if (httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK)
	{
		return 0;
	}
	j->len += len;
	return len;
}

static esp_err_t Esp32cam_Capture(httpd_req_t *req)
{
	camera_fb_t *fb = NULL;
	esp_err_t res = ESP_OK;

	fb = esp_camera_fb_get();
	if (!fb)
	{
		Serial.println("Camera capture failed");
		httpd_resp_send_500(req);
		return ESP_FAIL;
	}

	httpd_resp_set_type(req, "image/jpeg");
	httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
	httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

	size_t out_len, out_width, out_height;
	uint8_t *out_buf;
	bool s;
	bool detected = false;
	int face_id = 0;
	size_t fb_len = 0;
	if (fb->format == PIXFORMAT_JPEG)
	{
		fb_len = fb->len;
		res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
	}
	else
	{
		jpg_chunking_t jchunk = {req, 0};
		res = frame2jpg_cb(fb, 80, jpg_encode_stream, &jchunk) ? ESP_OK : ESP_FAIL;
		httpd_resp_send_chunk(req, NULL, 0);
		fb_len = jchunk.len;
	}
	esp_camera_fb_return(fb);
	return res;
}

static esp_err_t Esp32cam_Stream(httpd_req_t *req)
{
	camera_fb_t *fb = NULL;
	esp_err_t res = ESP_OK;
	char *part_buf[64];

	res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
	if (res != ESP_OK)
		return res;
	httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
	do
	{
		fb = esp_camera_fb_get();
		Serial.print("Frame size ");
		Serial.println(fb->len);
		if (!fb)
		{
			Serial.println("Camera capture failed");
			continue;
		}
		size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, fb->len);
		res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
		if (res == ESP_OK)
			res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
		if (res == ESP_OK)
			res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
		esp_camera_fb_return(fb);
	} while(res == ESP_OK);
	return res;
}

static esp_err_t Esp32cam_Comando(httpd_req_t *req)
{
	char *buf;
	size_t buf_len;
	char parametro[32] = {0,};
	char valor[32] = {0,};

	buf_len = httpd_req_get_url_query_len(req) + 1;
	if (buf_len > 1)
	{
		buf = (char *)malloc(buf_len);
		if (!buf)
		{
			httpd_resp_send_500(req);
			return ESP_FAIL;
		}
		if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
		{
			if (httpd_query_key_value(buf, "parametro", parametro, sizeof(parametro)) == ESP_OK &&
				httpd_query_key_value(buf, "valor", valor, sizeof(valor)) == ESP_OK)
			{
			}
			else
			{
				free(buf);
				httpd_resp_send_404(req);
				return ESP_FAIL;
			}
		}
		else
		{
			free(buf);
			httpd_resp_send_404(req);
			return ESP_FAIL;
		}
		free(buf);
	}
	else
	{
		httpd_resp_send_404(req);
		return ESP_FAIL;
	}

	int val = atoi(valor);
	sensor_t *s = esp_camera_sensor_get();
	int res = 0;

	if (!strcmp(parametro, "framesize"))
	{
		if (s->pixformat == PIXFORMAT_JPEG)
			res = s->set_framesize(s, (framesize_t)val);
	}
	else if (!strcmp(parametro, "quality"))
		res = s->set_quality(s, val);
	else if (!strcmp(parametro, "contrast"))
		res = s->set_contrast(s, val);
	else if (!strcmp(parametro, "brightness"))
		res = s->set_brightness(s, val);
	else if (!strcmp(parametro, "saturation"))
		res = s->set_saturation(s, val);
	else if (!strcmp(parametro, "special_effect"))
		res = s->set_special_effect(s, val);
	else if (!strcmp(parametro, "wb_mode"))
	{
		if (val == -1)
		{
			res = s->set_awb_gain(s, 0);
		}
		else
		{
			res = s->set_awb_gain(s, 1);
			res = s->set_wb_mode(s, val);
		}
	}
 
	if (res)
	{
		return httpd_resp_send_500(req);
	}

	httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
	return httpd_resp_send(req, NULL, 0);
}

static esp_err_t Esp32cam_Status(httpd_req_t *req)
{
	static char responde_json[1024];

	sensor_t *s = esp_camera_sensor_get();
	char *p = responde_json;
	*p++ = '{';

	p += sprintf(p, "\"framesize\":%u,", s->status.framesize);
	p += sprintf(p, "\"quality\":%u,", s->status.quality);
	p += sprintf(p, "\"brightness\":%d,", s->status.brightness);
	p += sprintf(p, "\"contrast\":%d,", s->status.contrast);
	p += sprintf(p, "\"saturation\":%d,", s->status.saturation);
	p += sprintf(p, "\"sharpness\":%d,", s->status.sharpness);
	p += sprintf(p, "\"special_effect\":%u,", s->status.special_effect);
	p += sprintf(p, "\"wb_mode\":%u,", s->status.wb_mode);
	
	*p++ = '}';
	*p++ = 0;
	httpd_resp_set_type(req, "application/json");
	httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
	return httpd_resp_send(req, responde_json, strlen(responde_json));
}

static esp_err_t Esp32cam_Index(httpd_req_t *req)
{
	httpd_resp_set_type(req, "text/html");
	//httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
	return httpd_resp_send(req, (const char *)WebCam, strlen(WebCam));
}

void START_ESP32CAM_SERVER()
{
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	httpd_uri_t index_uri = {
		.uri = "/",
		.method = HTTP_GET,
		.handler = Esp32cam_Index,
		.user_ctx = NULL};

	httpd_uri_t status_uri = {
		.uri = "/status",
		.method = HTTP_GET,
		.handler = Esp32cam_Status,
		.user_ctx = NULL};

	httpd_uri_t cmd_uri = {
		.uri = "/control",
		.method = HTTP_GET,
		.handler = Esp32cam_Comando,
		.user_ctx = NULL};

	httpd_uri_t capture_uri = {
		.uri = "/capture",
		.method = HTTP_GET,
		.handler = Esp32cam_Capture,
		.user_ctx = NULL};

	httpd_uri_t stream_uri = {
		.uri = "/stream",
		.method = HTTP_GET,
		.handler = Esp32cam_Stream,
		.user_ctx = NULL};
	

	Serial.printf("Starting web server on port: '%d'\n", config.server_port);
	if (httpd_start(&camera_httpd, &config) == ESP_OK)
	{
		httpd_register_uri_handler(camera_httpd, &index_uri);
		httpd_register_uri_handler(camera_httpd, &cmd_uri);
		httpd_register_uri_handler(camera_httpd, &status_uri);
		httpd_register_uri_handler(camera_httpd, &capture_uri);
	}

	config.server_port += 1;
	config.ctrl_port += 1;
	Serial.printf("Starting stream server on port: '%d'\n", config.server_port);
	if (httpd_start(&stream_httpd, &config) == ESP_OK)
	{
		httpd_register_uri_handler(stream_httpd, &stream_uri);
	}
}
