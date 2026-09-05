// appearancesettings.cpp —— 见 appearancesettings.h
#include "appearancesettings.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QTimer>
#include <QRandomGenerator>

QString AppearanceSettings::skinDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/MSM_Remote/skin");
}

// 扫描文件夹内图片文件（与本地端 SettingsController::scanImageFiles 对齐）
static QStringList scanImageFiles(const QString &folder)
{
    QStringList out;
    if (folder.isEmpty() || !QFile::exists(folder))
        return out;
    static const QStringList exts = {QStringLiteral("*.png"), QStringLiteral("*.jpg"),
                                     QStringLiteral("*.jpeg"), QStringLiteral("*.bmp"),
                                     QStringLiteral("*.webp"), QStringLiteral("*.gif")};
    QDir dir(folder);
    for (const QString &e : exts) {
        const QStringList files = dir.entryList(QStringList() << e, QDir::Files, QDir::Name);
        for (const QString &f : files)
            out.append(QDir::cleanPath(dir.absoluteFilePath(f)));
    }
    return out;
}

AppearanceSettings::AppearanceSettings(QObject *parent) : QObject(parent)
{
    QSettings s;
    m_bgEnabled = s.value(QStringLiteral("app/bgEnabled"), false).toBool();
    m_bgImagePath = s.value(QStringLiteral("app/bgImage")).toString();
    m_bgmEnabled = s.value(QStringLiteral("app/bgmEnabled"), false).toBool();
    m_bgmPath = s.value(QStringLiteral("app/bgmPath")).toString();
    m_bgmVolume = s.value(QStringLiteral("app/bgmVolume"), 0.5).toDouble();

    // 壁纸轮换设置（键名与本地端对齐：bg/folder、bg/list、bg/mode、bg/interval、bg/intervalUnit、bg/index）
    m_bgImageFolder = s.value(QStringLiteral("bg/folder")).toString();
    m_bgImageList = s.value(QStringLiteral("bg/list")).toStringList();
    m_bgImageMode = s.value(QStringLiteral("bg/mode"), QStringLiteral("sequential")).toString();
    m_bgImageInterval = s.value(QStringLiteral("bg/interval"), 10).toInt();
    m_bgImageIntervalUnit = s.value(QStringLiteral("bg/intervalUnit"), QStringLiteral("min")).toString();
    m_bgImageIndex = s.value(QStringLiteral("bg/index"), 0).toInt();
    if (m_bgImageList.isEmpty() && !m_bgImageFolder.isEmpty())
        m_bgImageList = scanImageFiles(m_bgImageFolder);   // 升级兼容：旧版只存了文件夹

    m_bgScrimOpacity = s.value(QStringLiteral("bg/scrimOpacity"), 0.45).toDouble();
    m_uiTransparency = s.value(QStringLiteral("ui/transparency"), 1.0).toDouble();
    m_bgLayerOpacity = s.value(QStringLiteral("ui/bgLayerOpacity"), 1.0).toDouble();

    // 壁纸轮换定时器
    m_rotTimer = new QTimer(this);
    m_rotTimer->setSingleShot(false);
    connect(m_rotTimer, &QTimer::timeout, this, &AppearanceSettings::rotateBg);
    refreshRotation();
}

void AppearanceSettings::setBgEnabled(bool v)
{
    if (m_bgEnabled != v) {
        m_bgEnabled = v;
        QSettings().setValue(QStringLiteral("app/bgEnabled"), v);
        emit bgEnabledChanged();
    }
    refreshRotation();
}

void AppearanceSettings::setBgmEnabled(bool v)
{
    if (m_bgmEnabled != v) { m_bgmEnabled = v; QSettings().setValue("app/bgmEnabled", v); emit bgmEnabledChanged(); }
}
void AppearanceSettings::setBgmVolume(double v)
{
    const double n = qBound(0.0, v, 1.0);
    if (m_bgmVolume != n) { m_bgmVolume = n; QSettings().setValue("app/bgmVolume", n); emit bgmVolumeChanged(); }
}

// ---------- 壁纸轮换 ----------

void AppearanceSettings::setBgImageFolder(const QString &v)
{
    const QString folder = stripFileUrl(v);
    if (m_bgImageFolder != folder) {
        m_bgImageFolder = folder;
        QSettings().setValue(QStringLiteral("bg/folder"), folder);
        emit bgImageFolderChanged();
    }
    const QStringList list = scanImageFiles(folder);
    if (m_bgImageList != list) {
        m_bgImageList = list;
        emit bgImageListChanged();
    }
    m_bgImageIndex = 0;
    emit bgImageIndexChanged();
    if (!list.isEmpty())
        setBgEnabled(true);
    refreshRotation();
}

