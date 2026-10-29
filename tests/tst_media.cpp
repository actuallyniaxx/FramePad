#include "media/MediaReader.h"
#include "media/Previews.h"

#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>

using namespace fp;

class TestMedia : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        if (!QFileInfo::exists(QStringLiteral(FRAMEPAD_TEST_FFMPEG)))
            QSKIP("ffmpeg.exe not found next to the FFmpeg libraries");
        QVERIFY(m_dir.isValid());
        // 2 s of 320x240 at 25 fps with a 440 Hz tone at 44.1 kHz, and 1 s of a 1 kHz tone alone.
        m_clip = m_dir.filePath(QStringLiteral("clip.mp4"));
        makeMedia({"-f", "lavfi", "-i", "testsrc2=size=320x240:rate=25:duration=2",
                   "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=44100:duration=2",
                   "-c:v", "libx264", "-pix_fmt", "yuv420p", "-c:a", "aac", "-shortest", m_clip});
        m_tone = m_dir.filePath(QStringLiteral("tone.wav"));
        makeMedia({"-f", "lavfi", "-i", "sine=frequency=1000:duration=1", m_tone});
    }

    void probeReadsStreams()
    {
        MediaReader reader;
        QString error;
        QVERIFY2(reader.open(m_clip, &error), qPrintable(error));
        const MediaInfo &info = reader.info();
        QVERIFY(info.hasVideo);
        QCOMPARE(info.width, 320);
        QCOMPARE(info.height, 240);
        QCOMPARE(qRound(info.fps), 25);
        QVERIFY(info.hasAudio);
        QCOMPARE(info.sampleRate, 44100);
        QVERIFY2(info.duration > 1.9 && info.duration < 2.2, qPrintable(QString::number(info.duration)));
    }

    void decodesEveryFrame()
    {
        MediaReader reader;
        QVERIFY(reader.open(m_clip, nullptr));
        std::vector<double> videoPts;
        long long audioFrames = 0;
        bool planesOk = true;
        reader.onVideo = [&](VideoFramePtr frame) {
            videoPts.push_back(frame->pts);
            planesOk = planesOk && frame->planes[0].size() == 320 * 240 && frame->planes[1].size() == 160 * 120
                && frame->planes[2].size() == 160 * 120;
        };
        reader.onAudio = [&](AudioChunk &&chunk) { audioFrames += chunk.frames(); };
        while (reader.step()) {
        }

        QVERIFY2(videoPts.size() >= 49 && videoPts.size() <= 51, qPrintable(QString::number(videoPts.size())));
        QVERIFY(std::is_sorted(videoPts.begin(), videoPts.end()));
        QVERIFY(planesOk);
        // Resampled to 48 kHz: about 2 s worth, give or take the AAC priming samples.
        QVERIFY2(audioFrames > AudioRate * 1.9 && audioFrames < AudioRate * 2.1, qPrintable(QString::number(audioFrames)));
    }

    void seekLandsOnTarget()
    {
        MediaReader reader;
        QVERIFY(reader.open(m_clip, nullptr));
        double firstVideo = -1.0;
        double firstAudio = -1.0;
        reader.onVideo = [&](VideoFramePtr frame) {
            if (firstVideo < 0.0)
                firstVideo = frame->pts;
        };
        reader.onAudio = [&](AudioChunk &&chunk) {
            if (firstAudio < 0.0)
                firstAudio = chunk.pts;
        };
        reader.seek(1.0);
        while ((firstVideo < 0.0 || firstAudio < 0.0) && reader.step()) {
        }
        QVERIFY2(firstVideo > 0.97 && firstVideo < 1.05, qPrintable(QString::number(firstVideo)));
        QVERIFY2(firstAudio > 0.99 && firstAudio < 1.03, qPrintable(QString::number(firstAudio)));
    }

    void thumbnailFitsTheBox()
    {
        QString error;
        const QImage image = makeThumbnail(m_clip, QSize(160, 90), &error);
        QVERIFY2(!image.isNull(), qPrintable(error));
        QCOMPARE(image.size(), QSize(120, 90)); // 4:3 inside 16:9
    }

    void peaksFollowTheAudio()
    {
        QString error;
        const std::vector<float> peaks = computePeaks(m_tone, 50, &error);
        QVERIFY2(peaks.size() == 50, qPrintable(error));
        const float loudest = *std::max_element(peaks.begin(), peaks.end());
        QVERIFY2(loudest > 0.05f && loudest <= 1.0f, qPrintable(QString::number(loudest)));
        QVERIFY(makeThumbnail(m_tone, QSize(160, 90)).isNull());
    }

    void rejectsMissingFiles()
    {
        MediaReader reader;
        QString error;
        QVERIFY(!reader.open(m_dir.filePath(QStringLiteral("nothing-here.mp4")), &error));
        QVERIFY(!error.isEmpty());
    }

    void timecodes()
    {
        QCOMPARE(formatTimecode(0.0, 25.0), QStringLiteral("00:00:00:00"));
        QCOMPARE(formatTimecode(3661.5, 25.0), QStringLiteral("01:01:01:12"));
        QCOMPARE(formatTimecode(1.0, 29.97), QStringLiteral("00:00:01:00"));
    }

private:
    void makeMedia(QStringList args)
    {
        args.prepend(QStringLiteral("error"));
        args.prepend(QStringLiteral("-v"));
        args.prepend(QStringLiteral("-y"));
        QProcess ffmpeg;
        ffmpeg.start(QStringLiteral(FRAMEPAD_TEST_FFMPEG), args);
        QVERIFY2(ffmpeg.waitForFinished(120000), "ffmpeg did not finish");
        QVERIFY2(ffmpeg.exitCode() == 0, ffmpeg.readAllStandardError().constData());
    }

    QTemporaryDir m_dir;
    QString m_clip;
    QString m_tone;
};

QTEST_GUILESS_MAIN(TestMedia)
#include "tst_media.moc"
