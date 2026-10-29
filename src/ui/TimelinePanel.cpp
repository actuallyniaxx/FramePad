#include "ui/TimelinePanel.h"

#include "project/Project.h"

#include <QListWidget>
#include <QVBoxLayout>

namespace fp {

TimelinePanel::TimelinePanel(Project *project, QWidget *parent)
    : QWidget(parent)
    , m_project(project)
    , m_list(new QListWidget(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_list);

    connect(m_project, &Project::tracksChanged, this, &TimelinePanel::refresh);
    refresh();
}

int TimelinePanel::currentTrack() const
{
    return m_list->currentRow();
}

void TimelinePanel::refresh()
{
    const int row = m_list->currentRow();
    m_list->clear();
    for (const Track &t : m_project->tracks()) {
        const QString kind = t.kind == TrackKind::Video ? tr("Video") : tr("Audio");
        m_list->addItem(QStringLiteral("%1  ·  %2").arg(t.name, kind));
    }
    m_list->setCurrentRow(qMin(row, m_list->count() - 1));
}

} // namespace fp
