#pragma once

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>
#include <QUrl>

namespace arachnel::core {

inline QString joinRelayUrl(const QString& base, const QString& path)
{
    const QString trimmedBase = base.trimmed();
    if (trimmedBase.isEmpty())
        return {};
    return trimmedBase.endsWith(QLatin1Char('/')) ? trimmedBase.left(trimmedBase.size() - 1) + path
                                                  : trimmedBase + path;
}

inline QString describeRelayError(QNetworkReply* reply)
{
    if (!reply)
        return QCoreApplication::translate("Core", "Can't reach the relay");

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString detail =
        QJsonDocument::fromJson(reply->readAll()).object().value(QStringLiteral("detail")).toString();
    if (detail == QLatin1String("invalid or expired code") || status == 400)
        return QCoreApplication::translate("Core", "Invalid or expired code");
    if (status == 429)
        return QCoreApplication::translate("Core", "Too many tries, wait a bit");
    if (status == 403)
        return QCoreApplication::translate("Core", "Relay rejected the request");

    switch (reply->error()) {
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::UnknownNetworkError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::RemoteHostClosedError:
        return QCoreApplication::translate("Core", "Can't reach the relay");
    case QNetworkReply::SslHandshakeFailedError:
        return QCoreApplication::translate("Core", "Relay TLS failed");
    case QNetworkReply::TimeoutError:
    case QNetworkReply::OperationCanceledError:
        return QCoreApplication::translate("Core", "Relay timed out");
    default:
        return QCoreApplication::translate("Core", "Relay request failed");
    }
}

inline QNetworkRequest makeRelayRequest(const QUrl& url, bool jsonBody)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Arachnel/0.1"));
    if (jsonBody)
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(15000);
    return request;
}

} // namespace arachnel::core
