#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/ip4_addr.h"
#include "inc/ssd1306.h"
#include "ws2818b.pio.h"

// Definição dos pinos
#define I2C_SDA 14
#define I2C_SCL 15
#define BUTTON_A 5
#define BUTTON_B 6
#define LED_RED 13
#define LED_GREEN 11
#define LED_BLUE 12
#define BUZZER_A 21
#define BUZZER_B 10
#define LED_MATRIX_PIN 7
#define MIC_PIN 2
#define ADC_IN_INIT 26

// Configurações de áudio
#define SAMPLE_RATE 8000
#define MAX_SAMPLES 80000  
#define BITS_PER_SAMPLE 16
#define CHUNK_SIZE 512

// Definição do número de LEDs
#define LED_COUNT 25

// Estados para análise da resposta HTTP
typedef enum {
    WAITING_FOR_HEADER,
    WAITING_FOR_BODY,
    RESPONSE_COMPLETE
} response_state_t;

// Estados do upload
typedef enum {
    SEND_HTTP_HEADER,
    SEND_WAV_HEADER,
    SEND_DATA,
    WAIT_RESPONSE,
    UPLOAD_DONE
} upload_state_t;

// Estrutura do cabeçalho WAV
typedef struct {
    char riff_id[4];      // "RIFF"
    uint32_t file_size;   // Total file size - 8
    char wave_id[4];      // "WAVE"
    char fmt_id[4];       // "fmt "
    uint32_t fmt_size;    // Format chunk size (16 for PCM)
    uint16_t audio_format;// Audio format (1 for PCM)
    uint16_t num_channels;// Number of channels
    uint32_t sample_rate; // Sample rate
    uint32_t byte_rate;   // Byte rate
    uint16_t block_align; // Block align
    uint16_t bits_per_sample; // Bits per sample
    char data_id[4];      // "data"
    uint32_t data_size;   // Data size
} wav_header_t;

// Estrutura para armazenar eventos
#define MAX_EVENTOS 25
typedef struct {
    char id[50];
    char descricao[50]; 
    uint8_t hora;
    uint8_t minuto;
    uint8_t led_index;
    bool concluido;
    bool agendado;
} Evento;

// Buffer para amostras de áudio
uint8_t audio_buffer[MAX_SAMPLES * 2];
volatile bool is_recording = false;
volatile uint32_t sample_count = 0;

// Estado da conexão TCP
static struct tcp_pcb *tcp_client = NULL;
static bool upload_complete = false;
static response_state_t response_state = WAITING_FOR_HEADER;
static char response_buffer[256];
static int response_len = 0;

// Estado do upload
static upload_state_t upload_state = SEND_HTTP_HEADER;
static uint32_t bytes_sent = 0;
static uint32_t total_bytes = 0;
static bool chunk_sent = false;

// Cabeçalho WAV
wav_header_t wav_header;

// Array único de eventos
Evento eventos[MAX_EVENTOS];
int num_eventos = 0;
int proximo_evento_idx = 0;

// Buffer do display
uint8_t display_buffer[ssd1306_buffer_length];

// Matriz de LEDs
struct pixel_t {
    uint8_t G, R, B;
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t;
npLED_t leds[LED_COUNT];

// Variáveis para uso da máquina PIO
PIO np_pio;
uint sm;

// Protótipos de funções
void init_display();
void show_message(const char* line1, const char* line2);
void play_confirmation_beep();
void set_led_yellow();
void set_led_red();
void set_led_green();
void turn_off_leds();
void npInit(uint pin);
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b);
void npClear();
void npWrite();
void init_wav_header(uint32_t data_size);
void record_audio();
void process_server_response();
void mostrar_proximo_evento();
bool adicionar_evento(const char* descricao, uint8_t hora, uint8_t minuto);
void remover_evento(int index);
void extract_json_value(const char* json, const char* key, char* value, size_t max_len);

// Configurações de rede
static ip4_addr_t remote_addr;
static bool tcp_connected = false;
static bool upload_in_progress = false;

