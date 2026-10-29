#pragma once

#include <QWidget>

class QLabel;
class QSlider;
class QStackedLayout;
class QToolButton;

namespace fp {

class Player;
class VideoWidget;

// A viewer with its transport: picture, play/pause, scrub bar and timecode.
// Keys while it has focus: Space/K/L play and pause, Left/Right step a frame, Home/End jump.
class ViewerPanel : public QWidget {
    Q_OBJECT
public:
    explicit ViewerPanel(const QString &emptyText, QWidget *parent = nullptr);

    bool openMedia(const QString &path, QString *error);
    void closeMedia();
    Player *player() const { return m_player; }

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void showPosition(double seconds);

    Player *m_player;
    VideoWidget *m_video;
    QLabel *m_message;
    QStackedLayout *m_stack;
    QToolButton *m_play;
    QSlider *m_slider;
    QLabel *m_time;
    QString m_emptyText;
};

} // namespace fp
