#ifndef AVATARCROPPER_H
#define AVATARCROPPER_H

#include <QPixmap>
#include <QImage>
#include <QWidget>
#include <QHash>
#include <QByteArray>

class AvatarCropper {
public:
    /// 居中裁剪为正方形并缩放到 targetSize
    static QPixmap cropAndScale(const QImage &source, int targetSize = 200);

    /// 打开文件对话框选择图片，裁剪后返回；取消返回 null pixmap
    static QPixmap selectAndCrop(QWidget *parent, int targetSize = 200);

    /// 将 QPixmap 编码为 PNG 格式的 QByteArray
    static QByteArray toPngBytes(const QPixmap &pixmap);

    /// 从原始字节生成圆形头像 QPixmap（带缓存）
    static QPixmap roundAvatar(const QByteArray &data, int size);

    /// 生成默认头像（昵称首字 + 彩色圆形背景）（带缓存）
    static QPixmap defaultAvatar(const QString &name, int size);

    /// 清除头像缓存
    static void clearCache();

private:
    /// 圆形头像缓存: key = "size:dataHash"
    static QHash<QString, QPixmap> &roundCache();
    /// 默认头像缓存: key = "size:name"
    static QHash<QString, QPixmap> &defaultCache();
};

#endif // AVATARCROPPER_H
