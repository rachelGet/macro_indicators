# macro_indicators — FRED Macroeconomic Indicators Service

C++/Qt6 service that fetches macroeconomic series from the **FRED API** (Federal Reserve Economic Data) on a **Dapr cron schedule**, backfills historical data on first run, and persists every observation through Dapr state stores into SQL Server. Each indicator gets its own table and its own cron binding.

This module is one of the data extractors of the [Market Profiling & OSINT Sensemaking](../README.md) project.

---

## What it does

- Exposes a small embedded HTTP server (`MiniHttpServer`) that Dapr cron bindings POST to.
- Subscribes to three FRED series, each with its own release cadence:

  | Series | Indicator | Cron route | Schedule (UTC) |
  | :--- | :--- | :--- | :--- |
  | `CPIAUCSL` | Consumer Price Index | `POST /cron/cpi` | `30 12 10-14 * *` (10th-14th of each month, 12:30 UTC) |
  | `WALCL` | Fed balance sheet (assets, weekly) | `POST /cron/walcl` | Thursdays, 4:30 PM ET |
  | `UMCSENT` | UMich consumer sentiment | `POST /cron/umcsent` | Last Friday of each month |

- On first run, **backfills** each series from January of the previous year up to today via `fetchIndicatorRange`, then marks the store as initialized.
- On subsequent runs, fetches only the latest observations via `fetchIndicator`.
- Each `MacroObservation` is fanned out to a Qt `MacroChartWindow` (live UI) **and** to a `DaprMacroStore` that POSTs it to the `macroindicators` Dapr sidecar.
- Provides two operational endpoints: `GET /health` and `POST /fetch-all` (manual full refresh).

---

## Architecture

```
       Dapr cron bindings                            FRED API
   ┌─────────────────────┐                     ┌─────────────────┐
   │ cron-cpi  (10-14 m) │                     │  api.stlouis    │
   │ cron-walcl (Thu)    │                     │   fed.org       │
   │ cron-umcsent (lastF)│                     └────────┬────────┘
   └──────────┬──────────┘                              │ HTTPS
              │ POST /cron/<id>                         │
              ▼                                         ▼
   ┌─────────────────────────────────────────────────────────────┐
   │   macro_indicators (port 3002)                              │
   │                                                             │
   │   MiniHttpServer  ──►  FredClient ──►  observationReady()   │
   │                                            │                │
   │              ┌─────────────────────────────┴─────────────┐  │
   │              ▼                                           ▼  │
   │   MacroChartWindow                              DaprMacroStore │
   └──────────────┬──────────────────────────────────┬───────────┘
                  │                                  │ HTTP :3502
                  ▼                                  ▼
            (Qt window)                  ┌─────────────────────────┐
                                         │ Dapr `macroindicators`  │
                                         └────────────┬────────────┘
                                                      ▼
                                         ┌─────────────────────────┐
                                         │ SQL Server tradingdb    │
                                         │  cpi · walcl · umcsent  │
                                         └─────────────────────────┘
```

---

## Repository layout

| Path | Purpose |
| :--- | :--- |
| [Main.cpp](Main.cpp) | Wires FRED → chart + Dapr stores; registers cron HTTP routes; performs first-run backfill. |
| [FredClient.cpp](FredClient.cpp) | HTTPS client for the FRED API; emits `observationReady` / `fetchError`. |
| [DaprMacroStore.cpp](DaprMacroStore.cpp) | HTTP client → `cpi-store` / `walcl-store` / `umcsent-store` Dapr components. |
| [MacroChartWindow.cpp](MacroChartWindow.cpp) | Qt Charts window for the three series. |
| [MiniHttpServer.cpp](MiniHttpServer.cpp) | Tiny HTTP router used to receive Dapr cron callbacks. |
| [MacroTypes.h](MacroTypes.h) | `MacroObservation` POD type. |
| [components/cron-cpi.yaml](components/cron-cpi.yaml) · [cron-walcl.yaml](components/cron-walcl.yaml) · [cron-umcsent.yaml](components/cron-umcsent.yaml) | Dapr cron bindings. |
| [components/cpi-store.yaml](components/cpi-store.yaml) · [walcl-store.yaml](components/walcl-store.yaml) · [umcsent-store.yaml](components/umcsent-store.yaml) | Dapr state stores → SQL Server tables `cpi`, `walcl`, `umcsent`. |
| [style.qss](style.qss) | Qt stylesheet loaded at startup. |
| [CMakeLists.txt](CMakeLists.txt) | Build target. |

---

## Requirements

| Requirement | Version / Notes |
| :--- | :--- |
| **C++** | C++17 (gcc 11.4+) |
| **CMake** | 3.16+ |
| **Qt** | Qt6 — `Core Gui Widgets Charts Network` |
| **Dapr** | 1.15.1 (`macroindicators` sidecar on HTTP `:3502`) |
| **SQL Server** | 2022/2025 (provided by `sql1` in the parent compose) |
| **FRED API key** | Free at https://fred.stlouisfed.org/docs/api/api_key.html |
| **Env vars** | `FRED_API` (required, read by the binary) · `PASSWORD_SQL` (consumed by Dapr components) |

---

## Build

```bash
# from macro_indicators/
cmake -S . -B build
cmake --build build -j
```

Output: `build/macro_indicators`.

---

## Run

1. **Export the FRED API key:**
   ```bash
   export FRED_API="your_fred_api_key"
   ```
2. **Bring up SQL Server + the `macroindicators` sidecar** from the project root:
   ```bash
   docker compose up -d sql1 macroindicators-dapr
   ```
3. **Launch the service:**
   ```bash
   ./build/macro_indicators
   ```
   The chart window opens, the embedded HTTP server starts on port `3002`, and after ~2 seconds the backfill check runs against each store.

### HTTP endpoints

| Method | Path | Purpose |
| :--- | :--- | :--- |
| `POST` | `/cron/cpi` | Triggered by `cron-cpi` Dapr binding. |
| `POST` | `/cron/walcl` | Triggered by `cron-walcl` Dapr binding. |
| `POST` | `/cron/umcsent` | Triggered by `cron-umcsent` Dapr binding. |
| `POST` | `/fetch-all` | Manual full refresh of all three series. |
| `GET` | `/health` | Liveness probe. |

Manual refresh from the host:
```bash
curl -X POST http://localhost:3002/fetch-all
```

Runtime logs are written to `macro_indicators.log` next to the binary.

---

## Troubleshooting

| Symptom | Likely cause |
| :--- | :--- |
| `ERROR: FRED_API environment variable not set` | Export `FRED_API` before launching. |
| Cron routes never fire | `macroindicators-dapr` sidecar not running, or the cron schedule has not yet reached its window. Trigger `/fetch-all` manually to verify the pipeline. |
| Backfill stays empty | FRED rate-limited, or the API key is invalid — see `macro_indicators.log` for `FRED error [...]` entries. |
| Chart updates but DB is empty | `PASSWORD_SQL` mismatch — check `docker compose logs macroindicators-dapr sql1`. |
