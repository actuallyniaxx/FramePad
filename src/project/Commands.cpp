#include "project/Commands.h"

#include <QCoreApplication>

#include <algorithm>

namespace fp {

AddTrackCommand::AddTrackCommand(Project *project, int index, Track track)
    : m_project(project)
    , m_index(index)
    , m_track(std::move(track))
{
    setText(QCoreApplication::translate("fp::Commands", "Add track"));
}

void AddTrackCommand::redo() { m_project->insertTrack(m_index, m_track); }
void AddTrackCommand::undo() { m_project->takeTrack(m_index); }

RemoveTrackCommand::RemoveTrackCommand(Project *project, int index)
    : m_project(project)
    , m_index(index)
    , m_track(project->tracks().at(index))
{
    setText(QCoreApplication::translate("fp::Commands", "Remove track"));
}

void RemoveTrackCommand::redo() { m_project->takeTrack(m_index); }
void RemoveTrackCommand::undo() { m_project->insertTrack(m_index, m_track); }

SetSettingsCommand::SetSettingsCommand(Project *project, ProjectSettings settings)
    : m_project(project)
    , m_before(project->settings())
    , m_after(settings)
{
    setText(QCoreApplication::translate("fp::Commands", "Change project settings"));
}

void SetSettingsCommand::redo() { m_project->setSettings(m_after); }
void SetSettingsCommand::undo() { m_project->setSettings(m_before); }

ImportMediaCommand::ImportMediaCommand(Project *project, QList<MediaItem> items)
    : m_project(project)
    , m_first(int(project->media().size()))
    , m_items(std::move(items))
{
    setText(QCoreApplication::translate("fp::Commands", "Import media"));
}

void ImportMediaCommand::redo()
{
    for (int i = 0; i < m_items.size(); ++i)
        m_project->insertMedia(m_first + i, m_items.at(i));
}

void ImportMediaCommand::undo()
{
    for (int i = int(m_items.size()) - 1; i >= 0; --i)
        m_project->takeMedia(m_first + i);
}

RemoveMediaCommand::RemoveMediaCommand(Project *project, QList<int> rows)
    : m_project(project)
    , m_rows(std::move(rows))
{
    std::sort(m_rows.begin(), m_rows.end());
    m_rows.erase(std::unique(m_rows.begin(), m_rows.end()), m_rows.end());
    for (int row : m_rows)
        m_items.append(project->media().at(row));
    setText(QCoreApplication::translate("fp::Commands", "Remove media"));
}

void RemoveMediaCommand::redo()
{
    // Back to front, so the earlier rows keep their indexes.
    for (int i = int(m_rows.size()) - 1; i >= 0; --i)
        m_project->takeMedia(m_rows.at(i));
}

void RemoveMediaCommand::undo()
{
    for (int i = 0; i < m_rows.size(); ++i)
        m_project->insertMedia(m_rows.at(i), m_items.at(i));
}

} // namespace fp
