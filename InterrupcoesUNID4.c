#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "pio_matrix.pio.h"

#define NUM_PIXELS 25
#define OUT_PIN 7
#define RED_LED_PIN 13 // GPIO para o LED vermelho
#define BTN_A_PIN 5
#define BTN_B_PIN 6

volatile int numero_atual = 0; // Número exibido na matriz
volatile bool estado_led_vermelho = false;

int mapa_leds[25] = {
    24, 23, 22, 21, 20,  // Linha 1: direita -> esquerda
    15, 16, 17, 18, 19,  // Linha 2: esquerda -> direita
    14, 13, 12, 11, 10,  // Linha 3: direita -> esquerda
     5,  6,  7,  8,  9,  // Linha 4: esquerda -> direita
     4,  3,  2,  1,  0   // Linha 5: direita -> esquerda
};

// Função para converter RGB em formato aceito pela matriz
uint32_t matrix_rgb(double r, double g, double b) {
    return ((uint8_t)(g * 255) << 24) | ((uint8_t)(r * 255) << 16) | ((uint8_t)(b * 255) << 8);
}

// Função para configurar a matriz de LEDs com um número específico
void exibir_numero(int numero, PIO pio, uint sm, double r, double g, double b) {
    const int numeros[10][25] = {
        {1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1}, // 0
        {0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0}, // 1
        {1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1}, // 2
        {1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1}, // 3
        {1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}, // 4
        {1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1}, // 5
        {1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1}, // 6
        {1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}, // 7
        {1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1}, // 8
        {1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1}  // 9
    };

    for (int i = 0; i < NUM_PIXELS; i++) {
        int led_index = mapa_leds[i]; // Corrige a ordem dos LEDs com base no mapa
        uint32_t valor_led = matrix_rgb(numeros[numero][led_index] * r, numeros[numero][led_index] * g, numeros[numero][led_index] * b);
        pio_sm_put_blocking(pio, sm, valor_led);
    }
}

// Callback para alternar o estado do LED vermelho
bool alternar_led_callback(repeating_timer_t *rt) {
    estado_led_vermelho = !estado_led_vermelho;
    gpio_put(RED_LED_PIN, estado_led_vermelho);
    return true;
}

// Tratamento de debouncing para botões
volatile uint32_t ultimo_tempo_btn_a = 0;
volatile uint32_t ultimo_tempo_btn_b = 0;

void gpio_callback(uint gpio, uint32_t events) {
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());

    if (gpio == BTN_A_PIN && (tempo_atual - ultimo_tempo_btn_a > 200)) {
        numero_atual = (numero_atual + 1) % 10;
        ultimo_tempo_btn_a = tempo_atual;
    } else if (gpio == BTN_B_PIN && (tempo_atual - ultimo_tempo_btn_b > 200)) {
        numero_atual = (numero_atual - 1 + 10) % 10;
        ultimo_tempo_btn_b = tempo_atual;
    }
}

int main() {
    stdio_init_all();

    // Configura GPIOs
    gpio_init(RED_LED_PIN);
    gpio_set_dir(RED_LED_PIN, GPIO_OUT);

    gpio_init(BTN_A_PIN);
    gpio_set_dir(BTN_A_PIN, GPIO_IN);
    gpio_pull_up(BTN_A_PIN);
    gpio_set_irq_enabled_with_callback(BTN_A_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    gpio_init(BTN_B_PIN);
    gpio_set_dir(BTN_B_PIN, GPIO_IN);
    gpio_pull_up(BTN_B_PIN);
    gpio_set_irq_enabled(BTN_B_PIN, GPIO_IRQ_EDGE_FALL, true);

    // Inicializa PIO para matriz
    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &pio_matrix_program);
    pio_matrix_program_init(pio, sm, offset, OUT_PIN);

    // Timer para LED vermelho
    repeating_timer_t timer;
    add_repeating_timer_ms(-100, alternar_led_callback, NULL, &timer);

    while (true) {
        exibir_numero(numero_atual, pio, sm, 0.01, 0.01, 0.01);
        sleep_ms(100);
    }

    return 0;
}
