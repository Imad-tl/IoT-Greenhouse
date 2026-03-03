.PHONY: start stop restart gen-cert

start:
	./script/start.sh

stop:
	docker compose down -v

restart:
	docker compose down
	./script/start.sh

cert:
	./script/cert.sh
