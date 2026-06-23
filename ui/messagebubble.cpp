#include "messagebubble.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QFont>

// ==================== MessageBubble ====================

MessageBubble::MessageBubble(const QString &text, const QString &senderName,
                             const QString &timeStr, bool isMine, QWidget *parent)
    : QWidget(parent)
{
    setupUI(text, senderName, timeStr, isMine);
}

void MessageBubble::setupUI(const QString &text, const QString &senderName,
                             const QString &timeStr, bool isMine)
{
    QHBoxLayout *outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(10, 4, 10, 4);

    QWidget *bubbleContainer = new QWidget;
    bubbleContainer->setMaximumWidth(450);

    QVBoxLayout *bubbleLayout = new QVBoxLayout(bubbleContainer);
    bubbleLayout->setContentsMargins(0, 0, 0, 0);
    bubbleLayout->setSpacing(2);

    // 发送者名称
    QLabel *nameLabel = new QLabel(senderName);
    nameLabel->setStyleSheet("font-size: 11px; color: #999; border: none; background: transparent;");
    bubbleLayout->addWidget(nameLabel);

    // 消息气泡
    QLabel *msgLabel = new QLabel(text);
    msgLabel->setWordWrap(true);
    msgLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    msgLabel->setContentsMargins(12, 8, 12, 8);

    if (isMine) {
        msgLabel->setStyleSheet(
            "background-color: #95ec69; color: #000; border-radius: 8px; "
            "font-size: 14px; padding: 8px 12px; border: none;"
        );
    } else {
        msgLabel->setStyleSheet(
            "background-color: white; color: #333; border-radius: 8px; "
            "font-size: 14px; padding: 8px 12px; border: 1px solid #e0e0e0;"
        );
    }
    bubbleLayout->addWidget(msgLabel);

    // 时间
    QLabel *timeLabel = new QLabel(timeStr);
    timeLabel->setStyleSheet("font-size: 10px; color: #bbb; border: none; background: transparent;");
    bubbleLayout->addWidget(timeLabel);

    if (isMine) {
        outerLayout->addStretch();
        nameLabel->setAlignment(Qt::AlignRight);
        timeLabel->setAlignment(Qt::AlignRight);
        outerLayout->addWidget(bubbleContainer);
    } else {
        outerLayout->addWidget(bubbleContainer);
        outerLayout->addStretch();
    }
}

// ==================== FileMessageCard ====================

FileMessageCard::FileMessageCard(const QString &fileName, qint64 fileSize, bool isMine,
                                 const QString &senderName, const QString &timeStr,
                                 QWidget *parent)
    : QWidget(parent)
{
    setupUI(fileName, fileSize, isMine, senderName, timeStr);
}

