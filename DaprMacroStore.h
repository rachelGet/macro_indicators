#pragma once
#ifndef DAPR_MACRO_STORE_H
#define DAPR_MACRO_STORE_H

#include "MacroTypes.h"

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMap>
#include <QEventLoop>
#include <string>
#include <functional>

/**
 * DaprMacroStore routes FRED observations to the correct Dapr state store
 * based on the indicator's seriesId. Each indicator has its own table:
 *
 *   CPIAUCSL → cpi-store   → SQL table "cpi"
 *   WALCL    → walcl-store → SQL table "walcl"
 *   UMCSENT  → umcsent-store → SQL table "umcsent"
 *
 * Key format: "<date>" e.g. "2026-03-01"
 */
class DaprMacroStore : public QObject
{
    Q_OBJECT

public:
    explicit DaprMacroStore(const std::string& daprHost = "127.0.0.1",
                            int daprPort = 3502,
                            QObject* parent = nullptr);
    ~DaprMacroStore();

    /// Check if a store has been initialized (has the _init metadata key).
    /// Calls callback(true) if empty (no _init key), callback(false) if already initialized.
    void checkStoreEmpty(const QString& seriesId,
                         std::function<void(bool isEmpty)> callback);

    /// Mark a store as initialized by writing the _init metadata key.
    void markInitialized(const QString& seriesId);

public slots:
    void saveObservation(MacroObservation obs);

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    QNetworkAccessManager* m_manager;
    QString m_baseUrl;  // "http://127.0.0.1:3502/v1.0/state/"

    // seriesId → Dapr store name
    QMap<QString, QString> m_storeMap;
};

#endif // DAPR_MACRO_STORE_H
