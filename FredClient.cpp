#include "FredClient.h"
#include "logs.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>
#include <QDateTime>

extern Logger* globalLogger;

FredClient::FredClient(const std::string& apiKey, QObject* parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_apiKey(QString::fromStdString(apiKey))
{
    connect(m_manager, &QNetworkAccessManager::finished,
            this, &FredClient::onNetworkReply);

    if (globalLogger) {
        globalLogger->log("FredClient initialized with API key: "
                         + apiKey.substr(0, 8) + "...");
    }
}

FredClient::~FredClient() {}

QString FredClient::buildUrl(const QString& seriesId, int limit,
                              const QString& units) const {
    // FRED API: GET /fred/series/observations?series_id=X&api_key=Y&file_type=json
    return QString("%1?series_id=%2&api_key=%3&file_type=json"
                   "&sort_order=desc&limit=%4&units=%5")
        .arg(kBaseUrl)
        .arg(seriesId)
        .arg(m_apiKey)
        .arg(limit)
        .arg(units);
}

MacroIndicator FredClient::indicatorFromSeriesId(const QString& seriesId) const {
    if (seriesId == "CPIAUCSL") return MacroIndicator::CPI;
    if (seriesId == "WALCL")    return MacroIndicator::FederalFundsRate;
    if (seriesId == "UMCSENT")  return MacroIndicator::ConsumerSentiment;
    return MacroIndicator::CPI;  // fallback
}

void FredClient::fetchIndicator(const QString& seriesId, int limit,
                                 const QString& units) {
    QString url = buildUrl(seriesId, limit, units);

    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::User, QVariant(seriesId));

    if (globalLogger) {
        globalLogger->log("FRED fetch: " + seriesId.toStdString()
                         + " limit=" + std::to_string(limit)
                         + " units=" + units.toStdString());
    }

    m_manager->get(request);
}

QString FredClient::buildRangeUrl(const QString& seriesId,
                                   const QString& startDate,
                                   const QString& endDate,
                                   const QString& units) const {
    return QString("%1?series_id=%2&api_key=%3&file_type=json"
                   "&sort_order=asc&observation_start=%4&observation_end=%5&units=%6")
        .arg(kBaseUrl)
        .arg(seriesId)
        .arg(m_apiKey)
        .arg(startDate)
        .arg(endDate)
        .arg(units);
}

void FredClient::fetchIndicatorRange(const QString& seriesId,
                                      const QString& startDate,
                                      const QString& endDate,
                                      const QString& units) {
    QString url = buildRangeUrl(seriesId, startDate, endDate, units);

    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::User, QVariant(seriesId));

    if (globalLogger) {
        globalLogger->log("FRED range fetch: " + seriesId.toStdString()
                         + " from=" + startDate.toStdString()
                         + " to=" + endDate.toStdString()
                         + " units=" + units.toStdString());
    }

    m_manager->get(request);
}

void FredClient::fetchAll() {
    fetchIndicator("CPIAUCSL", 5, "lin");
    fetchIndicator("WALCL", 5, "lin");
    fetchIndicator("UMCSENT", 5, "lin");
}

void FredClient::onNetworkReply(QNetworkReply* reply) {
    QString seriesId = reply->request().attribute(QNetworkRequest::User).toString();

    if (reply->error() != QNetworkReply::NoError) {
        QString err = reply->errorString();
        if (globalLogger) {
            globalLogger->log("FRED request error [" + seriesId.toStdString()
                             + "]: " + err.toStdString());
        }
        emit fetchError(seriesId, err);
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    parseObservations(data, seriesId);
    reply->deleteLater();
}

void FredClient::parseObservations(const QByteArray& data, const QString& seriesId) {
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        if (globalLogger) {
            globalLogger->log("FRED parse error [" + seriesId.toStdString()
                             + "]: response is not a JSON object");
        }
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray observations = root["observations"].toArray();



    MacroIndicator indicator = indicatorFromSeriesId(seriesId);

    for (int i = 0; i < observations.size(); ++i) {
        QJsonObject obs = observations[i].toObject();

        QString date = obs["date"].toString();
        QString valueStr = obs["value"].toString();

        // FRED returns "." for missing/pending values
        if (valueStr == "." || valueStr.isEmpty()) continue;

        double value = valueStr.toDouble();

        MacroObservation macroObs;
        macroObs.indicator = indicator;
        macroObs.seriesId  = seriesId;
        macroObs.date      = date;
        macroObs.value     = value;
        macroObs.fetchedAt = QDateTime::currentDateTimeUtc();

        emit observationReady(macroObs);

        if (globalLogger) {
            globalLogger->log("FRED [" + seriesId.toStdString()
                             + "] " + date.toStdString()
                             + " = " + std::to_string(value));
        }
    }
}
