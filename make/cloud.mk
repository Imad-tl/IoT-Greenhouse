CLOUD_DIR         := ./cloud
CLOUD_SCRIPTS_DIR := ./scripts/cloud

.PHONY: cloud-stop cloud-daemon cloud-restart

cloud-stop:
	docker compose -f $(CLOUD_DIR)/docker-compose.yaml down

cloud-daemon:
	bash $(CLOUD_SCRIPTS_DIR)/start.sh --daemon

cloud-restart: cloud-stop cloud-start

# Handles: cloud-start, cloud-cert, cloud-seed, and any future script.
cloud-%:
	bash $(CLOUD_SCRIPTS_DIR)/$*.sh
