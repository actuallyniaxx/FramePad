#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QJsonObject>

namespace fp {

enum class TrackKind { Video, Audio };

struct Track {
    QString name;
    TrackKind kind = TrackKind::Video;
    bool muted = false;
    bool locked = false;

    bool operator==(const Track &) const = default;
};

struct ProjectSettings {
    int width = 1920;
    int height = 1080;
    double fps = 30.0;
    int sampleRate = 48000;

    bool operator==(const ProjectSettings &) const = default;
};

// The edit document. Every change goes through a QUndoCommand (Commands.h),
// which calls the mutators below; they emit signals so the UI can refresh.
class Project : public QObject {
    Q_OBJECT
public:
    // Bump when the file layout changes and add a step to migrate().
    static constexpr int FormatVersion = 1;
    static constexpr const char *FileSuffix = "fpproj";

    explicit Project(QObject *parent = nullptr);

    // A new project starts with one video and one audio track.
    void resetToDefault();

    const ProjectSettings &settings() const { return m_settings; }
    const QList<Track> &tracks() const { return m_tracks; }

    void setSettings(const ProjectSettings &settings);
    void insertTrack(int index, const Track &track);
    Track takeTrack(int index);

    QJsonObject toJson() const;
    // Returns false and fills error if the file is unreadable or from a newer FramePad.
    bool fromJson(const QJsonObject &json, QString *error);

    bool save(const QString &path, QString *error) const;
    bool load(const QString &path, QString *error);

signals:
    void settingsChanged();
    void tracksChanged();

private:
    static QJsonObject migrate(QJsonObject json, int fromVersion);

    ProjectSettings m_settings;
    QList<Track> m_tracks;
};

} // namespace fp
