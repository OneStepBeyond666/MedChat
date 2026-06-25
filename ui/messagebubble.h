#ifndef MESSAGEBUBBLE_H
#define MESSAGEBUBBLE_H

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QPoint>

// 文本消息气泡
class MessageBubble : public QWidget
{
    Q_OBJECT
public:
    // isMine: 是否为自己发送的消息 (右侧绿色气泡)
    explicit MessageBubble(const QString &text, const QString &senderName,
                           const QString &timeStr, bool isMine, QWidget *parent = nullptr);

    void setMsgId(qint64 id) { m_msgId = id; }
    void setTimestamp(qint64 ts) { m_timestamp = ts; }
    qint64 msgId() const { return m_msgId; }
    bool isRecalled() const { return m_isRecalled; }
    void setRecalled(bool recalled, bool isMine);

signals:
    void deleteRequested(qint64 msgId);
    void recallRequested(qint64 msgId, qint64 timestamp);
    void forwardRequested(int msgType, const QString &content, const QString &fileId);

private slots:
    void showContextMenu(const QPoint &pos);
    void onCopyClicked();
    void onEnlargeClicked();
    void onDeleteClicked();
    void onRecallClicked();
    void onForwardClicked();

private:
    void setupUI(const QString &text, const QString &senderName,
                 const QString &timeStr, bool isMine);

    QString m_text;
    QString m_senderName;
    qint64 m_msgId = 0;
    qint64 m_timestamp = 0;
    bool m_isMine = false;
    bool m_isRecalled = false;
    QLabel *m_msgLabel = nullptr;
};

// 文件传输消息卡片
class FileMessageCard : public QWidget
{
    Q_OBJECT
public:
    enum State { Pending, Transferring, Completed, Rejected, Error };

    explicit FileMessageCard(const QString &fileName, qint64 fileSize, bool isMine,
                             const QString &senderName, const QString &timeStr,
                             QWidget *parent = nullptr);

    void setProgress(qint64 received, qint64 total);
    void setState(State state, const QString &info = "");
    QString fileId; // 用于外部关联

signals:
    void acceptClicked();
    void rejectClicked();
    void openClicked();
    void forwardRequested(int msgType, const QString &content, const QString &fileId);

private slots:
    void showContextMenu(const QPoint &pos);

private:
    void setupUI(const QString &fileName, qint64 fileSize, bool isMine,
                 const QString &senderName, const QString &timeStr);
    static QString formatSize(qint64 bytes);

    QLabel *m_fileNameLabel;
    QLabel *m_fileSizeLabel;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    QPushButton *m_acceptBtn;
    QPushButton *m_rejectBtn;
    QPushButton *m_openBtn;
    State m_state = Pending;
};

#endif // MESSAGEBUBBLE_H
