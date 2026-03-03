.PHONY: start stop restart cert daemon seed

SCRIPT_DIR := ./script

START_SCRIPT := $(SCRIPT_DIR)/start.sh
CERT_SCRIPT := $(SCRIPT_DIR)/cert.sh
SEED_SCRIPT := $(SCRIPT_DIR)/seed.sh

start:
	$(START_SCRIPT)

daemon:
	$(START_SCRIPT) --daemon

stop:
	docker compose down

restart: 
	stop
	$(START_SCRIPT)

cert:
	$(CERT_SCRIPT)

seed:
	$(SEED_SCRIPT)