void AppearanceSettings::addBgImages(const QStringList &files)
{
    bool changed = false;
    for (const QString &f : files) {
        const QString p = stripFileUrl(f);
        if (p.isEmpty() || !QFile::exists(p))
            continue;
        const QString clean = QDir::cleanPath(p);
        if (!m_bgImageList.contains(clean)) {
            m_bgImageList.append(clean);
            changed = true;
        }
    }
    if (changed) {
        emit bgImageListChanged();
        setBgEnabled(true);
    }
    refreshRotation();
}

void AppearanceSettings::removeBgImage(int index)
{
    if (index < 0 || index >= m_bgImageList.size())
        return;
    m_bgImageList.removeAt(index);
    if (m_bgImageIndex >= m_bgImageList.size())
        m_bgImageIndex = qMax(0, m_bgImageList.size() - 1);
    emit bgImageListChanged();
    emit bgImageIndexChanged();
    if (m_bgImageList.isEmpty())
        setBgEnabled(false);
    refreshRotation();
}

void AppearanceSettings::moveBgImage(int from, int to)
{
    if (from < 0 || from >= m_bgImageList.size() || to < 0 || to >= m_bgImageList.size() || from == to)
        return;
    // 移动后保持“当前播放项”指向同一张图
    const QString cur = (m_bgImageIndex >= 0 && m_bgImageIndex < m_bgImageList.size())
                            ? m_bgImageList.at(m_bgImageIndex) : QString();
    m_bgImageList.move(from, to);
    if (!cur.isEmpty())
        m_bgImageIndex = m_bgImageList.indexOf(cur);
    emit bgImageListChanged();
    emit bgImageIndexChanged();
}

void AppearanceSettings::clearBgImages()
{
    m_bgImageFolder.clear();
    QSettings().setValue(QStringLiteral("bg/folder"), m_bgImageFolder);
    m_bgImageList.clear();
    m_bgImageIndex = 0;
    emit bgImageFolderChanged();
    emit bgImageListChanged();
    emit bgImageIndexChanged();
    setBgEnabled(false);
    refreshRotation();
}

void AppearanceSettings::setBgImageMode(const QString &v)
{
    if (m_bgImageMode != v) { m_bgImageMode = v; QSettings().setValue("bg/mode", v); emit bgImageModeChanged(); }
    refreshRotation();
}
void AppearanceSettings::setBgImageInterval(int v)
{
    const int n = qBound(1, v, 1000000);
    if (m_bgImageInterval != n) { m_bgImageInterval = n; QSettings().setValue("bg/interval", n); emit bgImageIntervalChanged(); }
    refreshRotation();
}
void AppearanceSettings::setBgImageIntervalUnit(const QString &v)
{
    if (m_bgImageIntervalUnit != v) { m_bgImageIntervalUnit = v; QSettings().setValue("bg/intervalUnit", v); emit bgImageIntervalUnitChanged(); }
    refreshRotation();
}
void AppearanceSettings::setBgImageIndex(int v)
{
    if (m_bgImageList.isEmpty()) { m_bgImageIndex = 0; return; }
    const int n = qBound(0, v, m_bgImageList.size() - 1);
    if (m_bgImageIndex != n) { m_bgImageIndex = n; emit bgImageIndexChanged(); }
    emit bgImagePathChanged();   // 通知 BackgroundLayer 刷新当前壁纸
}

void AppearanceSettings::setBgScrimOpacity(double v)
{
    const double n = qBound(0.0, v, 1.0);
    if (m_bgScrimOpacity != n) { m_bgScrimOpacity = n; QSettings().setValue("bg/scrimOpacity", n); emit bgScrimOpacityChanged(); }
}
void AppearanceSettings::setUiTransparency(double v)
{
    const double n = qBound(0.0, v, 1.0);
    if (m_uiTransparency != n) { m_uiTransparency = n; QSettings().setValue("ui/transparency", n); emit uiTransparencyChanged(); }
}
void AppearanceSettings::setBgLayerOpacity(double v)
{
    const double n = qBound(0.0, v, 1.0);
    if (m_bgLayerOpacity != n) { m_bgLayerOpacity = n; QSettings().setValue("ui/bgLayerOpacity", n); emit bgLayerOpacityChanged(); }
}

