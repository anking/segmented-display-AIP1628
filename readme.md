# Display App

A PlatformIO project for the ESP32 that displays a two-digit value on a 2-segment display using the AIP1628 driver(pulled from the dehumidifier). The app accepts UART input in the format `XX YY` (value and threshold), blinks the display if the value exceeds the threshold, and clears the display after a 1-minute timeout if no new input is received.

## Features
- Displays two-digit values (00-99) on a 7-segment display.
- Accepts UART input in `XX YY` format (e.g., `55 50` for value 55, threshold 50).
- Blinks the display continuously if the value exceeds the threshold.
- Clears the display after 60 seconds of inactivity.
- Built with ESP-IDF and FreeRTOS for task management.

## Hardware Requirements
- **ESP32 Development Board** (e.g., ESP32-DevKitC).
- **7-Segment Display with AIP1628 Driver** (or compatible TM1628-based display).
- UART interface (e.g., via USB-to-serial connection).

### Pin Configuration
| Pin       | GPIO   | Description       |
|-----------|--------|-------------------|
| STB_PIN   | GPIO5  | Latch Pin         |
| CLK_PIN   | GPIO18 | Clock Pin         |
| DATA_PIN  | GPIO23 | Data Pin          |

## Prerequisites
- [PlatformIO](https://platformio.org/) installed (recommended with VS Code).
- Git installed for cloning the repository.
- A terminal program (e.g., PuTTY, minicom, or PlatformIO’s built-in monitor) for UART input.

## Installation
1. **Clone the Repository**:
   ```bash
   git clone https://github.com/anking/segmented-display-aip1628
   cd display-app