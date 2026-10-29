#include "project/Commands.h"
#include "project/Project.h"

#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>
#include <QUndoStack>

using namespace fp;

class TestProject : public QObject {
    Q_OBJECT
private slots:
    void newProjectHasOneVideoAndOneAudioTrack()
    {
        Project p;
        QCOMPARE(int(p.tracks().size()), 2);
        QVERIFY(p.tracks().at(0).kind == TrackKind::Video);
        QVERIFY(p.tracks().at(1).kind == TrackKind::Audio);
    }

    void saveAndReopenKeepsEverything()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("test.fpproj"));

        Project original;
        original.setSettings({3840, 2160, 23.976, 44100});
        original.insertTrack(1, {QStringLiteral("V2"), TrackKind::Video, true, true});

        QString error;
        QVERIFY2(original.save(path, &error), qPrintable(error));

        Project reopened;
        QVERIFY2(reopened.load(path, &error), qPrintable(error));
        QVERIFY(reopened.settings() == original.settings());
        QVERIFY(reopened.tracks() == original.tracks());
    }

    void refusesProjectsFromNewerVersions()
    {
        QJsonObject json = Project().toJson();
        json["formatVersion"] = Project::FormatVersion + 1;
        Project p;
        QString error;
        QVERIFY(!p.fromJson(json, &error));
        QVERIFY(!error.isEmpty());
    }

    void refusesFilesThatAreNotProjects()
    {
        Project p;
        QString error;
        QVERIFY(!p.fromJson(QJsonObject{{"hello", "world"}}, &error));
    }

    void undoAndRedoTrackChanges()
    {
        Project p;
        QUndoStack stack;
        const auto before = p.tracks();

        stack.push(new AddTrackCommand(&p, 1, {QStringLiteral("V2"), TrackKind::Video}));
        QCOMPARE(int(p.tracks().size()), 3);
        stack.push(new RemoveTrackCommand(&p, 0));
        QCOMPARE(int(p.tracks().size()), 2);
        QCOMPARE(p.tracks().at(0).name, QStringLiteral("V2"));

        stack.undo();
        stack.undo();
        QVERIFY(p.tracks() == before);

        stack.redo();
        QCOMPARE(int(p.tracks().size()), 3);
    }

    void undoSettingsChange()
    {
        Project p;
        QUndoStack stack;
        const ProjectSettings before = p.settings();
        stack.push(new SetSettingsCommand(&p, {1280, 720, 60.0, 48000}));
        QCOMPARE(p.settings().width, 1280);
        stack.undo();
        QVERIFY(p.settings() == before);
    }
};

QTEST_GUILESS_MAIN(TestProject)
#include "tst_project.moc"