// Callbacks do TCP
static err_t tcp_connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err) {
    if (err == ERR_OK) {
        tcp_connected = true;
    }
    return ERR_OK;
}

static void tcp_error_callback(void *arg, err_t err) {
    tcp_connected = false;
    tcp_client = NULL;
}

static err_t tcp_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    return ERR_OK;
}

void init_display() {
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_init();
    memset(display_buffer, 0, ssd1306_buffer_length);
}

void show_message(const char* line1, const char* line2) {
    // Preparar área de renderização
    struct render_area frame_area = {
        .start_column = 0,
        .end_column = ssd1306_width - 1,
        .start_page = 0,
        .end_page = ssd1306_n_pages - 1
    };

    calculate_render_area_buffer_length(&frame_area);
    
    // Limpa o buffer
    memset(display_buffer, 0, ssd1306_buffer_length);
    
    // Desenha as strings em múltiplas linhas
    if (line1) {
        // Primeira mensagem pode ocupar até 2 linhas
        char parte1[32] = "";
        char parte2[32] = "";
        
        if (strlen(line1) > 16) {
            strncpy(parte1, line1, 16);
            parte1[16] = '\0';
            strcpy(parte2, line1 + 16);
        } else {
            strcpy(parte1, line1);
        }
        
        ssd1306_draw_string(display_buffer, 0, 0, parte1);  // Linha 1
        if (strlen(parte2) > 0) {
            ssd1306_draw_string(display_buffer, 0, 10, parte2);  // Linha 2
        }
    }
    
    if (line2) {
        char partes[3][32] = {"", "", ""};
        int len = strlen(line2);
        int pos = 0;
        
        // Divide a mensagem em partes de até 16 caracteres
        for (int i = 0; i < 3 && pos < len; i++) {
            int chars_to_copy = len - pos > 16 ? 16 : len - pos;
            strncpy(partes[i], line2 + pos, chars_to_copy);
            partes[i][chars_to_copy] = '\0';
            pos += chars_to_copy;
        }
        
        // Desenha cada parte em uma nova linha (pulando a linha 3)
        for (int i = 0; i < 3 && strlen(partes[i]) > 0; i++) {
            ssd1306_draw_string(display_buffer, 0, 30 + (i * 10), partes[i]);  // Linhas 4-6
        }
    }
    
    // Renderiza no display
    render_on_display(display_buffer, &frame_area);
}

void play_confirmation_beep() {
    for(int i = 0; i < 500; i++) {
        gpio_put(BUZZER_A, 1);
        gpio_put(BUZZER_B, 1);
        sleep_us(500000/1000);  
        gpio_put(BUZZER_A, 0);
        gpio_put(BUZZER_B, 0);
        sleep_us(500000/1000);  
    }
}

void set_led_yellow() {
    gpio_put(LED_RED, 1);
    gpio_put(LED_GREEN, 1);
    gpio_put(LED_BLUE, 0);
}

void set_led_red() {
    gpio_put(LED_RED, 1);
    gpio_put(LED_GREEN, 0);
    gpio_put(LED_BLUE, 0);
}

void set_led_green() {
    gpio_put(LED_RED, 0);
    gpio_put(LED_GREEN, 1);
    gpio_put(LED_BLUE, 0);
}

void turn_off_leds() {
    gpio_put(LED_RED, 0);
    gpio_put(LED_GREEN, 0);
    gpio_put(LED_BLUE, 0);
}

void npInit(uint pin) {
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;

    sm = pio_claim_unused_sm(np_pio, false);
    if (sm < 0) {
        np_pio = pio1;
        sm = pio_claim_unused_sm(np_pio, true);
    }

    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);

    for (uint i = 0; i < LED_COUNT; ++i) {
        leds[i].R = 0;
        leds[i].G = 0;
        leds[i].B = 0;
    }
}

void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
    if (index < LED_COUNT) {
        leds[index].R = r;
        leds[index].G = g;
        leds[index].B = b;
    }
}

void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i)
        npSetLED(i, 0, 0, 0);
}

void npWrite() {
    for (uint i = 0; i < LED_COUNT; ++i) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    sleep_us(100);
}

