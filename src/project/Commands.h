#pragma once

#include "project/Project.h"

#include <QUndoCommand>

namespace fp {

class AddTrackCommand : public QUndoCommand {
public:
    AddTrackCommand(Project *project, int index, Track track);
    void redo() override;
    void undo() override;

private:
    Project *m_project;
    int m_index;
    Track m_track;
};

class RemoveTrackCommand : public QUndoCommand {
public:
    RemoveTrackCommand(Project *project, int index);
    void redo() override;
    void undo() override;

private:
    Project *m_project;
    int m_index;
    Track m_track;
};

class SetSettingsCommand : public QUndoCommand {
public:
    SetSettingsCommand(Project *project, ProjectSettings settings);
    void redo() override;
    void undo() override;

private:
    Project *m_project;
    ProjectSettings m_before;
    ProjectSettings m_after;
};

// Appends items to the media bin.
class ImportMediaCommand : public QUndoCommand {
public:
    ImportMediaCommand(Project *project, QList<MediaItem> items);
    void redo() override;
    void undo() override;

private:
    Project *m_project;
    int m_first;
    QList<MediaItem> m_items;
};

class RemoveMediaCommand : public QUndoCommand {
public:
    RemoveMediaCommand(Project *project, QList<int> rows);
    void redo() override;
    void undo() override;

private:
    Project *m_project;
    QList<int> m_rows; // ascending
    QList<MediaItem> m_items;
};

} // namespace fp
