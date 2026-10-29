#include "project/Project.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace fp {

namespace {

QString kindToString(TrackKind kind)
{
    return kind == TrackKind::Audio ? QStringLiteral("audio") : QStringLiteral("video");
}

TrackKind kindFromString(const QString &s)
{
    return s == QLatin1String("audio") ? TrackKind::Audio : TrackKind::Video;
}

} // namespace

Project::Project(QObject *parent)
    : QObject(parent)
{
    resetToDefault();
}

void Project::resetToDefault()
{
    m_settings = {};
    m_tracks = {
        {QStringLiteral("V1"), TrackKind::Video},
        {QStringLiteral("A1"), TrackKind::Audio},
    };
    m_media.clear();
    emit settingsChanged();
    emit tracksChanged();
    emit mediaChanged();
}

void Project::setSettings(const ProjectSettings &settings)
{
    if (m_settings == settings)
        return;
    m_settings = settings;
    emit settingsChanged();
}

void Project::insertTrack(int index, const Track &track)
{
    m_tracks.insert(qBound(0, index, int(m_tracks.size())), track);
    emit tracksChanged();
}

Track Project::takeTrack(int index)
{
    Track track = m_tracks.takeAt(index);
    emit tracksChanged();
    return track;
}

int Project::indexOfMedia(const QString &path) const
{
    for (int i = 0; i < m_media.size(); ++i) {
        if (QFileInfo(m_media.at(i).path) == QFileInfo(path))
            return i;
    }
    return -1;
}

void Project::insertMedia(int index, const MediaItem &item)
{
    m_media.insert(qBound(0, index, int(m_media.size())), item);
    emit mediaChanged();
}

MediaItem Project::takeMedia(int index)
{
    MediaItem item = m_media.takeAt(index);
    emit mediaChanged();
    return item;
}

QJsonObject Project::toJson(const QString &projectDir) const
{
    QJsonArray tracks;
    for (const Track &t : m_tracks) {
        tracks.append(QJsonObject{
            {"name", t.name},
            {"kind", kindToString(t.kind)},
            {"muted", t.muted},
            {"locked", t.locked},
        });
    }

    QJsonArray media;
    for (const MediaItem &m : m_media) {
        QJsonObject item{{"path", QDir::fromNativeSeparators(m.path)}};
        if (!projectDir.isEmpty())
            item["relativePath"] = QDir(projectDir).relativeFilePath(m.path);
        media.append(item);
    }

    return {
        {"formatVersion", FormatVersion},
        {"application", QStringLiteral("FramePad " FRAMEPAD_VERSION)},
        {"settings", QJsonObject{
            {"width", m_settings.width},
            {"height", m_settings.height},
            {"fps", m_settings.fps},
            {"sampleRate", m_settings.sampleRate},
        }},
        {"tracks", tracks},
        {"media", media},
    };
}

bool Project::fromJson(const QJsonObject &input, QString *error, const QString &projectDir)
{
    const int version = input.value("formatVersion").toInt(0);
    if (version < 1) {
        if (error)
            *error = tr("This is not a FramePad project.");
        return false;
    }
    if (version > FormatVersion) {
        if (error)
            *error = tr("This project was saved with a newer version of FramePad. Update FramePad to open it.");
        return false;
    }

    const QJsonObject json = migrate(input, version);

    const QJsonObject s = json.value("settings").toObject();
    ProjectSettings settings;
    settings.width = s.value("width").toInt(settings.width);
    settings.height = s.value("height").toInt(settings.height);
    settings.fps = s.value("fps").toDouble(settings.fps);
    settings.sampleRate = s.value("sampleRate").toInt(settings.sampleRate);

    QList<Track> tracks;
    for (const QJsonValue &v : json.value("tracks").toArray()) {
        const QJsonObject t = v.toObject();
        tracks.append(Track{
            t.value("name").toString(),
            kindFromString(t.value("kind").toString()),
            t.value("muted").toBool(),
            t.value("locked").toBool(),
        });
    }

    QList<MediaItem> media;
    for (const QJsonValue &v : json.value("media").toArray()) {
        const QJsonObject m = v.toObject();
        QString path = m.value("path").toString();
        const QString relative = m.value("relativePath").toString();
        // The absolute path wins; the relative one rescues projects whose folder was moved.
        if (!QFileInfo::exists(path) && !relative.isEmpty() && !projectDir.isEmpty()) {
            const QString moved = QDir::cleanPath(QDir(projectDir).absoluteFilePath(relative));
            if (QFileInfo::exists(moved))
                path = moved;
        }
        if (!path.isEmpty())
            media.append(MediaItem{path});
    }

    m_settings = settings;
    m_tracks = tracks;
    m_media = media;
    emit settingsChanged();
    emit tracksChanged();
    emit mediaChanged();
    return true;
}

QJsonObject Project::migrate(QJsonObject json, int fromVersion)
{
    // One step per format bump, applied in order.
    if (fromVersion < 2)
        json["media"] = QJsonArray(); // v1 had no media bin
    json["formatVersion"] = FormatVersion;
    return json;
}

bool Project::save(const QString &path, QString *error) const
{
    // QSaveFile writes to a temp file and renames, so a crash never leaves a half-written project.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(toJson(QFileInfo(path).absolutePath())).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

bool Project::load(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error)
            *error = tr("This is not a FramePad project.");
        return false;
    }
    return fromJson(doc.object(), error, QFileInfo(path).absolutePath());
}

} // namespace fp
