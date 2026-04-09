#pragma once
#ifndef MACRO_TYPES_H
#define MACRO_TYPES_H

#include <QDateTime>
#include <QString>
#include <QVector>
#include <QMetaType>

/// Identifies which macro indicator this observation belongs to
enum class MacroIndicator {
    CPI,                // CPIAUCSL — Consumer Price Index (Inflation)
    FederalFundsRate,   // WALCL — Federal Reserve Balance Sheet (Weekly)
    ConsumerSentiment   // UMCSENT — U. of Michigan Consumer Sentiment
};

/// A single FRED API observation data point
struct MacroObservation {
    MacroIndicator indicator;
    QString seriesId;       // "CPIAUCSL", "WALCL", "UMCSENT"
    QString date;           // "2026-03-01"
    double  value;          // the observation value
    QDateTime fetchedAt;    // when we fetched it
};

/// Schedule metadata for each indicator
struct MacroSchedule {
    MacroIndicator indicator;
    QString seriesId;
    QString description;
    QString cronExpression;     // Dapr cron expression
    QString fredUnits;          // "lin" for level, "pc1" for % change
    int     observationLimit;   // how many recent observations to fetch
};

Q_DECLARE_METATYPE(MacroObservation)

#endif // MACRO_TYPES_H
