# ─────────────────────────────────────────────────────────────────────────────
# Telemetry – PlatformIO targets
# ─────────────────────────────────────────────────────────────────────────────
# Configurable serial ports (override on the command line if needed)
#   make mega-flash MEGA_PORT=/dev/ttyUSB0
#   make c6-monitor C6_PORT=/dev/ttyUSB1
MEGA_PORT ?= /dev/ttyACM0
C6_PORT   ?= /dev/ttyACM1

MEGA_DIR := ./Telemetry/ARDUINO-MEGA
C6_DIR   := ./Telemetry/ESP32-C6

# ── Arduino Mega ─────────────────────────────────────────────────────────────
.PHONY: mega-build mega-flash mega-monitor

mega-build:
	cd $(MEGA_DIR) && pio run -e mega2560

mega-flash:
	cd $(MEGA_DIR) && pio run -e mega2560 -t upload

mega-monitor:
	cd $(MEGA_DIR) && pio device monitor --port $(MEGA_PORT) --baud 9600

# ── ESP32-C6 – individual nodes ───────────────────────────────────────────────
.PHONY: c6-build-temp-hum c6-flash-temp-hum \
        c6-build-lum     c6-flash-lum     \
        c6-build-motion  c6-flash-motion  \
        c6-monitor

c6-build-temp-hum:
	cd $(C6_DIR) && pio run -e c6_temp_hum

c6-flash-temp-hum:
	cd $(C6_DIR) && pio run -e c6_temp_hum -t upload

c6-build-lum:
	cd $(C6_DIR) && pio run -e c6_lum

c6-flash-lum:
	cd $(C6_DIR) && pio run -e c6_lum -t upload

c6-build-motion:
	cd $(C6_DIR) && pio run -e c6_motion

c6-flash-motion:
	cd $(C6_DIR) && pio run -e c6_motion -t upload

c6-monitor:
	cd $(C6_DIR) && pio device monitor --port $(C6_PORT) --baud 115200

# ── ESP32-C6 – convenience: all nodes ────────────────────────────────────────
.PHONY: c6-build-all c6-flash-all

c6-build-all: c6-build-temp-hum c6-build-lum c6-build-motion

c6-flash-all: c6-flash-temp-hum c6-flash-lum c6-flash-motion

# ── Port utilities ────────────────────────────────────────────────────────────
.PHONY: ports port-kill

## List all connected serial devices
ports:
	pio device list

## Show which process is locking C6_PORT (default /dev/ttyACM1)
##   make port-kill C6_PORT=/dev/ttyACM0   (to check the Mega port instead)
port-kill:
	@echo "Processes holding $(C6_PORT):"
	@lsof $(C6_PORT) || echo "  (none)"
	@echo ""
	@echo "Run  kill <PID>  to release the port."
