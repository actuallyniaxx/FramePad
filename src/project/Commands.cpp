#include "project/Commands.h"

#include <QCoreApplication>

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

} // namespace fp
