#include "avatarcropper.h"
#include "common/constants.h"
#include <QFileDialog>
#include <QPainter>
#include <QPainterPath>
#include <QBuffer>
#include <QCryptographicHash>

// ============================================================
// 缓存基础设施
// ============================================================

QHash<QString, QPixmap> &AvatarCropper::roundCache()
{
    static QHash<QString, QPixmap> cache;
    return cache;
}

QHash<QString, QPixmap> &AvatarCropper::defaultCache()
{
    static QHash<QString, QPixmap> cache;
    return cache;
}

void AvatarCropper::clearCache()
{
    roundCache().clear();
    defaultCache().clear();
}

QPixmap AvatarCropper::cropAndScale(const QImage &source, int targetSize)
{
    if (source.isNull())
        return QPixmap();

    // 居中裁剪为正方形
    int side = qMin(source.width(), source.height());
    QRect cropRect((source.width() - side) / 2, (source.height() - side) / 2, side, side);
    QImage cropped = source.copy(cropRect);

    // 缩放到目标尺寸
    QImage scaled = cropped.scaled(targetSize, targetSize,
                                   Qt::KeepAspectRatioByExpanding,
                                   Qt::SmoothTransformation);
    return QPixmap::fromImage(scaled);
}

QPixmap AvatarCropper::selectAndCrop(QWidget *parent, int targetSize)
{
    QString filePath = QFileDialog::getOpenFileName(
        parent, "选择头像图片", QString(),
        "图片文件 (*.png *.jpg *.jpeg *.bmp);;所有文件 (*)");
    if (filePath.isEmpty())
        return QPixmap();

    QImage img(filePath);
    if (img.isNull())
        return QPixmap();

    return cropAndScale(img, targetSize);
}

QByteArray AvatarCropper::toPngBytes(const QPixmap &pixmap)
{
    if (pixmap.isNull())
        return QByteArray();

    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    pixmap.save(&buffer, "PNG");
    return ba;
}

QPixmap AvatarCropper::roundAvatar(const QByteArray &data, int size)
{
    if (data.isEmpty())
        return defaultAvatar("?", size);

    // 缓存键: size + 数据哈希
    QString key = QString::number(size) + ":"
                  + QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex().left(16));

    auto &cache = roundCache();
    auto it = cache.constFind(key);
    if (it != cache.constEnd())
        return *it;

    QPixmap src;
    src.loadFromData(data);

    if (src.isNull())
        return defaultAvatar("?", size);

    // 圆形直径比size小8px，留出透明边距，防止抗锯齿边缘被裁剪
    int circleDiameter = size - 8;
    int circleOffset = 4;  // 圆形左上角偏移4px，居中显示

    // 先缩放为正方形
    QPixmap scaled = src.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    // 居中裁剪
    int x = (scaled.width() - size) / 2;
    int y = (scaled.height() - size) / 2;
    QPixmap cropped = scaled.copy(x, y, size, size);

    // 绘制圆形（缩小版，居中）
    QPixmap result(size, size);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addEllipse(circleOffset, circleOffset, circleDiameter, circleDiameter);
    painter.setClipPath(path);
    // 将裁剪后的正方形pixmap绘制到result上，位置居中
    painter.drawPixmap(circleOffset, circleOffset, circleDiameter, circleDiameter, cropped);
    painter.end();

    cache.insert(key, result);
    return result;
}

QPixmap AvatarCropper::defaultAvatar(const QString &name, int size)
{
    // 缓存键: size + name
    QString key = QString::number(size) + ":" + name;
    auto &cache = defaultCache();
    auto it = cache.constFind(key);
    if (it != cache.constEnd())
        return *it;

    // 根据名称 hash 选择颜色
    static const QColor colors[] = {
        QColor("#1AAD19"), QColor("#4C84FF"), QColor("#FF6B6B"),
        QColor("#FFA940"), QColor("#9254DE"), QColor("#36CFC9"),
        QColor("#F759AB"), QColor("#597EF7"), QColor("#73D13D"),
        QColor("#FF7A45")
    };
    int colorIdx = 0;
    if (!name.isEmpty())
        colorIdx = qHash(name) % 10;

    // 圆形直径比size小8px，留出透明边距，防止抗锯齿边缘被裁剪
    int circleDiameter = size - 8;
    int circleOffset = 4;

    QPixmap result(size, size);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);

    // 画圆形背景（缩小版，居中）
    painter.setBrush(colors[colorIdx]);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(circleOffset, circleOffset, circleDiameter, circleDiameter);

    // 画首字（在圆形区域内居中）
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPixelSize(circleDiameter * 0.45);  // 基于圆形直径计算字体大小
    font.setBold(true);
    painter.setFont(font);
    QString ch = name.isEmpty() ? "?" : name.left(1);
    // 文字绘制区域微调：x偏移+1px修正中文字体左bearing导致的视觉偏左
    painter.drawText(QRect(circleOffset + 1, circleOffset, circleDiameter, circleDiameter), Qt::AlignCenter, ch);

    painter.end();

    cache.insert(key, result);
    return result;
}
