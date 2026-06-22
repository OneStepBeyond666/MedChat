#ifndef MESSAGEBUBBLE_H
#define MESSAGEBUBBLE_H

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>

// 文本消息气泡
class MessageBubble : public QWidget
{
    Q_OBJECT
public:
    // isMine: 是否为自己发送的消息 (右侧绿色气泡)
    explicit MessageBubble(const QString &text, const QString &senderName,
                           const QString &timeStr, bool isMine, QWidget *parent = nullptr);

private:
    void setupUI(const QString &text, const QString &senderName,
                 const QString &timeStr, bool isMine);
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
};

#endif // MESSAGEBUBBLE_H
