#include "esp_camera.h"
#include "esp_http_server.h"
#include <WiFi.h>
#include <ESP32Servo.h>

#ifndef MIN
  #define MIN(a,b) ((a)<(b)?(a):(b))
#endif

const char* ssid = "pc";
const char* password = "12345678";

IPAddress local_IP(192, 168, 137, 7);
IPAddress gateway(192, 168, 137, 1);
IPAddress subnet(255, 255, 255, 0);

#define PWDN_GPIO_NUM    32
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM     0
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27

#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      21
#define Y4_GPIO_NUM      19
#define Y3_GPIO_NUM      18
#define Y2_GPIO_NUM       5
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22

// -------- Servo Tanımları --------
const int servoXPin = 12;
const int servoYPin = 14;

Servo servoX;
Servo servoY;

// Mevcut konumu global değişkende tutuyoruz
int posX = 90;
int posY = 90;

httpd_handle_t stream_httpd = NULL;

// Basit bir fonksiyon: Servoyu verilen açıya götür
void setServoPosition(Servo &servo, int angle) {
    servo.write(angle);
}

// --------- move_handler: Servo hareketlerini yumuşak yapma ---------
esp_err_t move_handler(httpd_req_t *req) {
    char buf[100];
    int ret, remaining = req->content_len;
    float dx = 0, dy = 0;

    // Gelen POST verisini okuyalım (örnek: "dx=5&dy=-3")
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf) - 1))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                httpd_resp_send_408(req);
            }
            return ESP_FAIL;
        }
        remaining -= ret;
        buf[ret] = '\0'; // String sonu
        sscanf(buf, "dx=%f&dy=%f", &dx, &dy);
    }

    Serial.printf("Alınan dx: %.2f, dy: %.2f\n", dx, dy);

    // Hedef konum = anlık konum + dx/dy
    float targetX = posX + dx;
    float targetY = posY + dy;

    // Sınır dışına çıkarsa 0-180 aralığına çekiyoruz
    if (targetX < 0)   targetX = 0;
    if (targetX > 180) targetX = 180;
    if (targetY < 0)   targetY = 0;
    if (targetY > 180) targetY = 180;

    // Mevcut konumdan hedef konuma küçük adımlarla ilerleyelim
    // Adım sayısını dx/dy farkına göre ayarlamak için abs() kullanıyoruz
    // (Burada max(abs(dx), abs(dy))) kadar tekrarla da yapılabilir)
    while ( (int)posX != (int)targetX || (int)posY != (int)targetY ) {
        
        // X ekseni için küçük adım
        if (posX < (int)targetX) {
            posX++;
        }
        else if (posX > (int)targetX) {
            posX--;
        }

        // Y ekseni için küçük adım
        if (posY < (int)targetY) {
            posY++;
        }
        else if (posY > (int)targetY) {
            posY--;
        }

        setServoPosition(servoX, posX);
        setServoPosition(servoY, posY);

        // Çok hızlı adım atmak istemiyorsak ufak bir gecikme ekleyelim
        delay(10);
    }

    // Güncel değerleri loglayalım
    Serial.printf("Servo pozisyonları güncellendi: X=%d, Y=%d\n", posX, posY);

    // HTTP yanıtı oluştur
    String msg = "Servo X: " + String(posX) + ", Servo Y: " + String(posY);
    httpd_resp_send(req, msg.c_str(), msg.length());

    return ESP_OK;
}

// -------- Kamera görüntü akışı (değişiklik yok) --------
esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len;
    uint8_t *_jpg_buf;
    char part_buf[64];

    res = httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
    if (res != ESP_OK) return res;

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            res = ESP_FAIL;
        } else {
            if (fb->format != PIXFORMAT_JPEG) {
                bool jpeg_converted = frame2jpg(fb, 10, &_jpg_buf, &_jpg_buf_len);
                if (!jpeg_converted) {
                    esp_camera_fb_return(fb);
                    res = ESP_FAIL;
                }
            } else {
                _jpg_buf_len = fb->len;
                _jpg_buf = fb->buf;
            }
        }

        if (res == ESP_OK) {
            size_t hlen = snprintf(
                part_buf, sizeof(part_buf),
                "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                _jpg_buf_len
            );
            res = httpd_resp_send_chunk(req, part_buf, hlen);
            if (res == ESP_OK) {
                res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
            }
            if (res == ESP_OK) {
                res = httpd_resp_send_chunk(req, "\r\n", 2);
            }
        }

        if (fb) {
            esp_camera_fb_return(fb);
        } else if (_jpg_buf) {
            free(_jpg_buf);
        }

        if (res != ESP_OK) {
            break;
        }
        delay(10);
    }
    return res;
}

// -------- HTTP Sunucusu başlatma --------
void startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Eşzamanlı bağlantıları artırmak ve timeout değerlerini ayarlamak
    config.max_open_sockets = 4;
    config.max_uri_handlers = 8;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 10;

    httpd_uri_t stream_uri = {
        .uri       = "/stream",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t move_uri = {
        .uri       = "/move",
        .method    = HTTP_POST,
        .handler   = move_handler,
        .user_ctx  = NULL
    };

    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
        httpd_register_uri_handler(stream_httpd, &move_uri);
    }
}

// -------- setup() ve loop() fonksiyonları --------
void setup() {
    Serial.begin(115200);
    Serial.println("ESP32 başlatılıyor...");

    servoX.attach(servoXPin);
    servoY.attach(servoYPin);
    setServoPosition(servoX, posX);
    setServoPosition(servoY, posY);
    Serial.println("Servo motorlar ayarlandı.");

    if (!WiFi.config(local_IP, gateway, subnet)) {
        Serial.println("Statik IP yapılandırılamadı.");
    }
    WiFi.begin(ssid, password);
    Serial.print("WiFi'ye bağlanılıyor");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi bağlantısı kuruldu.");

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    if (psramFound()) {
        config.frame_size = FRAMESIZE_QVGA; // 320x240
        config.jpeg_quality = 10;          // kalite
        config.fb_count = 2;
    } else {
        config.frame_size = FRAMESIZE_QQVGA; // 160x120
        config.jpeg_quality = 12;            // kalite
        config.fb_count = 1;
    }

    if (esp_camera_init(&config) != ESP_OK) {
        Serial.println("Kamera başlatılamadı!");
        return;
    }
    Serial.println("Kamera başlatıldı.");

    startCameraServer();
    Serial.println("Kamera sunucusu başlatıldı.");
    Serial.println(WiFi.localIP());
}

void loop() {
    // HTTP sunucusu main() thread'inde çalışıyor; loop'u boş bıraktık.
}