void AppearanceSettings::refreshRotation()
{
    if (!m_rotTimer)
        return;
    if (!m_bgEnabled || m_bgImageList.size() < 2 || m_bgImageMode == QStringLiteral("off")) {
        m_rotTimer->stop();
        return;
    }
    // 间隔（数值 × 单位）转毫秒：sec/min/hour/day
    const auto unitToMs = [](const QString &u) -> qint64 {
        if (u == QStringLiteral("sec"))  return 1000;
        if (u == QStringLiteral("hour")) return 3600000;
        if (u == QStringLiteral("day"))  return 86400000;
        return 60000;   // "min" 及未知单位
    };
    const qint64 ms = qMax<qint64>(1, m_bgImageInterval) * unitToMs(m_bgImageIntervalUnit);
    m_rotTimer->setInterval(static_cast<int>(qMin<qint64>(ms, Q_INT64_C(2147483647))));
    m_rotTimer->start();
}

void AppearanceSettings::rotateBg()
{
    if (!m_bgEnabled || m_bgImageList.size() < 2)
        return;
    const int cur = qBound(0, m_bgImageIndex, m_bgImageList.size() - 1);
    int next = cur;
    if (m_bgImageMode == QStringLiteral("random")) {
        if (QRandomGenerator::system())
            do { next = QRandomGenerator::system()->bounded(m_bgImageList.size()); }
            while (next == cur && m_bgImageList.size() > 1);
    } else {
        next = (cur + 1) % m_bgImageList.size();
    }
    m_bgImageIndex = next;
    emit bgImageIndexChanged();
    emit bgImagePathChanged();   // 通知 BackgroundLayer 刷新当前壁纸
}

// ---------- 单图（皮肤）导入 ----------

static QString copySkinFile(const QString &srcFile, const QString &dstDir, const QString &base)
{
    const QString src = QUrl(srcFile).isLocalFile() ? QUrl(srcFile).toLocalFile() : srcFile;
    if (src.isEmpty() || !QFile::exists(src))
        return QString();
    QDir().mkpath(dstDir);
    // 保留原扩展名，便于 Image / QtMultimedia 识别格式
    const QString ext = QFileInfo(src).suffix();
    const QString dstName = base + (ext.isEmpty() ? QString() : (QStringLiteral(".") + ext));
    const QString dst = dstDir + QLatin1Char('/') + dstName;
    if (QFile::exists(dst))
        QFile::remove(dst);
    return QFile::copy(src, dst) ? dstName : QString();
}

bool AppearanceSettings::importSkinImage(const QString &src)
{
    const QString name = copySkinFile(src, bgImageDir(), QStringLiteral("bgimage"));
    if (name.isEmpty())
        return false;
    m_bgImagePath = name;
    QSettings().setValue(QStringLiteral("app/bgImage"), name);
    setBgEnabled(true);   // 选图成功后自动开启背景图，选完即生效
    emit bgImagePathChanged();
    return true;
}
bool AppearanceSettings::importSkinMusic(const QString &src)
{
    const QString name = copySkinFile(src, bgmDir(), QStringLiteral("bgm"));
    if (name.isEmpty())
        return false;
    m_bgmPath = name;
    QSettings().setValue(QStringLiteral("app/bgmPath"), name);
    emit bgmPathChanged();
    return true;
}
void AppearanceSettings::clearSkinImage()
{
    const QString f = bgImageDir() + QStringLiteral("/") + m_bgImagePath;
    const QString fallback = bgImageDir() + QStringLiteral("/bgimage");
    if (QFile::exists(f)) QFile::remove(f);
    if (QFile::exists(fallback)) QFile::remove(fallback);
    m_bgImagePath.clear();
    QSettings().remove(QStringLiteral("app/bgImage"));
    emit bgImagePathChanged();
}
void AppearanceSettings::clearSkinMusic()
{
    const QString f = bgmDir() + QStringLiteral("/") + m_bgmPath;
    const QString fallback = bgmDir() + QStringLiteral("/bgm");
    if (QFile::exists(f)) QFile::remove(f);
    if (QFile::exists(fallback)) QFile::remove(fallback);
    m_bgmPath.clear();
    QSettings().remove(QStringLiteral("app/bgmPath"));
    emit bgmPathChanged();
}

QString AppearanceSettings::bgImageFullPath() const
{
    if (!m_bgEnabled)
        return QString();
    // 优先级：播放列表当前项 > 旧版单图
    if (!m_bgImageList.isEmpty()) {
        const int idx = qBound(0, m_bgImageIndex, m_bgImageList.size() - 1);
        return QUrl::fromLocalFile(m_bgImageList.at(idx)).toString();
    }
    if (!m_bgImagePath.isEmpty()) {
        const QString full = bgImageDir() + QLatin1Char('/') + m_bgImagePath;
        if (QFile::exists(full))
            return QUrl::fromLocalFile(full).toString();
    }
    return QString();
}
