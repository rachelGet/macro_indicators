#include "DaprMacroStore.h"
#include "logs.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>

extern Logger* globalLogger;

DaprMacroStore::DaprMacroStore(const std::string& daprHost,
                               int daprPort,
                               QObject* parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
    m_baseUrl = QString("http://%1:%2/v1.0/state/")
                    .arg(QString::fromStdString(daprHost))
                    .arg(daprPort);

    // Map each FRED series to its dedicated Dapr state store
    m_storeMap["CPIAUCSL"] = "cpi-store";
    m_storeMap["WALCL"]    = "walcl-store";
    m_storeMap["UMCSENT"]  = "umcsent-store";

    connect(m_manager, &QNetworkAccessManager::finished,
            this, &DaprMacroStore::onReplyFinished);

    if (globalLogger) {
        globalLogger->log("DaprMacroStore initialized -> " + m_baseUrl.toStdString());
    }
}

DaprMacroStore::~DaprMacroStore() {}

void DaprMacroStore::checkStoreEmpty(const QString& seriesId,
                                      std::function<void(bool isEmpty)> callback) {
    QString storeName = m_storeMap.value(seriesId, "");
    if (storeName.isEmpty()) {
        if (globalLogger)
            globalLogger->log("checkStoreEmpty: unknown seriesId " + seriesId.toStdString());
        callback(true);
        return;
    }

    // Try to GET the _init metadata key from the store
    QString url = m_baseUrl + storeName + "/_init";
    QNetworkRequest request{QUrl(url)};

    QNetworkAccessManager* tempMgr = new QNetworkAccessManager(this);
    QNetworkReply* reply = tempMgr->get(request);

    connect(reply, &QNetworkReply::finished, this, [=]() {
        bool isEmpty = (reply->error() != QNetworkReply::NoError
                        || reply->readAll().isEmpty());

        if (globalLogger) {
            globalLogger->log("checkStoreEmpty [" + storeName.toStdString() + "]: "
                             + (isEmpty ? "EMPTY (needs init)" : "already initialized"));
        }

        callback(isEmpty);
        reply->deleteLater();
        tempMgr->deleteLater();
    });
}

void DaprMacroStore::markInitialized(const QString& seriesId) {
    QString storeName = m_storeMap.value(seriesId, "");
    if (storeName.isEmpty()) return;

    QJsonObject val;
    val["initialized"] = true;
    val["timestamp"]   = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    QJsonObject stateEntry;
    stateEntry["key"]   = "_init";
    stateEntry["value"] = val;

    QJsonArray stateArray;
    stateArray.append(stateEntry);

    QJsonDocument doc(stateArray);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);

    QString url = m_baseUrl + storeName;
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    m_manager->post(request, payload);

    if (globalLogger) {
        globalLogger->log("markInitialized [" + storeName.toStdString() + "]: _init key written");
    }
}

void DaprMacroStore::saveObservation(MacroObservation obs) {
    // Route to the correct store based on seriesId
    QString storeName = m_storeMap.value(obs.seriesId, "");
    if (storeName.isEmpty()) {
        if (globalLogger) {
            globalLogger->log("DaprMacroStore: unknown seriesId " + obs.seriesId.toStdString());
        }
        return;
    }

    // Key is just the date (unique within each table)
    QString key = obs.date;

    QJsonObject val;
    val["seriesId"]  = obs.seriesId;
    val["date"]      = obs.date;
    val["value"]     = obs.value;
    val["fetchedAt"] = obs.fetchedAt.toString(Qt::ISODate);

    switch (obs.indicator) {
        case MacroIndicator::CPI:
            val["indicator"] = "CPI (Inflation)"; break;
        case MacroIndicator::FederalFundsRate:
            val["indicator"] = "Fed Balance Sheet (WALCL)"; break;
        case MacroIndicator::ConsumerSentiment:
            val["indicator"] = "Consumer Sentiment (UMich)"; break;
    }

    QJsonObject stateEntry;
    stateEntry["key"]   = key;
    stateEntry["value"] = val;

    QJsonArray stateArray;
    stateArray.append(stateEntry);

    QJsonDocument doc(stateArray);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);

    QString url = m_baseUrl + storeName;
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    m_manager->post(request, payload);

    if (globalLogger) {
        globalLogger->log("Dapr save [" + storeName.toStdString() + "]: "
                         + key.toStdString() + " = " + std::to_string(obs.value));
    }
}

void DaprMacroStore::onReplyFinished(QNetworkReply* reply) {
    if (reply->error() != QNetworkReply::NoError) {
        if (globalLogger) {
            globalLogger->log("Dapr macro store error: "
                              + reply->errorString().toStdString());
        }
    }
    reply->deleteLater();
}
