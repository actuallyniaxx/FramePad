#include "ui/ViewerPanel.h"

#include "media/Player.h"
#include "media/Previews.h"
#include "ui/VideoWidget.h"

#include <QBoxLayout>
#include <QFileInfo>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QLabel>
#include <QSlider>
#include <QStackedLayout>
#include <QStyle>
#include <QToolButton>

namespace fp {

ViewerPanel::ViewerPanel(const QString &emptyText, QWidget *parent)
    : QWidget(parent)
    , m_player(new Player(this))
    , m_video(new VideoWidget(this))
    , m_message(new QLabel(emptyText, this))
    , m_play(new QToolButton(this))
    , m_slider(new QSlider(Qt::Horizontal, this))
    , m_time(new QLabel(this))
    , m_emptyText(emptyText)
{
    // The panel keeps keyboard focus so its shortcuts work; the transport controls never take it.
    setFocusPolicy(Qt::StrongFocus);
    m_play->setFocusPolicy(Qt::NoFocus);
    m_slider->setFocusPolicy(Qt::NoFocus);

    m_message->setAlignment(Qt::AlignCenter);
    m_message->setWordWrap(true);
    m_message->setMinimumSize(320, 180);
    m_message->setStyleSheet(QStringLiteral("background: #111; color: #888;"));

    auto *screen = new QWidget(this);
    m_stack = new QStackedLayout(screen);
    m_stack->addWidget(m_message);
    m_stack->addWidget(m_video);

    m_play->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_play->setToolTip(tr("Play / pause (Space)"));
    m_time->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    auto *transport = new QHBoxLayout;
    transport->setContentsMargins(4, 0, 4, 0);
    transport->addWidget(m_play);
    transport->addWidget(m_slider, 1);
    transport->addWidget(m_time);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 4);
    layout->addWidget(screen, 1);
    layout->addLayout(transport);

    connect(m_play, &QToolButton::clicked, m_player, &Player::togglePlay);
    // actionTriggered covers dragging and clicking the groove; setValue() from playback does not fire it.
    connect(m_slider, &QSlider::actionTriggered, this, [this] { m_player->seek(m_slider->sliderPosition() / 1000.0); });
    connect(m_player, &Player::frameReady, m_video, &VideoWidget::setFrame);
    connect(m_player, &Player::positionChanged, this, &ViewerPanel::showPosition);
    connect(m_player, &Player::playingChanged, this, [this](bool playing) {
        m_play->setIcon(style()->standardIcon(playing ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
    });

    closeMedia();
}

bool ViewerPanel::openMedia(const QString &path, QString *error)
{
    m_video->setFrame(nullptr);
    if (!m_player->open(path, error)) {
        closeMedia();
        return false;
    }
    const MediaInfo &info = m_player->info();
    if (info.hasVideo) {
        m_stack->setCurrentWidget(m_video);
    } else {
        m_message->setText(tr("%1\n(audio only)").arg(QFileInfo(path).fileName()));
        m_stack->setCurrentWidget(m_message);
    }
    m_slider->setRange(0, int(info.duration * 1000));
    m_slider->setEnabled(true);
    m_play->setEnabled(true);
    showPosition(0.0);
    setFocus();
    return true;
}

void ViewerPanel::closeMedia()
{
    m_player->close();
    m_video->setFrame(nullptr);
    m_message->setText(m_emptyText);
    m_stack->setCurrentWidget(m_message);
    m_slider->setRange(0, 0);
    m_slider->setEnabled(false);
    m_play->setEnabled(false);
    showPosition(0.0);
}

void ViewerPanel::showPosition(double seconds)
{
    const MediaInfo &info = m_player->info();
    const double fps = info.fps > 0.0 ? info.fps : 30.0;
    m_time->setText(QStringLiteral("%1 / %2").arg(formatTimecode(seconds, fps), formatTimecode(info.duration, fps)));
    if (!m_slider->isSliderDown())
        m_slider->setValue(int(seconds * 1000));
}

void ViewerPanel::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Space:
        m_player->togglePlay();
        break;
    case Qt::Key_K:
        m_player->pause();
        break;
    case Qt::Key_L:
        m_player->play();
        break;
    case Qt::Key_Left:
        m_player->stepFrames(-1);
        break;
    case Qt::Key_Right:
        m_player->stepFrames(1);
        break;
    case Qt::Key_Home:
        m_player->pause();
        m_player->seek(0.0);
        break;
    case Qt::Key_End:
        m_player->pause();
        m_player->seek(m_player->info().duration);
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    event->accept();
}

} // namespace fp
