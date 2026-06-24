#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QMap>
#include <QList>
#include <QDateTime>
#include <functional>

class MessageBubble;
class FileMessageCard;
struct StoredMessage;

struct ChatMessage {
    enum Type { Text, File };
    Type type;
    QString sender;
    QString text;
    qint64 timestamp;
    bool isMine;
    // 文件相关
    QString fileId;
    QString fileName;
    qint64 fileSize = 0;
};

class ChatWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ChatWidget(QWidget *parent = nullptr);

    void setChatPartner(const QString &username, const QString &role, const QString &displayName);
    void clearChat();
    QString currentPartner() const { return m_partner; }

    void addTextMessage(const QString &sender, const QString &text, qint64 timestamp, bool isMine);
    void addFileMessage(const QString &sender, const QString &fileName, qint64 fileSize,
                        const QString &fileId, bool isMine);
    void updateFileProgress(const QString &fileId, qint64 received, qint64 total);
    void setFileCompleted(const QString &fileId);
    void setFileRejected(const QString &fileId, const QString &reason);
    void setFileError(const QString &fileId, const QString &error);

    void loadHistoryMessages(const QVector<StoredMessage> &messages,
                             const QString &partnerNick,
                             std::function<bool(const QString&)> transferActiveCheck = nullptr);

signals:
    void sendMessage(const QString &to, const QString &text);
    void sendFileRequest(const QString &to);
    void sendFileWithPath(const QString &to, const QString &filePath);
    void fileAccepted(const QString &fileId);
    void fileRejected(const QString &fileId);
    void fileOpenRequested(const QString &fileId);

private slots:
    void onSendClicked();
    void onPlusClicked();
    void onPreviewClose();
    void onFileAcceptClicked();
    void onFileRejectClicked();
    void onFileOpenClicked();

private:
    void setupUI();
    void applyStyles();
    void scrollToBottom();
    QString formatTime(qint64 msecsSinceEpoch);
    void showFilePreview(const QString &filePath);
    void clearFilePreview();
    QString humanFileSize(qint64 bytes) const;

    // 拖拽
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    QLabel *m_partnerLabel;
    QLabel *m_partnerRoleLabel;
    QWidget *m_messageContainer;
    QVBoxLayout *m_messageLayout;
    QScrollArea *m_scrollArea;

    // 输入区
    QTextEdit *m_inputEdit;
    QToolButton *m_plusBtn;
    QPushButton *m_sendBtn;

    // 文件预览区
    QWidget *m_previewArea;
    QLabel *m_previewNameLabel;
    QLabel *m_previewSizeLabel;
    QString m_pendingFilePath;

    QString m_partner;
    QString m_partnerRole;

    // 存储消息历史
    QList<ChatMessage> m_messages;
    // fileId -> FileMessageCard
    QMap<QString, FileMessageCard*> m_fileCards;
};

#endif // CHATWIDGET_H