void init_wav_header(uint32_t data_size) {
    memcpy(wav_header.riff_id, "RIFF", 4);
    wav_header.file_size = data_size + sizeof(wav_header_t) - 8;
    memcpy(wav_header.wave_id, "WAVE", 4);
    memcpy(wav_header.fmt_id, "fmt ", 4);
    wav_header.fmt_size = 16;
    wav_header.audio_format = 1;
    wav_header.num_channels = 1;
    wav_header.sample_rate = SAMPLE_RATE;
    wav_header.bits_per_sample = BITS_PER_SAMPLE;
    wav_header.block_align = (wav_header.num_channels * wav_header.bits_per_sample) / 8;
    wav_header.byte_rate = wav_header.sample_rate * wav_header.block_align;
    memcpy(wav_header.data_id, "data", 4);
    wav_header.data_size = data_size;
}

void record_audio() {
    is_recording = true;
    sample_count = 0;
    
    show_message("Gravando...", "Pressione B para parar");
    set_led_red();  
    
    while(gpio_get(BUTTON_B)) {
        sleep_ms(10); 
    }
    
    while (is_recording && sample_count < MAX_SAMPLES) {
        uint16_t adc_val = adc_read();
        uint16_t audio_sample = (uint16_t)((adc_val * 16) + 0);
        
        audio_buffer[sample_count * 2] = audio_sample & 0xFF;
        audio_buffer[sample_count * 2 + 1] = (audio_sample >> 8) & 0xFF;
        
        sample_count++;
        sleep_us(125);
        
        if (gpio_get(BUTTON_B)) {
            is_recording = false;
            while(gpio_get(BUTTON_B)) {
                sleep_ms(10); // Espera soltar o botão
            }
        }
    }
    
    if (sample_count >= MAX_SAMPLES) {
        show_message("Erro!", "Tempo max. excedido");
        set_led_red();
        sleep_ms(2000);
    } else {
        show_message("Gravacao", "Concluida!");
        set_led_green();
        play_confirmation_beep();  
        sleep_ms(1000);
    }
    
    turn_off_leds();
}

void extract_json_value(const char* json, const char* key, char* value, size_t max_len) {
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "\"%s\":\"", key);
    
    char* start = strstr(json, search_key);
    if (start) {
        start += strlen(search_key);
        char* end = strchr(start, '"');
        if (end) {
            size_t len = end - start;
            if (len >= max_len) len = max_len - 1;
            strncpy(value, start, len);
            value[len] = '\0';
        } else {
            value[0] = '\0';
        }
    } else {
        value[0] = '\0';
    }
}

void process_server_response() {
    set_led_yellow();  
    show_message("Processando...", "Aguarde");
    
    char event_id[50] = "";
    char event_name[50] = "";
    char event_status[20] = "";
    
    extract_json_value(response_buffer, "id", event_id, sizeof(event_id));
    extract_json_value(response_buffer, "name", event_name, sizeof(event_name));
    extract_json_value(response_buffer, "status", event_status, sizeof(event_status));
    
    if (strlen(event_name) > 0 && strcmp(event_status, "confirmed") == 0) {
        uint8_t hora = 8; 
        
        bool horario_encontrado = false;
        while (hora < 22 && !horario_encontrado) { 
            bool hora_ocupada = false;
            for (int i = 0; i < num_eventos; i++) {
                if (eventos[i].hora == hora) {
                    hora_ocupada = true;
                    break;
                }
            }
            if (!hora_ocupada) {
                horario_encontrado = true;
                break;
            }
            hora++;
        }
        
        if (horario_encontrado && adicionar_evento(event_name, hora, 0)) {
            strncpy(eventos[num_eventos-1].id, event_id, sizeof(eventos[0].id)-1);
            eventos[num_eventos-1].id[sizeof(eventos[0].id)-1] = '\0';
            
            set_led_green();
            char msg[64];
            snprintf(msg, sizeof(msg), "%s as %02d:00", event_name, hora);
            show_message("Evento adicionado", msg);
            play_confirmation_beep();
        } else {
            set_led_red();
            show_message("Erro!", "Agenda cheia ou sem horarios");
        }
    } else {
        set_led_red();
        if (strlen(event_name) == 0) {
            show_message("Erro!", "Evento invalido");
        } else {
            show_message("Erro!", "Status nao confirmado");
        }
    }
    
    sleep_ms(2000);
    turn_off_leds();
}

