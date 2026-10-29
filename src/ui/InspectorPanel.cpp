#include "ui/InspectorPanel.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLocale>
#include <QSignalBlocker>
#include <QSpinBox>

namespace fp {

InspectorPanel::InspectorPanel(Project *project, QWidget *parent)
    : QWidget(parent)
    , m_project(project)
    , m_width(new QSpinBox(this))
    , m_height(new QSpinBox(this))
    , m_fps(new QComboBox(this))
    , m_sampleRate(new QComboBox(this))
{
    for (QSpinBox *box : {m_width, m_height}) {
        box->setRange(16, 8192);
        box->setSingleStep(2);
        box->setSuffix(QStringLiteral(" px"));
    }

    for (double fps : {23.976, 24.0, 25.0, 29.97, 30.0, 50.0, 59.94, 60.0})
        m_fps->addItem(QLocale().toString(fps, 'g', 5), fps);
    for (int rate : {44100, 48000, 96000})
        m_sampleRate->addItem(QStringLiteral("%1 Hz").arg(rate), rate);

    auto *form = new QFormLayout(this);
    form->addRow(tr("Width"), m_width);
    form->addRow(tr("Height"), m_height);
    form->addRow(tr("Frame rate"), m_fps);
    form->addRow(tr("Sample rate"), m_sampleRate);

    // editingFinished, not valueChanged: one undo step per edit, not one per keystroke.
    connect(m_width, &QSpinBox::editingFinished, this, &InspectorPanel::emitEdited);
    connect(m_height, &QSpinBox::editingFinished, this, &InspectorPanel::emitEdited);
    connect(m_fps, &QComboBox::activated, this, &InspectorPanel::emitEdited);
    connect(m_sampleRate, &QComboBox::activated, this, &InspectorPanel::emitEdited);
    connect(m_project, &Project::settingsChanged, this, &InspectorPanel::refresh);
    refresh();
}

void InspectorPanel::refresh()
{
    const ProjectSettings &s = m_project->settings();
    const QSignalBlocker b1(m_width), b2(m_height), b3(m_fps), b4(m_sampleRate);
    m_width->setValue(s.width);
    m_height->setValue(s.height);
    m_fps->setCurrentIndex(qMax(0, m_fps->findData(s.fps)));
    m_sampleRate->setCurrentIndex(qMax(0, m_sampleRate->findData(s.sampleRate)));
}

void InspectorPanel::emitEdited()
{
    ProjectSettings s;
    s.width = m_width->value();
    s.height = m_height->value();
    s.fps = m_fps->currentData().toDouble();
    s.sampleRate = m_sampleRate->currentData().toInt();
    if (!(s == m_project->settings()))
        emit settingsEdited(s);
}

} // namespace fp
