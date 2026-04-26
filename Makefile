# Default target
.PHONY: all compile upload monitor clean deps list-ports help

# -----------------------------------------------------------------------------
# Configuration Variables
# -----------------------------------------------------------------------------
# Override on the command line, e.g.:
#   make upload SKETCH=examples/Basic/Button/Button.ino \
#               FQBN=m5stack:esp32:m5stack_core PORT=/dev/ttyACM0
#
# Default target is the FlappyBird example on M5StickC Plus.
FQBN   ?= m5stack:esp32:m5stack_stickc_plus
SKETCH ?= examples/Advanced/Games/Games.ino

# Auto-detect a likely serial port if PORT is not set:
# prefer /dev/ttyUSB* (CP210x on StickC Plus), fall back to /dev/ttyACM*.
PORT ?= $(shell ls /dev/ttyUSB* 2>/dev/null | head -n1)
ifeq ($(strip $(PORT)),)
  PORT := $(shell ls /dev/ttyACM* 2>/dev/null | head -n1)
endif
ifeq ($(strip $(PORT)),)
  PORT := /dev/ttyUSB0
endif

# Build directory for arduino-cli (keeps artifacts out of /tmp).
BUILD_DIR ?= .build

# For PlatformIO
PIO_ENV ?=

# -----------------------------------------------------------------------------
# Targets
# -----------------------------------------------------------------------------
all: compile

help:
	@echo "Targets:"
	@echo "  make compile     - build SKETCH for FQBN"
	@echo "  make upload      - build + flash SKETCH to PORT"
	@echo "  make monitor     - open serial monitor on PORT"
	@echo "  make deps        - install m5stack core + M5Unified deps via arduino-cli"
	@echo "  make list-ports  - list serial ports arduino-cli can see"
	@echo "  make clean       - remove $(BUILD_DIR)"
	@echo ""
	@echo "Variables (override on the command line):"
	@echo "  SKETCH  = $(SKETCH)"
	@echo "  FQBN    = $(FQBN)"
	@echo "  PORT    = $(PORT)"

compile:
	@if command -v pio >/dev/null 2>&1 && [ -f platformio.ini ]; then \
		echo "Found platformio.ini, using PlatformIO..."; \
		if [ -n "$(PIO_ENV)" ]; then pio run -e $(PIO_ENV); else pio run; fi; \
	elif command -v arduino-cli >/dev/null 2>&1; then \
		echo "Using arduino-cli to compile $(SKETCH) for $(FQBN)..."; \
		arduino-cli compile --fqbn $(FQBN) --library . \
			--build-path $(abspath $(BUILD_DIR)) $(SKETCH); \
	elif command -v idf.py >/dev/null 2>&1; then \
		echo "Using ESP-IDF..."; \
		idf.py build; \
	else \
		echo "Error: No supported build tool found (PlatformIO, Arduino CLI, ESP-IDF)."; \
		exit 1; \
	fi

upload: compile
	@if command -v pio >/dev/null 2>&1 && [ -f platformio.ini ]; then \
		echo "Uploading via PlatformIO..."; \
		if [ -n "$(PIO_ENV)" ]; then pio run -t upload -e $(PIO_ENV); else pio run -t upload; fi; \
	elif command -v arduino-cli >/dev/null 2>&1; then \
		if [ ! -e "$(PORT)" ]; then \
			echo "Error: serial port '$(PORT)' not found."; \
			echo "Plug in the device or pass PORT=/dev/ttyXXX (try: make list-ports)."; \
			exit 1; \
		fi; \
		echo "Uploading $(SKETCH) via arduino-cli on $(PORT)..."; \
		arduino-cli upload -p $(PORT) --fqbn $(FQBN) \
			--input-dir $(abspath $(BUILD_DIR)) $(SKETCH); \
	elif command -v idf.py >/dev/null 2>&1; then \
		echo "Uploading via ESP-IDF..."; \
		idf.py flash; \
	else \
		echo "Error: No supported build tool found (PlatformIO, Arduino CLI, ESP-IDF)."; \
		exit 1; \
	fi

monitor:
	@if command -v arduino-cli >/dev/null 2>&1; then \
		arduino-cli monitor -p $(PORT) -c baudrate=115200; \
	elif command -v pio >/dev/null 2>&1; then \
		pio device monitor -p $(PORT) -b 115200; \
	else \
		echo "No serial monitor tool available (arduino-cli or pio)."; \
		exit 1; \
	fi

# Install board cores + libraries needed by the FlappyBird sketch.
deps:
	@command -v arduino-cli >/dev/null 2>&1 || { \
		echo "arduino-cli not installed. See https://arduino.github.io/arduino-cli/"; exit 1; }
	arduino-cli config init --overwrite
	arduino-cli config add board_manager.additional_urls \
		https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json \
		https://espressif.github.io/arduino-esp32/package_esp32_index.json
	arduino-cli core update-index
	arduino-cli core install m5stack:esp32
	arduino-cli lib install "M5GFX" "M5Unified"

list-ports:
	@if command -v arduino-cli >/dev/null 2>&1; then \
		arduino-cli board list; \
	else \
		ls /dev/tty* 2>/dev/null | grep -E 'USB|ACM' || echo "No USB/ACM serial devices found."; \
	fi

clean:
	@if command -v pio >/dev/null 2>&1 && [ -f platformio.ini ]; then \
		echo "Cleaning via PlatformIO..."; \
		pio run -t clean; \
	elif command -v idf.py >/dev/null 2>&1; then \
		echo "Cleaning via ESP-IDF..."; \
		idf.py clean; \
	else \
		rm -rf $(BUILD_DIR); \
		echo "Removed $(BUILD_DIR)/"; \
	fi
