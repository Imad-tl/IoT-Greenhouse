.PHONY: start stop restart cert daemon

start:
	./script/start.sh

daemon:
	./script/start.sh --daemon

stop:
	docker compose down

restart:
	docker compose down
	./script/start.sh

cert:
	./script/cert.sh

seed:
	./script/seed.sh