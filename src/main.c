#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "driver/uart.h"
#include "freertos/semphr.h"

SemaphoreHandle_t display_mutex;

#define UART_NUM UART_NUM_0
#define UART_BUF_SIZE 1024
#define BLINK_DELAY pdMS_TO_TICKS(300) // 300ms blink interval
#define DISPLAY_TIMEOUT pdMS_TO_TICKS(60000) // 1 minute timeout

#define STB_PIN GPIO_NUM_5   // Latch Pin
#define CLK_PIN GPIO_NUM_18  // Clock Pin
#define DATA_PIN GPIO_NUM_23 // Data Pin

bool is_blinking = false; 
uint8_t digit1 = 10, digit2 = 10; // Default blank display

// Send a single byte, LSB first
void aip1628_sendByte(uint8_t data) {
    for (uint8_t i = 0; i < 8; i++) {
        gpio_set_level(CLK_PIN, 0);
        esp_rom_delay_us(10); // Increased delay for reliability
        gpio_set_level(DATA_PIN, (data & 0x01) ? 1 : 0); // LSB first
        esp_rom_delay_us(10);
        gpio_set_level(CLK_PIN, 1);
        esp_rom_delay_us(10);
        data >>= 1; // Shift right to send next bit
    }
}

// Send a command (e.g., display control or mode)
void aip1628_sendCommand(uint8_t command) {
    gpio_set_level(STB_PIN, 0);
    aip1628_sendByte(command);
    gpio_set_level(STB_PIN, 1);
    esp_rom_delay_us(10); // Small delay after command
}

// Send data to a specific address
void aip1628_sendData(uint8_t addr, uint8_t data) {
    gpio_set_level(STB_PIN, 0);
    aip1628_sendByte(0xC0 | (addr & 0x0F)); // Address command (0xC0 + address)
    aip1628_sendByte(data);
    gpio_set_level(STB_PIN, 1);
    esp_rom_delay_us(10);
}

