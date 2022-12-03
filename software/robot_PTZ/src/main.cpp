#include <Arduino.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include <WiFi.h>
#include "D:\test_f\robot_PTZ\.pio\build\esp32cam\ESP32Servo\src\ESP32Servo.h"

// wifi STA
#define ssid "xxxxx"
#define password "xxxxxx"

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";\


// servo
#define level_servo_pin GPIO_NUM_12
#define vertical_servo_pin GPIO_NUM_13
Servo level_servo, vertical_servo;
int16_t level_val, vertical_val;
uint8_t servo_move_val = 30;


// camera
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

camera_config_t config_cam() {
    camera_config_t config_camera;

    config_camera.ledc_channel = LEDC_CHANNEL_0;
    config_camera.ledc_timer = LEDC_TIMER_0;
    config_camera.pin_d0 = Y2_GPIO_NUM;
    config_camera.pin_d1 = Y3_GPIO_NUM;
    config_camera.pin_d2 = Y4_GPIO_NUM;
    config_camera.pin_d3 = Y5_GPIO_NUM;
    config_camera.pin_d4 = Y6_GPIO_NUM;
    config_camera.pin_d5 = Y7_GPIO_NUM;
    config_camera.pin_d6 = Y8_GPIO_NUM;
    config_camera.pin_d7 = Y9_GPIO_NUM;
    config_camera.pin_xclk = XCLK_GPIO_NUM;
    config_camera.pin_pclk = PCLK_GPIO_NUM;
    config_camera.pin_vsync = VSYNC_GPIO_NUM;
    config_camera.pin_href = HREF_GPIO_NUM;
    config_camera.pin_sscb_sda = SIOD_GPIO_NUM;
    config_camera.pin_sscb_scl = SIOC_GPIO_NUM;
    config_camera.pin_pwdn = PWDN_GPIO_NUM;
    config_camera.pin_reset = RESET_GPIO_NUM;
    config_camera.xclk_freq_hz = 20000000;
    config_camera.pixel_format = PIXFORMAT_JPEG;

    if(psramFound()){
        config_camera.frame_size = FRAMESIZE_VGA;
        config_camera.jpeg_quality = 10;
        config_camera.fb_count = 2;
    } else {
        config_camera.frame_size = FRAMESIZE_SVGA;
        config_camera.jpeg_quality = 12;
        config_camera.fb_count = 1;
    }
    return config_camera;
}

httpd_handle_t stream_httpd = NULL;

// http
static  esp_err_t camera_handler(httpd_req_t *req) {
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char * part_buf[64];

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        return res;
    }

    while(true){
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed");
            res = ESP_FAIL;
        }
        else {
            if(fb->width > 400){
                if(fb->format != PIXFORMAT_JPEG){
                    bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                    esp_camera_fb_return(fb);
                    fb = NULL;
                    if(!jpeg_converted){
                        Serial.println("JPEG compression failed");
                        res = ESP_FAIL;
                    }
                }
                else {
                    _jpg_buf_len = fb->len;
                    _jpg_buf = fb->buf;
                }
            }
        }

        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }

        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }

        if(fb){
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        }
        else if(_jpg_buf){
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if(res != ESP_OK){
            break;
        }
    }

    return res;
}
static esp_err_t command_handler(httpd_req_t *req) {
    char content[32] = {0,};

    size_t recv_size = min(req->content_len, sizeof(content));

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }


    if (!strcmp(content, "top")) {
        vertical_val += servo_move_val;
        vertical_servo.write(vertical_val);
    }
    else if (!strcmp(content, "bottom")) {
        vertical_val -= servo_move_val;
        vertical_servo.write(vertical_val);
    }
    else if (!strcmp(content, "left")) {
        level_val += servo_move_val;
        level_servo.write(level_val);
    }
    else if (!strcmp(content, "right")) {
        level_val -= servo_move_val;
        level_servo.write(level_val);

    }
    else if (!strcmp(content, "init")) {
        level_val = 0;
        vertical_val = 0;
        level_servo.write(level_val);
        vertical_servo.write(vertical_val);
    }



    Serial.print("data: ");
    Serial.println(content);

    return ESP_OK;
}


void setup() {

    // init
    Serial.begin(115200);
    WiFi.begin(ssid, password);

    level_servo.attach(level_servo_pin);
    vertical_servo.attach(vertical_servo_pin);


    //  wifi
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println(WiFi.localIP());

    // camera
    camera_config_t config_camera = config_cam();
    esp_err_t err = esp_camera_init(&config_camera);
    if (err != ESP_OK) {
        Serial.printf("camera init failed with error 0x%x", err);
        return;
    }

    // http
    httpd_config_t config_http = HTTPD_DEFAULT_CONFIG();
    config_http.server_port = 80;

    httpd_uri_t camera_uri = {
            .uri      = "/camera",
            .method   = HTTP_GET,
            .handler  = camera_handler,
            .user_ctx = NULL
    };
    httpd_uri_t command_uri = {
            .uri      = "/command",
            .method   = HTTP_POST,
            .handler  = command_handler,
            .user_ctx = NULL
    };


    if (httpd_start(&stream_httpd, &config_http) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &camera_uri);
        httpd_register_uri_handler(stream_httpd, &command_uri);
    }


}

void loop() {
}
