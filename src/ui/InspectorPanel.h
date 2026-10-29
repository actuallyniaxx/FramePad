#pragma once

#include "project/Project.h"

#include <QWidget>

class QComboBox;
class QSpinBox;

namespace fp {

// Phase 1: shows the project settings. Later it shows whatever is selected.
class InspectorPanel : public QWidget {
    Q_OBJECT
public:
    explicit InspectorPanel(Project *project, QWidget *parent = nullptr);

signals:
    // Emitted when the user edits a field; MainWindow turns it into an undo command.
    void settingsEdited(const fp::ProjectSettings &settings);

private:
    void refresh();
    void emitEdited();

    Project *m_project;
    QSpinBox *m_width;
    QSpinBox *m_height;
    QComboBox *m_fps;
    QComboBox *m_sampleRate;
};

} // namespace fp
