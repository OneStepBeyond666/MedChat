#include "messagebubble.h"
#include "client/localdb.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QFont>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QUrl>
#include <QDialog>
#include <QTextEdit>
#include <QFrame>
#include <QDateTime>
#include <QFile>

// ==================== MessageBubble ====================

MessageBubble::MessageBubble(const QString &text, const QString &senderName,
                             const QString &timeStr, bool isMine, QWidget *parent)
    : QWidget(parent), m_text(text), m_senderName(senderName), m_isMine(isMine)
{
    setupUI(text, senderName, timeStr, isMine);
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &MessageBubble::showContextMenu);
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
    m_msgLabel = new QLabel(text);
    m_msgLabel->setWordWrap(true);
    m_msgLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_msgLabel->setContentsMargins(12, 8, 12, 8);

    if (isMine) {
        m_msgLabel->setStyleSheet(
            "background-color: #95ec69; color: #000; border-radius: 8px; "
            "font-size: 14px; padding: 8px 12px; border: none;"
        );
    } else {
        m_msgLabel->setStyleSheet(
            "background-color: white; color: #333; border-radius: 8px; "
            "font-size: 14px; padding: 8px 12px; border: 1px solid #e0e0e0;"
        );
    }
    bubbleLayout->addWidget(m_msgLabel);

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

void MessageBubble::showContextMenu(const QPoint &pos)
{
    // 已撤回消息不显示菜单
    if (m_isRecalled) return;

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: white; border: 1px solid #e0e0e0; border-radius: 4px; padding: 4px 0; min-width: 120px; }"
        "QMenu::item { padding: 8px 24px; font-size: 13px; color: #333; }"
        "QMenu::item:selected { background-color: #f5f5f5; }"
        "QMenu::item:disabled { color: #999; }"
        "QMenu::separator { height: 1px; background: #e0e0e0; margin: 4px 8px; }"
    );

    QAction *copyAction = menu.addAction(QStringLiteral("复制"));
    connect(copyAction, &QAction::triggered, this, &MessageBubble::onCopyClicked);

    menu.addSeparator();

    QAction *enlargeAction = menu.addAction(QStringLiteral("放大阅读"));
    connect(enlargeAction, &QAction::triggered, this, &MessageBubble::onEnlargeClicked);

    menu.addSeparator();

    QAction *forwardAction = menu.addAction(QStringLiteral("转发"));
    connect(forwardAction, &QAction::triggered, this, &MessageBubble::onForwardClicked);

    // 撤回：仅自己发送的消息 && 2分钟内
    qint64 timeDiff = QDateTime::currentMSecsSinceEpoch() - m_timestamp;
    bool canRecall = m_isMine && (timeDiff <= 120000);

    if (canRecall) {
        menu.addSeparator();
        QAction *recallAction = menu.addAction(QStringLiteral("撤回"));
        connect(recallAction, &QAction::triggered, this, &MessageBubble::onRecallClicked);
    }

    if (m_msgId > 0) {
        menu.addSeparator();
        QAction *deleteAction = menu.addAction(QStringLiteral("删除"));
        connect(deleteAction, &QAction::triggered, this, &MessageBubble::onDeleteClicked);
    }

    menu.exec(mapToGlobal(pos));
}

void MessageBubble::onCopyClicked()
{
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(m_text);
}

