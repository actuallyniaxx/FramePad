#include "ui/MainWindow.h"

#include "project/Commands.h"
#include "project/Project.h"
#include "ui/InspectorPanel.h"
#include "ui/Language.h"
#include "ui/TimelinePanel.h"

#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QUndoStack>

namespace fp {

namespace {

QLabel *makeViewer(const QString &text, QWidget *parent)
{
    auto *viewer = new QLabel(text, parent);
    viewer->setAlignment(Qt::AlignCenter);
    viewer->setMinimumSize(320, 180);
    viewer->setStyleSheet(QStringLiteral("background: #111; color: #888;"));
    return viewer;
}

QString fileFilter()
{
    return MainWindow::tr("FramePad projects (*.%1)").arg(QLatin1String(Project::FileSuffix));
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_project(new Project(this))
    , m_undo(new QUndoStack(this))
{
    createPanels();
    createMenus();

    connect(m_undo, &QUndoStack::cleanChanged, this, &MainWindow::updateTitle);
    updateTitle();

    QSettings settings;
    if (!restoreGeometry(settings.value("window/geometry").toByteArray()))
        resize(1400, 860);
    restoreState(settings.value("window/state").toByteArray());
}

void MainWindow::createPanels()
{
    setDockOptions(AnimatedDocks | AllowNestedDocks | AllowTabbedDocks);

    // The program viewer is the heart of the window; everything else docks around it.
    setCentralWidget(makeViewer(tr("Program viewer"), this));

    m_panelsMenu = new QMenu(tr("&Panels"), this);
    auto addPanel = [this](const QString &objectName, const QString &title, QWidget *widget,
                           Qt::DockWidgetArea area) {
        auto *dock = new QDockWidget(title, this);
        dock->setObjectName(objectName); // needed by saveState/restoreState
        dock->setWidget(widget);
        addDockWidget(area, dock);
        m_panelsMenu->addAction(dock->toggleViewAction());
        return dock;
    };

    auto *media = new QLabel(tr("Drag media files here.\nImporting arrives in the next version."), this);
    media->setAlignment(Qt::AlignCenter);
    media->setWordWrap(true);
    addPanel(QStringLiteral("media"), tr("Media"), media, Qt::LeftDockWidgetArea);
    addPanel(QStringLiteral("source"), tr("Source viewer"), makeViewer(tr("No clip loaded"), this),
             Qt::LeftDockWidgetArea);

    m_timeline = new TimelinePanel(m_project, this);
    addPanel(QStringLiteral("timeline"), tr("Timeline"), m_timeline, Qt::BottomDockWidgetArea);

    auto *inspector = new InspectorPanel(m_project, this);
    connect(inspector, &InspectorPanel::settingsEdited, this, [this](const ProjectSettings &s) {
        m_undo->push(new SetSettingsCommand(m_project, s));
    });
    addPanel(QStringLiteral("inspector"), tr("Inspector"), inspector, Qt::RightDockWidgetArea);
}

void MainWindow::createMenus()
{
    QMenu *file = menuBar()->addMenu(tr("&File"));
    file->addAction(tr("&New project"), QKeySequence::New, this, &MainWindow::newProject);
    file->addAction(tr("&Open project..."), QKeySequence::Open, this, &MainWindow::openProjectDialog);
    file->addSeparator();
    file->addAction(tr("&Save"), QKeySequence::Save, this, &MainWindow::save);
    file->addAction(tr("Save &as..."), QKeySequence::SaveAs, this, &MainWindow::saveAs);
    file->addSeparator();
    file->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    QMenu *edit = menuBar()->addMenu(tr("&Edit"));
    QAction *undo = m_undo->createUndoAction(this, tr("&Undo"));
    undo->setShortcut(QKeySequence::Undo);
    QAction *redo = m_undo->createRedoAction(this, tr("&Redo"));
    redo->setShortcuts(QKeySequence::Redo);
    edit->addAction(undo);
    edit->addAction(redo);

    QMenu *timeline = menuBar()->addMenu(tr("&Timeline"));
    timeline->addAction(tr("Add &video track"), this, [this] { addTrack(true); });
    timeline->addAction(tr("Add &audio track"), this, [this] { addTrack(false); });
    timeline->addAction(tr("&Remove selected track"), this, &MainWindow::removeTrack);

    QMenu *view = menuBar()->addMenu(tr("&View"));
    view->addMenu(m_panelsMenu);

    QMenu *language = view->addMenu(tr("&Language"));
    auto *group = new QActionGroup(language);
    const QString current = language::preferred();
    // Language names are written in their own language on purpose.
    const QList<QPair<QString, QString>> choices = {
        {QString(), tr("System default")},
        {QStringLiteral("en"), QStringLiteral("English")},
        {QStringLiteral("es"), QStringLiteral("Español")},
    };
    for (const auto &choice : choices) {
        const QString code = choice.first;
        QAction *action = language->addAction(choice.second, this, [this, code] { chooseLanguage(code); });
        action->setCheckable(true);
        action->setChecked(code == current);
        group->addAction(action);
    }

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("&About FramePad"), this, &MainWindow::about);
}

void MainWindow::newProject()
{
    if (!confirmDiscard())
        return;
    m_project->resetToDefault();
    m_undo->clear();
    m_path.clear();
    updateTitle();
}

void MainWindow::openProjectDialog()
{
    if (!confirmDiscard())
        return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Open project"), QString(), fileFilter());
    if (!path.isEmpty())
        openProject(path);
}

bool MainWindow::openProject(const QString &path)
{
    QString error;
    if (!m_project->load(path, &error)) {
        QMessageBox::warning(this, tr("Open project"),
                             tr("Could not open %1:\n%2").arg(QFileInfo(path).fileName(), error));
        return false;
    }
    m_undo->clear();
    m_path = path;
    updateTitle();
    return true;
}

bool MainWindow::save()
{
    if (m_path.isEmpty())
        return saveAs();

    QString error;
    if (!m_project->save(m_path, &error)) {
        QMessageBox::warning(this, tr("Save project"),
                             tr("Could not save %1:\n%2").arg(QFileInfo(m_path).fileName(), error));
        return false;
    }
    m_undo->setClean();
    updateTitle();
    return true;
}

bool MainWindow::saveAs()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save project"), m_path, fileFilter());
    if (path.isEmpty())
        return false;
    m_path = path;
    return save();
}

void MainWindow::addTrack(bool video)
{
    const TrackKind kind = video ? TrackKind::Video : TrackKind::Audio;
    int count = 0;
    for (const Track &t : m_project->tracks())
        count += t.kind == kind ? 1 : 0;

    // Video tracks go above audio tracks, matching how the timeline stacks them.
    int index = 0;
    while (index < m_project->tracks().size() && m_project->tracks().at(index).kind == TrackKind::Video)
        ++index;
    if (!video)
        index = int(m_project->tracks().size());

    const QString prefix = video ? QStringLiteral("V") : QStringLiteral("A");
    Track track{prefix + QString::number(count + 1), kind};
    m_undo->push(new AddTrackCommand(m_project, index, track));
}

void MainWindow::removeTrack()
{
    const int index = m_timeline->currentTrack();
    if (index < 0) {
        QMessageBox::information(this, tr("Remove track"), tr("Select a track in the timeline first."));
        return;
    }
    m_undo->push(new RemoveTrackCommand(m_project, index));
}

void MainWindow::chooseLanguage(const QString &code)
{
    if (code == language::preferred())
        return;
    language::setPreferred(code);
    QMessageBox::information(this, tr("Language"), tr("The new language will be used the next time FramePad starts."));
}

void MainWindow::about()
{
    QMessageBox::about(this, tr("About FramePad"),
                       tr("<b>FramePad %1</b><br>Open-source video editor for Windows.<br>"
                          "Licensed under the GNU GPL v3.")
                           .arg(QStringLiteral(FRAMEPAD_VERSION)));
}

bool MainWindow::confirmDiscard()
{
    if (m_undo->isClean())
        return true;
    const auto answer = QMessageBox::question(
        this, tr("Unsaved changes"), tr("Do you want to save the changes to this project?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Save)
        return save();
    return answer == QMessageBox::Discard;
}

void MainWindow::updateTitle()
{
    const QString name = m_path.isEmpty() ? tr("Untitled") : QFileInfo(m_path).completeBaseName();
    setWindowTitle(QStringLiteral("%1[*] — FramePad").arg(name));
    setWindowModified(!m_undo->isClean());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!confirmDiscard()) {
        event->ignore();
        return;
    }
    QSettings settings;
    settings.setValue("window/geometry", saveGeometry());
    settings.setValue("window/state", saveState());
    event->accept();
}

} // namespace fp
