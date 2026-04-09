BUILD_DIR = build
BIN       = $(BUILD_DIR)/bin/MacroIndicators

# ── Build ──

.PHONY: all build clean run fetch-all

all: build

build:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR)

release:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR)

# ── Run ──

run: build
	FRED_API=$${FRED_API} ./$(BIN)

# ── Manual triggers (app must be running) ──

fetch-all:
	curl -s -X POST http://localhost:3002/fetch-all | python3 -m json.tool

fetch-cpi:
	curl -s -X POST http://localhost:3002/cron/cpi | python3 -m json.tool

fetch-walcl:
	curl -s -X POST http://localhost:3002/cron/walcl | python3 -m json.tool

fetch-umcsent:
	curl -s -X POST http://localhost:3002/cron/umcsent | python3 -m json.tool

health:
	curl -s http://localhost:3002/health | python3 -m json.tool

# ── Clean ──

clean:
	rm -rf $(BUILD_DIR)
