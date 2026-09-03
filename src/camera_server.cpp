#include "camera_server.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

// ==========================================
// ALLUMAGE MATÉRIEL DE LA CAMÉRA
// ==========================================
bool initCamera()
{
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
    config.pixel_format = PIXFORMAT_JPEG; // streaming rapide

    // Utilisation de la mémoire PSRAM (Crucial pour la vidéo fluide)
    if (psramFound())
    {
        config.frame_size = FRAMESIZE_VGA; // Résolution équilibrée (640x480)
        config.jpeg_quality = 12;          // 0-63 (plus bas = meilleure qualité)
        config.fb_count = 2;               // Double buffer pour la fluidité
    }
    else
    {
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
    }

    // Allumage de la caméra
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        Serial.printf("Erreur critique: Echec de l'initialisation de la camera (0x%x)\n", err);
        return false;
    }

    // Astuce Freenove/OV2640 : Inverser l'image si elle est à l'envers
    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s, 0);   // Retournement vertical
    s->set_hmirror(s, 0); // Effet miroir horizontal (si besoin)

    return true;
}

// ==========================================
// PARAMÈTRES DU STREAM MULTIPART
// ==========================================
#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;

// ==========================================
// HANDLER : FLUX VIDÉO BRUT (/stream)
// ==========================================
static esp_err_t stream_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char part_buf[64];

    // TEST : La caméra fonctionne-t-elle au moment de la requête ?
    fb = esp_camera_fb_get();
    if (!fb)
    {
        Serial.println("Erreur: Capture de la camera a echoue (Capteur deconnecte ?)");
        // envoi de l'erreur 500 du navigateur
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK)
    {
        esp_camera_fb_return(fb);
        return res;
    }

    // Permet au tableau de bord de lire le flux sans erreur CORS
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    // Boucle infinie : capture et envoi des images
    while (true)
    {
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;

        // Envoi de la séparation de trame
        if (res == ESP_OK)
        {
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }

        // Envoi des entêtes
        if (res == ESP_OK)
        {
            size_t hlen = snprintf(part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, part_buf, hlen);
        }

        // Envoi de l'image
        if (res == ESP_OK)
        {
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }

        esp_camera_fb_return(fb);
        fb = NULL;
        _jpg_buf = NULL;

        if (res != ESP_OK)
        {
            break;
        }

        // Photo suivante
        fb = esp_camera_fb_get();
        if (!fb)
        {
            res = ESP_FAIL;
        }
    }
    return res;
}

// ==========================================
// INITIALISATION DU SERVEUR VIDEO (PORT 81)
// ==========================================
void startCameraServer()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 81; // changement de port pour ne pas bloquer le 80 su LittleFS

    // Route critique pour le flux vidéo
    httpd_uri_t stream_uri = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = stream_handler,
        .user_ctx = NULL};

    Serial.printf("Demarrage du serveur VIDEO sur le port: '%d'\n", config.server_port);

    if (httpd_start(&stream_httpd, &config) == ESP_OK)
    {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
    }
    else
    {
        Serial.println("Erreur lors du demarrage du serveur Video !");
    }
}