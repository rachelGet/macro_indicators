#include "FredClient.h"
#include "DaprMacroStore.h"
#include "MacroChartWindow.h"
#include "MacroTypes.h"
#include "MiniHttpServer.h"
#include "logs.h"

#include <QApplication>
#include <QFile>
#include <QJsonObject>
#include <QTimer>
#include <QDate>
#include <cstdlib>
#include <iostream>

using namespace std;

Logger* globalLogger = nullptr;

/**
 * Macro Indicators Service
 *
 * A headless app that fetches FRED economic data on Dapr cron schedules.
 * Dapr sends POST requests to this app when each cron binding fires:
 *
 *   POST /cron/cpi       — CPI (CPIAUCSL), 10th-14th monthly at 8:30 AM ET
 *   POST /cron/walcl     — Fed Balance Sheet (WALCL), Thursdays at 4:30 PM ET
 *   POST /cron/umcsent   — Consumer Sentiment (UMCSENT), last Friday monthly
 *
 * On startup, it also does an initial fetch of all three indicators.
 */
static void loadStyleSheet(QApplication &app) {
    QStringList paths = {
        QCoreApplication::applicationDirPath() + "/style.qss",
        QCoreApplication::applicationDirPath() + "/../../style.qss",
        "style.qss"
    };
    for (const auto &path : paths) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            app.setStyleSheet(f.readAll());
            f.close();
            return;
        }
    }
    if (globalLogger) globalLogger->log("No style.qss found, using defaults");
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    qRegisterMetaType<MacroObservation>("MacroObservation");

    globalLogger = new Logger("macro_indicators.log");
    globalLogger->log("--- Macro Indicators Service Active");

    loadStyleSheet(app);

    // Read API key from environment
    const char* apiKeyEnv = std::getenv("FRED_API");
    std::string apiKey = apiKeyEnv ? apiKeyEnv : "";
    if (apiKey.empty()) {
        globalLogger->log("ERROR: FRED_API environment variable not set");
        return -1;
    }

    int appPort = 3002;  // Dapr sidecar expects app on this port

    FredClient fred(apiKey);
    DaprMacroStore macroStore;  // localhost:3502, store "macrostore"

    // Chart window
    MacroChartWindow chartWin;

    // Wire FredClient observations to Dapr persistence + chart
    QObject::connect(&fred, &FredClient::observationReady,
                     &macroStore, &DaprMacroStore::saveObservation);
    QObject::connect(&fred, &FredClient::observationReady,
                     &chartWin, &MacroChartWindow::onObservation);

    QObject::connect(&fred, &FredClient::fetchError,
                     [](QString seriesId, QString err) {
        if (globalLogger) {
            globalLogger->log("FRED error [" + seriesId.toStdString()
                             + "]: " + err.toStdString());
        }
    });

    // ── HTTP server for Dapr cron binding callbacks ──
    MiniHttpServer server;

    // Dapr cron bindings POST to /cron/<binding-name>
    server.route("/cron/cpi", MiniHttpServer::POST, [&fred]() {
        if (globalLogger) globalLogger->log("Cron triggered: CPI (CPIAUCSL)");
        fred.fetchIndicator("CPIAUCSL", 3, "lin");
        return QJsonObject{{"status", "ok"}, {"series", "CPIAUCSL"}};
    });

    server.route("/cron/walcl", MiniHttpServer::POST, [&fred]() {
        if (globalLogger) globalLogger->log("Cron triggered: WALCL (Fed Balance Sheet)");
        fred.fetchIndicator("WALCL", 3, "lin");
        return QJsonObject{{"status", "ok"}, {"series", "WALCL"}};
    });

    server.route("/cron/umcsent", MiniHttpServer::POST, [&fred]() {
        if (globalLogger) globalLogger->log("Cron triggered: UMCSENT (Consumer Sentiment)");
        fred.fetchIndicator("UMCSENT", 3, "lin");
        return QJsonObject{{"status", "ok"}, {"series", "UMCSENT"}};
    });

    // Health check for Dapr
    server.route("/health", MiniHttpServer::GET, []() {
        return QJsonObject{{"status", "healthy"}};
    });

    // Manual trigger: fetch all indicators
    server.route("/fetch-all", MiniHttpServer::POST, [&fred]() {
        if (globalLogger) globalLogger->log("Manual fetch-all triggered");
        fred.fetchAll();
        return QJsonObject{{"status", "ok"}, {"series", "all"}};
    });

    if (!server.startListening(appPort)) {
        globalLogger->log("ERROR: Failed to start HTTP server on port "
                         + std::to_string(appPort));
        return -1;
    }

    // ── Init / Backfill logic ──
    // On first run (store empty), fetch historical data:
    //   CPI:    previous year + current year to today
    //   WALCL:  previous year + current year to today
    //   UMCSENT: previous year + current year to today
    // On subsequent runs, just fetch latest observations.
    QTimer::singleShot(2000, [&fred, &macroStore]() {
        if (globalLogger) globalLogger->log("Checking stores for init/backfill...");

        QDate today = QDate::currentDate();
        QString todayStr = today.toString(Qt::ISODate);

        // --- CPI: previous year + current year ---
        macroStore.checkStoreEmpty("CPIAUCSL", [&fred, &macroStore, today, todayStr](bool isEmpty) {
            if (isEmpty) {
                QString startDate = QDate(today.year() - 1, 1, 1).toString(Qt::ISODate);
                if (globalLogger)
                    globalLogger->log("CPI store empty -> backfill from "
                                     + startDate.toStdString() + " to " + todayStr.toStdString());
                fred.fetchIndicatorRange("CPIAUCSL", startDate, todayStr, "lin");
                macroStore.markInitialized("CPIAUCSL");
            } else {
                if (globalLogger)
                    globalLogger->log("CPI store initialized -> fetching latest");
                fred.fetchIndicator("CPIAUCSL", 3, "lin");
            }
        });

        // --- WALCL: previous year + current year ---
        macroStore.checkStoreEmpty("WALCL", [&fred, &macroStore, today, todayStr](bool isEmpty) {
            if (isEmpty) {
                QString startDate = QDate(today.year() - 1, 1, 1).toString(Qt::ISODate);
                if (globalLogger)
                    globalLogger->log("WALCL store empty -> backfill from "
                                     + startDate.toStdString() + " to " + todayStr.toStdString());
                fred.fetchIndicatorRange("WALCL", startDate, todayStr, "lin");
                macroStore.markInitialized("WALCL");
            } else {
                if (globalLogger)
                    globalLogger->log("WALCL store initialized -> fetching latest");
                fred.fetchIndicator("WALCL", 3, "lin");
            }
        });

        // --- UMCSENT: previous year + current year ---
        macroStore.checkStoreEmpty("UMCSENT", [&fred, &macroStore, today, todayStr](bool isEmpty) {
            if (isEmpty) {
                QString startDate = QDate(today.year() - 1, 1, 1).toString(Qt::ISODate);
                if (globalLogger)
                    globalLogger->log("UMCSENT store empty -> backfill from "
                                     + startDate.toStdString() + " to " + todayStr.toStdString());
                fred.fetchIndicatorRange("UMCSENT", startDate, todayStr, "lin");
                macroStore.markInitialized("UMCSENT");
            } else {
                if (globalLogger)
                    globalLogger->log("UMCSENT store initialized -> fetching latest");
                fred.fetchIndicator("UMCSENT", 3, "lin");
            }
        });
    });

    chartWin.show();
    globalLogger->log("UMacro Indicators Service running on port" + std::to_string(appPort) + " Endpoints: /cron/cpi, /cron/walcl, /cron/umcsent, /fetch-all ");

    return app.exec();
}