void mostrar_proximo_evento() {
    char linha1[64] = "Pressione B para agendamento";
    char linha2[128] = "";
    
    bool tem_evento_agendado = false;
    for (int i = 0; i < num_eventos; i++) {
        if (eventos[i].agendado) {
            snprintf(linha2, sizeof(linha2), 
                    "Proximo evento: %s - %02d:%02d", 
                    eventos[i].descricao, eventos[i].hora, eventos[i].minuto);
            tem_evento_agendado = true;
            break;
        }
    }
    
    if (!tem_evento_agendado) {
        strcpy(linha2, "Sem evento");
    }
    
    show_message(linha1, linha2);
}

bool adicionar_evento(const char* descricao, uint8_t hora, uint8_t minuto) {
    if (num_eventos >= MAX_EVENTOS) {
        return false;
    }

    for (int i = 0; i < num_eventos; i++) {
        if (strcmp(eventos[i].descricao, descricao) == 0 && !eventos[i].agendado) {
            eventos[i].agendado = true;
            eventos[i].hora = hora;
            eventos[i].minuto = minuto;
            
            int led_index = LED_COUNT - 1 - i;
            
            uint8_t r = (hora * 10) % 255;
            uint8_t g = (minuto * 4) % 255;
            uint8_t b = ((hora + minuto) * 5) % 255;
            npSetLED(led_index, r, g, b);
            npWrite();
            
            return true;
        }
    }

    return false;
}

void remover_evento(int index) {
    if (index < 0 || index >= num_eventos) return;

    eventos[index].agendado = false;
    
    int led_index = LED_COUNT - 1 - index;
    npSetLED(led_index, 0, 0, 0);
    npWrite();
}

void upload_audio() {
    if (!tcp_connected) {
        // Tenta conectar ao servidor
        IP4_ADDR(&remote_addr, 192, 168, 1, 11); // Ajuste para o IP do seu servidor
        tcp_client = tcp_new();
        if (tcp_client == NULL) {
            show_message("Erro!", "Falha ao criar TCP");
            return;
        }

        tcp_arg(tcp_client, NULL);
        tcp_sent(tcp_client, tcp_sent_callback);
        tcp_err(tcp_client, tcp_error_callback);

        if (tcp_connect(tcp_client, &remote_addr, 3000, tcp_connected_callback) != ERR_OK) {
            show_message("Erro!", "Falha na conexao");
            tcp_close(tcp_client);
            tcp_client = NULL;
            return;
        }

        // Aguarda conexão
        show_message("Conectando...", "Aguarde");
        int timeout = 100;
        while (!tcp_connected && timeout > 0) {
            sleep_ms(100);
            timeout--;
        }

        if (!tcp_connected) {
            show_message("Erro!", "Timeout conexao");
            tcp_close(tcp_client);
            tcp_client = NULL;
            return;
        }
    }

    init_wav_header(sample_count * 2);

    char http_header[256];
    snprintf(http_header, sizeof(http_header),
             "POST /upload HTTP/1.1\r\n"
             "Host: 192.168.1.11:3000\r\n"
             "Content-Type: audio/wav\r\n"
             "Content-Length: %d\r\n"
             "\r\n",
             sizeof(wav_header) + (sample_count * 2));

    set_led_yellow();
    show_message("Enviando...", "Aguarde");

    // Envia dados em chunks
    if (tcp_write(tcp_client, http_header, strlen(http_header), TCP_WRITE_FLAG_COPY) == ERR_OK &&
        tcp_write(tcp_client, &wav_header, sizeof(wav_header), TCP_WRITE_FLAG_COPY) == ERR_OK) {
        
        uint32_t bytes_sent = 0;
        while (bytes_sent < (sample_count * 2)) {
            uint16_t chunk_size = (sample_count * 2) - bytes_sent > CHUNK_SIZE ? CHUNK_SIZE : (sample_count * 2) - bytes_sent;
            if (tcp_write(tcp_client, &audio_buffer[bytes_sent], chunk_size, TCP_WRITE_FLAG_COPY) != ERR_OK) {
                show_message("Erro!", "Falha no envio");
                set_led_red();
                sleep_ms(2000);
                return;
            }
            bytes_sent += chunk_size;
            tcp_output(tcp_client);
        }

        show_message("Upload", "Concluido!");
        set_led_green();
        sleep_ms(1000);
    } else {
        show_message("Erro!", "Falha no envio");
        set_led_red();
        sleep_ms(2000);
    }

    turn_off_leds();
}

