#include "ui/MediaPanel.h"

#include "media/MediaReader.h"
#include "media/Previews.h"
#include "project/Project.h"

#include <QBoxLayout>
#include <QDir>
#include <QDragEnterEvent>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMimeData>
#include <QShortcut>
#include <QStackedLayout>
#include <QToolButton>
#include <QtConcurrent>

namespace fp {

namespace {

const QSize ThumbnailSize(160, 90);
constexpr int PathRole = Qt::UserRole;

struct PreviewResult {
    QString path;
    QImage image;
    QString detail;
};

QString formatLength(double seconds)
{
    const long long total = (long long)(seconds + 0.5);
    if (total >= 3600)
        return QStringLiteral("%1:%2:%3")
            .arg(total / 3600)
            .arg(total / 60 % 60, 2, 10, QLatin1Char('0'))
            .arg(total % 60, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(total / 60).arg(total % 60, 2, 10, QLatin1Char('0'));
}

// Runs on a worker thread: QImage and FFmpeg only, no QPixmap or widgets.
PreviewResult buildPreview(const QString &path)
{
    PreviewResult result{path, {}, {}};
    MediaInfo info;
    {
        MediaReader reader;
        if (!reader.open(path, &result.detail))
            return result;
        info = reader.info();
    }
    const QString length = formatLength(info.duration);
    if (info.hasVideo) {
        result.image = makeThumbnail(path, ThumbnailSize);
        result.detail = MediaPanel::tr("%1×%2 · %3 fps · %4")
                            .arg(info.width)
                            .arg(info.height)
                            .arg(QLocale().toString(info.fps, 'g', 4), length);
    } else {
        result.image = renderWaveform(computePeaks(path, ThumbnailSize.width()), ThumbnailSize, QColor(0x5c, 0xb8, 0x5c));
        result.detail = MediaPanel::tr("Audio · %1 Hz · %2 ch · %3").arg(info.sampleRate).arg(info.channels).arg(length);
    }
    return result;
}

QPixmap placeholder()
{
    QPixmap pixmap(ThumbnailSize);
    pixmap.fill(QColor(0x22, 0x22, 0x22));
    return pixmap;
}

} // namespace

MediaPanel::MediaPanel(Project *project, QWidget *parent)
    : QWidget(parent)
    , m_project(project)
    , m_list(new QListWidget(this))
{
    m_list->setViewMode(QListView::IconMode);
    m_list->setIconSize(ThumbnailSize);
    m_list->setGridSize(ThumbnailSize + QSize(24, 56));
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setWordWrap(true);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // Icon mode turns on internal drag and drop; files dropped from Explorer are handled by the panel instead.
    m_list->setDragDropMode(QAbstractItemView::NoDragDrop);
    m_list->setAcceptDrops(false);
    m_list->viewport()->setAcceptDrops(false);
    setAcceptDrops(true);

    auto *hint = new QLabel(tr("Drag video or audio files here,\nor use File → Import media."), this);
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);

    auto *importButton = new QToolButton(this);
    importButton->setText(tr("Import..."));
    importButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(importButton, &QToolButton::clicked, this, &MediaPanel::importRequested);

    auto *content = new QWidget(this);
    m_stack = new QStackedLayout(content);
    m_stack->addWidget(hint);
    m_stack->addWidget(m_list);

    auto *toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(4, 4, 4, 0);
    toolbar->addWidget(importButton);
    toolbar->addStretch();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(toolbar);
    layout->addWidget(content, 1);

    connect(m_list, &QListWidget::itemActivated, this,
            [this](QListWidgetItem *item) { emit openRequested(item->data(PathRole).toString()); });
    auto *remove = new QShortcut(QKeySequence::Delete, m_list);
    remove->setContext(Qt::WidgetWithChildrenShortcut);
    connect(remove, &QShortcut::activated, this, [this] {
        const QList<int> rows = selectedRows();
        if (!rows.isEmpty())
            emit removeRequested(rows);
    });
    connect(m_project, &Project::mediaChanged, this, &MediaPanel::rebuild);
    rebuild();
}

QList<int> MediaPanel::selectedRows() const
{
    QList<int> rows;
    for (QListWidgetItem *item : m_list->selectedItems())
        rows.append(m_list->row(item));
    return rows;
}

void MediaPanel::rebuild()
{
    m_list->clear();
    for (const MediaItem &media : m_project->media()) {
        auto *item = new QListWidgetItem(m_list);
        item->setData(PathRole, media.path);
        item->setToolTip(QDir::toNativeSeparators(media.path));
        showPreview(item);
    }
    m_stack->setCurrentIndex(m_project->media().isEmpty() ? 0 : 1);
}

void MediaPanel::showPreview(QListWidgetItem *item)
{
    const QString path = item->data(PathRole).toString();
    const QString name = QFileInfo(path).fileName();
    if (!QFileInfo::exists(path)) {
        item->setIcon(placeholder());
        item->setText(name + QLatin1Char('\n') + tr("File not found"));
        return;
    }
    const auto cached = m_previews.constFind(path);
    if (cached == m_previews.cend()) {
        item->setIcon(placeholder());
        item->setText(name);
        requestPreview(path);
        return;
    }
    item->setIcon(cached->pixmap.isNull() ? placeholder() : cached->pixmap);
    item->setText(name + QLatin1Char('\n') + cached->detail);
}

void MediaPanel::requestPreview(const QString &path)
{
    if (m_pending.contains(path))
        return;
    m_pending.insert(path);

    auto *watcher = new QFutureWatcher<PreviewResult>(this);
    connect(watcher, &QFutureWatcher<PreviewResult>::finished, this, [this, watcher] {
        const PreviewResult result = watcher->result();
        watcher->deleteLater();
        m_pending.remove(result.path);
        m_previews.insert(result.path, {QPixmap::fromImage(result.image), result.detail});
        for (int i = 0; i < m_list->count(); ++i) {
            if (m_list->item(i)->data(PathRole).toString() == result.path)
                showPreview(m_list->item(i));
        }
    });
    watcher->setFuture(QtConcurrent::run(buildPreview, path));
}

void MediaPanel::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MediaPanel::dropEvent(QDropEvent *event)
{
    QStringList paths;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile())
            paths.append(url.toLocalFile());
    }
    if (paths.isEmpty())
        return;
    event->acceptProposedAction();
    emit filesDropped(paths);
}

} // namespace fp
