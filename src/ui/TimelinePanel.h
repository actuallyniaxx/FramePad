#pragma once

#include <QWidget>

class QListWidget;

namespace fp {

class Project;

// Phase 1: just the track headers. Clips and the ruler arrive in phase 3.
class TimelinePanel : public QWidget {
    Q_OBJECT
public:
    explicit TimelinePanel(Project *project, QWidget *parent = nullptr);

    // -1 when no track is selected.
    int currentTrack() const;

private:
    void refresh();

    Project *m_project;
    QListWidget *m_list;
};

} // namespace fp