void FileMessageCard::setupUI(const QString &fileName, qint64 fileSize, bool isMine,
                               const QString &senderName, const QString &timeStr)
{
    QHBoxLayout *outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(10, 4, 10, 4);

    QWidget *card = new QWidget;
    card->setMaximumWidth(350);
    card->setStyleSheet(
        "background-color: white; border: 1px solid #e0e0e0; border-radius: 8px;"
    );

    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(12, 8, 12, 8);
    cardLayout->setSpacing(6);

    // 发送者
    QLabel *nameLabel = new QLabel(senderName + "  " + timeStr);
    nameLabel->setStyleSheet("font-size: 11px; color: #999; border: none;");
    cardLayout->addWidget(nameLabel);

    // 文件图标和名称
    QHBoxLayout *fileRow = new QHBoxLayout;
    QLabel *iconLabel = new QLabel(QStringLiteral("\u25A0"));
    iconLabel->setStyleSheet("font-size: 24px; border: none;");
    fileRow->addWidget(iconLabel);

    QVBoxLayout *fileInfoLayout = new QVBoxLayout;
    m_fileNameLabel = new QLabel(fileName);
    m_fileNameLabel->setStyleSheet("font-size: 13px; color: #333; font-weight: bold; border: none;");
    m_fileNameLabel->setWordWrap(true);
    fileInfoLayout->addWidget(m_fileNameLabel);

    m_fileSizeLabel = new QLabel(formatSize(fileSize));
    m_fileSizeLabel->setStyleSheet("font-size: 11px; color: #999; border: none;");
    fileInfoLayout->addWidget(m_fileSizeLabel);
    fileRow->addLayout(fileInfoLayout);
    fileRow->addStretch();
    cardLayout->addLayout(fileRow);

    // 进度条
    m_progressBar = new QProgressBar;
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(100);
    m_progressBar->setValue(0);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setTextVisible(false);
    m_progressBar->setStyleSheet(
        "QProgressBar { border: none; background: #e0e0e0; border-radius: 3px; }"
        "QProgressBar::chunk { background: #07c160; border-radius: 3px; }"
    );
    cardLayout->addWidget(m_progressBar);

    // 状态和按钮
    QHBoxLayout *bottomRow = new QHBoxLayout;
    m_statusLabel = new QLabel("");
    m_statusLabel->setStyleSheet("font-size: 11px; color: #999; border: none;");
    bottomRow->addWidget(m_statusLabel);
    bottomRow->addStretch();

    m_acceptBtn = new QPushButton("接收");
    m_acceptBtn->setFixedSize(50, 26);
    m_acceptBtn->setStyleSheet(
        "background: #07c160; color: white; border: none; border-radius: 4px; font-size: 12px;"
    );
    m_acceptBtn->setVisible(!isMine);
    connect(m_acceptBtn, &QPushButton::clicked, this, &FileMessageCard::acceptClicked);
    bottomRow->addWidget(m_acceptBtn);

    m_rejectBtn = new QPushButton("拒绝");
    m_rejectBtn->setFixedSize(50, 26);
    m_rejectBtn->setStyleSheet(
        "background: #f44336; color: white; border: none; border-radius: 4px; font-size: 12px;"
    );
    m_rejectBtn->setVisible(!isMine);
    connect(m_rejectBtn, &QPushButton::clicked, this, &FileMessageCard::rejectClicked);
    bottomRow->addWidget(m_rejectBtn);

    m_openBtn = new QPushButton("打开文件");
    m_openBtn->setFixedSize(65, 26);
    m_openBtn->setStyleSheet(
        "background: #1976D2; color: white; border: none; border-radius: 4px; font-size: 12px;"
    );
    m_openBtn->setVisible(false);
    connect(m_openBtn, &QPushButton::clicked, this, &FileMessageCard::openClicked);
    bottomRow->addWidget(m_openBtn);

    cardLayout->addLayout(bottomRow);

    if (isMine) {
        outerLayout->addStretch();
        outerLayout->addWidget(card);
    } else {
        outerLayout->addWidget(card);
        outerLayout->addStretch();
    }
}

void FileMessageCard::setProgress(qint64 received, qint64 total)
{
    if (total > 0) {
        int pct = static_cast<int>(received * 100 / total);
        m_progressBar->setValue(pct);
        m_statusLabel->setText(QString("传输中 %1 / %2").arg(formatSize(received), formatSize(total)));
    }
}

void FileMessageCard::setState(State state, const QString &info)
{
    switch (state) {
    case Pending:
        m_statusLabel->setText("等待接收...");
        break;
    case Transferring:
        m_statusLabel->setText("传输中...");
        break;
    case Completed:
        m_statusLabel->setText("已完成");
        m_statusLabel->setStyleSheet("font-size: 11px; color: #07c160; border: none;");
        m_progressBar->setValue(100);
        m_acceptBtn->hide();
        m_rejectBtn->hide();
        m_openBtn->show();
        break;
    case Rejected:
        m_statusLabel->setText("已拒绝: " + info);
        m_statusLabel->setStyleSheet("font-size: 11px; color: #f44336; border: none;");
        m_acceptBtn->hide();
        m_rejectBtn->hide();
        break;
    case Error:
        m_statusLabel->setText("错误: " + info);
        m_statusLabel->setStyleSheet("font-size: 11px; color: #f44336; border: none;");
        m_acceptBtn->hide();
        m_rejectBtn->hide();
        break;
    }
}

QString FileMessageCard::formatSize(qint64 bytes)
{
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1024LL * 1024 * 1024) return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
}
