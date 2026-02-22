#ifndef NET_RETRY_POLICY_H
#define NET_RETRY_POLICY_H

#include <QNetworkReply>
#include <QtGlobal>

// 网络重试策略：
// - 仅用于“尚未收到首包”的请求失败，避免流式输出中途重试造成重复内容。
// - 通过 HTTP 状态码与网络错误类型判断是否属于短暂故障。
inline bool isRetryableHttpStatus(int httpCode)
{
    return httpCode == 408 || httpCode == 409 || httpCode == 425 || httpCode == 429 || (httpCode >= 500 && httpCode <= 599);
}

inline bool isRetryableNetworkError(QNetworkReply::NetworkError error)
{
    switch (error)
    {
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::TimeoutError:
    case QNetworkReply::RemoteHostClosedError:
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::ServiceUnavailableError:
    case QNetworkReply::ProxyConnectionRefusedError:
    case QNetworkReply::ProxyConnectionClosedError:
    case QNetworkReply::ProxyNotFoundError:
    case QNetworkReply::ProxyTimeoutError:
        return true;
    default:
        break;
    }
    return false;
}

inline bool shouldRetryNetRequest(bool canceled,
                                  bool firstByteSeen,
                                  QNetworkReply::NetworkError error,
                                  int httpCode,
                                  int retriesUsed,
                                  int maxRetries)
{
    if (canceled) return false;
    if (firstByteSeen) return false;
    if (maxRetries <= 0) return false;
    if (retriesUsed >= maxRetries) return false;
    if (httpCode >= 400) return isRetryableHttpStatus(httpCode);
    return isRetryableNetworkError(error);
}

inline int nextRetryBackoffMs(int retriesUsed, int baseDelayMs, int maxDelayMs)
{
    const int safeBase = qMax(1, baseDelayMs);
    const int safeMax = qMax(safeBase, maxDelayMs);
    const int shift = qBound(0, retriesUsed, 10);
    const qint64 delay = qMin<qint64>(qint64(safeBase) << shift, safeMax);
    return int(delay);
}

#endif // NET_RETRY_POLICY_H