// Initialize the TM1628
void aip1628_init() {
    printf("\nInitializing AIP1628...\n");

    // Reset and configure pins
    gpio_reset_pin(STB_PIN);
    gpio_reset_pin(CLK_PIN);
    gpio_reset_pin(DATA_PIN);

    gpio_set_direction(STB_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(CLK_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(DATA_PIN, GPIO_MODE_OUTPUT);

    // Set initial states
    gpio_set_level(STB_PIN, 1);
    gpio_set_level(CLK_PIN, 1);
    gpio_set_level(DATA_PIN, 0);

    // TM1628 initialization sequence
    aip1628_sendCommand(0x40); // Data command: write mode, auto-increment
    aip1628_sendCommand(0x8F); // Display ON, max brightness (0x88-0x8F for brightness)

    printf("\nInitialization complete!\n");
}

void uart_init(){
    // Configure UART
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM, UART_BUF_SIZE, 0, 0, NULL, 0);
}

// Display a two-digit number with optional decimal point
void aip1628_displayNumber(uint8_t digit1, uint8_t digit2, bool dot1, bool dot2) {
    // Standard 7-segment mapping (common anode/cathode depends on hardware)
    const uint8_t segmentMap[] = {
        0b11011110, // 0 (A,B,C,D,E,F)     - 0xDE
        0b10001000, // 1 (B,C)            - 0x88
        0b01011101, // 2 (A,B,D,E,G)      - 0x5D
        0b10011101, // 3 (A,B,C,D,G)      - 0x9D
        0b10001011, // 4 (B,C,F,G)        - 0x8B
        0b10010111, // 5 (A,C,D,F,G)      - 0x97
        0b11010111, // 6 (A,C,D,E,F,G)    - 0xD7
        0b10001100, // 7 (A,B,C)          - 0x8C
        0b11011111, // 8 (A,B,C,D,E,F,G)  - 0xDF
        0b10011111,  // 9 (A,B,C,D,F,G)    - 0x9F
        0b00000000  // NOTHING ()           - 0x00
    };

    // Fetch segment data for digits
    uint8_t digit1_data = segmentMap[digit1 & 0x0F];
    uint8_t digit2_data = segmentMap[digit2 & 0x0F];

    // Adjust for dots (assuming dot is bit 7, 0x80)
    if (dot1) {
        digit1_data |= 0x20; // Set bit 5 for dot1
    }
    if (dot2) {
        digit2_data |= 0x20; // Set bit 5 for dot2
    }

    // Send data to display RAM
    aip1628_sendCommand(0x40); // Write mode, auto-increment
    aip1628_sendData(0x00, digit1_data);  // Digit 1 at address 0x00
    aip1628_sendData(0x02, digit2_data);  // Digit 2 at address 0x02
}

void blink_task(void *param) {
    while (1) {
        if (is_blinking) {
            xSemaphoreTake(display_mutex, portMAX_DELAY);
            aip1628_displayNumber(digit1, digit2, false, false);
            xSemaphoreGive(display_mutex);
            vTaskDelay(BLINK_DELAY);
            aip1628_displayNumber(10, 10, false, false); // Blank display
            vTaskDelay(BLINK_DELAY);
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void app_main() {
    uart_init();
    aip1628_init();

    // Create mutex and check for success
    display_mutex = xSemaphoreCreateMutex();
    if (display_mutex == NULL) {
        printf("Failed to create display_mutex! Halting.\n");
        while (1) vTaskDelay(1000 / portTICK_PERIOD_MS); // Halt if mutex creation fails
    }

    char buffer[10];
    int threshold = 50; // Default threshold
    TickType_t last_update_time = xTaskGetTickCount();

    xTaskCreate(blink_task, "blink_task", 2048, NULL, 1, NULL);

    while (1) {
        printf("Enter number and threshold (format: 'XX YY'), then press Enter:\n");

        int index = 0;
        memset(buffer, 0, sizeof(buffer));

        // Read input while checking timeout
        while (index < sizeof(buffer) - 1) {
            if ((xTaskGetTickCount() - last_update_time) > DISPLAY_TIMEOUT) {
                printf("No input received for 1 minute. Clearing display.\n");
                xSemaphoreTake(display_mutex, portMAX_DELAY);
                aip1628_displayNumber(10, 10, false, false);
                is_blinking = false;
                digit1 = 10;
                digit2 = 10;
                xSemaphoreGive(display_mutex);
                last_update_time = xTaskGetTickCount();
                index = 0;
                memset(buffer, 0, sizeof(buffer));
                printf("Enter number and threshold (format: 'XX YY'), then press Enter:\n");
                continue;
            }

            uint8_t byte;
            int len = uart_read_bytes(UART_NUM, &byte, 1, pdMS_TO_TICKS(100));
            if (len > 0) {
                if (byte == '\n' || byte == '\r') {
                    buffer[index] = '\0';
                    break;
                }
                buffer[index++] = byte;
            }
        }

        if (index == 0) {
            vTaskDelay(pdMS_TO_TICKS(100)); // Small delay to avoid tight loop
            continue;
        }

        last_update_time = xTaskGetTickCount();
        int number, new_threshold;
        if (sscanf(buffer, "%2d %2d", &number, &new_threshold) == 2) {
            threshold = new_threshold;
            printf("Threshold updated to %d\n", threshold);
        } else if (sscanf(buffer, "%2d", &number) != 1) {
            printf("Invalid input! Please enter a number in 'XX YY' format.\n");
            uart_flush(UART_NUM);
            continue;
        }

        if (number >= 0 && number <= 99) {
            xSemaphoreTake(display_mutex, portMAX_DELAY);
            digit1 = number / 10;
            digit2 = number % 10;
            is_blinking = (number > threshold);
            if (!is_blinking) {
                aip1628_displayNumber(digit1, digit2, false, false);
            }
            xSemaphoreGive(display_mutex);
            printf("Displaying '%d%d', Threshold: %d\n", digit1, digit2, threshold);
            if (is_blinking) {
                printf("Number exceeds threshold! Continuous blinking...\n");
            }
        } else {
            printf("Invalid number! Enter a value between 0-99.\n");
        }
        uart_flush(UART_NUM); // Clear UART buffer to prevent double prompts
    }
}