#pragma once

#include <QMainWindow>

class QUndoStack;

namespace fp {

class Project;
class TimelinePanel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    // Used by main() for a project passed on the command line (double-click on a .fpproj).
    bool openProject(const QString &path);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void createPanels();
    void createMenus();
    void newProject();
    void openProjectDialog();
    bool save();
    bool saveAs();
    void addTrack(bool video);
    void removeTrack();
    void chooseLanguage(const QString &code);
    void about();

    // Asks to save unsaved changes. Returns false if the user cancelled.
    bool confirmDiscard();
    void updateTitle();

    Project *m_project;
    QUndoStack *m_undo;
    TimelinePanel *m_timeline = nullptr;
    QMenu *m_panelsMenu = nullptr;
    QString m_path; // empty until the project is saved for the first time
};

} // namespace fp