int main() {
    stdio_init_all();

    // Inicializa GPIO
    gpio_init(BUTTON_A);
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_A);  
    gpio_pull_up(BUTTON_B);  
    
    gpio_init(LED_RED);
    gpio_init(LED_GREEN);
    gpio_init(LED_BLUE);
    gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_set_dir(LED_GREEN, GPIO_OUT);
    gpio_set_dir(LED_BLUE, GPIO_OUT);
    
    gpio_init(BUZZER_A);
    gpio_init(BUZZER_B);
    gpio_set_dir(BUZZER_A, GPIO_OUT);
    gpio_set_dir(BUZZER_B, GPIO_OUT);
    
    init_display();
    show_message("Iniciando...", "");
    
    adc_init();
    adc_gpio_init(26);
    adc_select_input(0);
    
    npInit(LED_MATRIX_PIN);
    npClear();
    npWrite();
    
    if (cyw43_arch_init()) {
        show_message("Erro!", "Falha WiFi init");
        set_led_red();
        sleep_ms(2000);
        return 1;
    }
    
    cyw43_arch_enable_sta_mode();
    
    show_message("Conectando", "WiFi...");
    if (cyw43_arch_wifi_connect_timeout_ms("SSDI do WIFI", "senha do wifi", CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        show_message("Erro!", "Falha WiFi");
        set_led_red();
        sleep_ms(2000);
        return 1;
    }
    
    show_message("WiFi", "Conectado!");
    set_led_green();
    sleep_ms(1000);
    turn_off_leds();

    bool esperando_confirmacao = false;
    int evento_para_concluir = -1;
    
    while (true) {
        if (esperando_confirmacao) {
            char mensagem[32];
            snprintf(mensagem, sizeof(mensagem), "Concluir %s?", 
                    eventos[evento_para_concluir].descricao);
            show_message(mensagem, "A para sim, B para nao");

            if (!gpio_get(BUTTON_A)) {
                remover_evento(evento_para_concluir);
                esperando_confirmacao = false;
                play_confirmation_beep();
                sleep_ms(500);
            }
            else if (!gpio_get(BUTTON_B)) {
                esperando_confirmacao = false;
                sleep_ms(500);
            }
        }
        else {
            mostrar_proximo_evento();

            if (!gpio_get(BUTTON_A)) {
                for (int i = 0; i < num_eventos; i++) {
                    if (eventos[i].agendado) {
                        esperando_confirmacao = true;
                        evento_para_concluir = i;
                        break;
                    }
                }
                sleep_ms(500);
            }
            else if (!gpio_get(BUTTON_B)) {
                // Inicia gravação e upload do áudio
                record_audio();
                if (sample_count > 0) {  // Só faz upload se gravou algo
                    show_message("Processando...", "Aguarde");
                    set_led_yellow();
                    upload_audio();
                    
                    if (upload_complete) {
                        process_server_response();
                    } else {
                        show_message("Erro ao enviar", "tente novamente");
                        set_led_red();
                        sleep_ms(2000);
                    }
                }
                turn_off_leds();
            }
            
            // Debounce
            while (!gpio_get(BUTTON_A) || !gpio_get(BUTTON_B)) {
                sleep_ms(10);
            }
        }
        sleep_ms(100);
    }
}
