#pragma once
#ifndef FRED_CLIENT_H
#define FRED_CLIENT_H

#include "MacroTypes.h"

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <string>

/**
 * FredClient fetches economic indicator data from the FRED API
 * (Federal Reserve Economic Data - St. Louis Fed).
 *
 * FRED API endpoint:
 *   GET https://api.stlouisfed.org/fred/series/observations
 *       ?series_id=CPIAUCSL&api_key=...&file_type=json
 *       &sort_order=desc&limit=5
 *
 * Three indicators:
 *   CPIAUCSL — CPI (Inflation), monthly, released ~10th-14th
 *   WALCL    — Fed Balance Sheet (proxy for Fed Funds policy), weekly Thursdays
 *   UMCSENT  — U. of Michigan Consumer Sentiment, monthly last Friday
 *
 * This client is triggered by Dapr cron bindings — each schedule invokes
 * fetchIndicator() for the appropriate series.
 */
class FredClient : public QObject
{
    Q_OBJECT

public:
    explicit FredClient(const std::string& apiKey,
                        QObject* parent = nullptr);
    ~FredClient();

    /// Fetch the latest observations for a specific FRED series
    void fetchIndicator(const QString& seriesId, int limit = 5,
                        const QString& units = "lin");

    /// Fetch observations within a date range (for backfill/init)
    void fetchIndicatorRange(const QString& seriesId,
                             const QString& startDate,
                             const QString& endDate,
                             const QString& units = "lin");

    /// Convenience: fetch all three indicators (latest only)
    void fetchAll();

signals:
    void observationReady(MacroObservation obs);
    void fetchError(QString seriesId, QString errorMsg);

private slots:
    void onNetworkReply(QNetworkReply* reply);

private:
    void parseObservations(const QByteArray& data, const QString& seriesId);
    QString buildUrl(const QString& seriesId, int limit,
                     const QString& units) const;
    QString buildRangeUrl(const QString& seriesId,
                          const QString& startDate,
                          const QString& endDate,
                          const QString& units) const;

    MacroIndicator indicatorFromSeriesId(const QString& seriesId) const;

    QNetworkAccessManager* m_manager;
    QString m_apiKey;
    static constexpr const char* kBaseUrl =
        "https://api.stlouisfed.org/fred/series/observations";
};

#endif // FRED_CLIENT_H
