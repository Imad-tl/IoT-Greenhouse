.PHONY: start stop restart cert

start:
	./script/start.sh

stop:
	docker compose down

restart:
	docker compose down
	./script/start.sh

cert:
	./script/cert.sh