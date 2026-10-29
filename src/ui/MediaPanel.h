#pragma once

#include <QHash>
#include <QPixmap>
#include <QSet>
#include <QWidget>

class QListWidget;
class QListWidgetItem;
class QStackedLayout;

namespace fp {

class Project;

// The media bin: the project's imported files with a thumbnail (or waveform for audio)
// and a one-line summary. Previews are made on a worker thread and cached by path.
class MediaPanel : public QWidget {
    Q_OBJECT
public:
    explicit MediaPanel(Project *project, QWidget *parent = nullptr);

    QList<int> selectedRows() const;

signals:
    void importRequested();
    void filesDropped(const QStringList &paths);
    void openRequested(const QString &path);
    void removeRequested(const QList<int> &rows);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    struct Preview {
        QPixmap pixmap;
        QString detail;
    };

    void rebuild();
    void showPreview(QListWidgetItem *item);
    void requestPreview(const QString &path);

    Project *m_project;
    QListWidget *m_list;
    QStackedLayout *m_stack;
    QHash<QString, Preview> m_previews;
    QSet<QString> m_pending;
};

} // namespace fp