void MessageBubble::onEnlargeClicked()
{
    QDialog *dlg = new QDialog(this, Qt::Window);
    dlg->setWindowTitle(QStringLiteral("消息内容"));
    dlg->setMinimumSize(500, 400);
    dlg->resize(600, 500);

    QVBoxLayout *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(12);

    // 头部：发送者 + 时间
    QString timeStr = QDateTime::fromMSecsSinceEpoch(m_timestamp).toString("yyyy-MM-dd HH:mm:ss");
    QLabel *headerLabel = new QLabel(m_senderName + "  " + timeStr);
    headerLabel->setStyleSheet("font-size: 12px; color: #999;");
    layout->addWidget(headerLabel);

    // 分隔线
    QFrame *line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("color: #e0e0e0;");
    layout->addWidget(line);

    // 正文：大字体只读 QTextEdit
    QTextEdit *textEdit = new QTextEdit();
    textEdit->setReadOnly(true);
    textEdit->setPlainText(m_text);
    textEdit->setStyleSheet("font-size: 18px; border: none; color: #333;");
    layout->addWidget(textEdit);

    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void MessageBubble::onDeleteClicked()
{
    if (m_msgId > 0)
        emit deleteRequested(m_msgId);
}

void MessageBubble::onRecallClicked()
{
    if (m_msgId > 0 && !m_isRecalled)
        emit recallRequested(m_msgId, m_timestamp);
}

void MessageBubble::onForwardClicked()
{
    emit forwardRequested(0, m_text, QString());
}

void MessageBubble::setRecalled(bool recalled, bool isMine)
{
    m_isRecalled = recalled;
    if (recalled && m_msgLabel) {
        m_text = isMine
            ? QStringLiteral("[你撤回了一条消息]")
            : QStringLiteral("[对方撤回了一条消息]");
        m_msgLabel->setText(m_text);
        m_msgLabel->setStyleSheet(
            "color: #999999; font-style: italic; font-size: 12px; "
            "padding: 4px 8px; background: transparent; border: none;"
        );
    }
}

// ==================== FileMessageCard ====================

FileMessageCard::FileMessageCard(const QString &fileName, qint64 fileSize, bool isMine,
                                 const QString &senderName, const QString &timeStr,
                                 bool isOffline, int expireDays,
                                 QWidget *parent)
    : QWidget(parent), m_isOffline(isOffline), m_expireDays(expireDays)
{
    setupUI(fileName, fileSize, isMine, senderName, timeStr);
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &FileMessageCard::showContextMenu);
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

    // 离线文件：显示过期天数
    if (m_isOffline && m_expireDays >= 0) {
        QLabel *expireLabel = new QLabel(QString("还有 %1 天过期").arg(m_expireDays));
        expireLabel->setStyleSheet("font-size: 11px; color: #f57c00; border: none;");
        fileInfoLayout->addWidget(expireLabel);
    }
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
    m_state = state;
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

void FileMessageCard::showContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: white; border: 1px solid #e0e0e0; border-radius: 4px; padding: 4px 0; min-width: 120px; }"
        "QMenu::item { padding: 8px 24px; font-size: 13px; color: #333; }"
        "QMenu::item:selected { background-color: #f5f5f5; }"
        "QMenu::item:disabled { color: #999; }"
        "QMenu::separator { height: 1px; background: #e0e0e0; margin: 4px 8px; }"
    );

    // 打开文件（仅已完成状态可用）
    QAction *openAction = menu.addAction(QStringLiteral("打开"));
    openAction->setEnabled(m_state == Completed);
    connect(openAction, &QAction::triggered, this, &FileMessageCard::openClicked);

    // 转发（仅已完成状态可用）
    menu.addSeparator();
    QAction *forwardAction = menu.addAction(QStringLiteral("转发"));
    forwardAction->setEnabled(m_state == Completed);
    connect(forwardAction, &QAction::triggered, this, [this]() {
        if (m_state == Completed)
            emit forwardRequested(1, m_fileNameLabel->text(), fileId);
    });

    // 复制文件到剪贴板（仅已完成且文件存在时可用）
    menu.addSeparator();
    FileRecord fileRec = LocalDB::instance().getFileRecord(fileId);
    QAction *copyFileAction = menu.addAction(QStringLiteral("复制"));
    bool fileExists = (m_state == Completed && !fileRec.savePath.isEmpty() && QFile::exists(fileRec.savePath));
    copyFileAction->setEnabled(fileExists);
    connect(copyFileAction, &QAction::triggered, this, [fileRec]() {
        QMimeData *mime = new QMimeData;
        QList<QUrl> urls;
        urls << QUrl::fromLocalFile(fileRec.savePath);
        mime->setUrls(urls);
        QApplication::clipboard()->setMimeData(mime);
    });

    // 复制文件名
    QAction *copyAction = menu.addAction(QStringLiteral("复制文件名"));
    connect(copyAction, &QAction::triggered, this, [this]() {
        QApplication::clipboard()->setText(m_fileNameLabel->text());
    });

    // 删除（仅当有 msgId 时可用）
    if (m_msgId > 0) {
        menu.addSeparator();
        QAction *deleteAction = menu.addAction(QStringLiteral("删除"));
        connect(deleteAction, &QAction::triggered, this, [this]() {
            emit deleteRequested(m_msgId);
        });
    }

    menu.exec(mapToGlobal(pos));
}
