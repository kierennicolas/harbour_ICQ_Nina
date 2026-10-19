// Records a voice message from the microphone. Uses the phone's encoder
// (Ogg Opus or Vorbis, else AAC) through QtMultimedia when it can, and falls
// back to a plain WAV file (bigger, but always works).
// Part of harbour_icq_nina; GPL-3.0-or-later.
#ifndef VOICERECORDER_H
#define VOICERECORDER_H

#include <QElapsedTimer>
#include <QFile>
#include <QObject>
#include <QPointer>
#include <QTimer>

class QAudioInput;
class QAudioRecorder;
class QIODevice;

class VoiceRecorder : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    // Waiting for the encoder to finish the file after stop().
    Q_PROPERTY(bool finishing READ finishing NOTIFY recordingChanged)
    Q_PROPERTY(int duration READ duration NOTIFY durationChanged)       // ms
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(int maxDuration READ maxDuration CONSTANT)

public:
    explicit VoiceRecorder(QObject *parent = nullptr);
    ~VoiceRecorder() override;

    bool recording() const { return m_recording; }
    bool finishing() const { return m_finishing; }
    int duration() const;
    QString error() const { return m_error; }
    int maxDuration() const { return 5 * 60 * 1000; }

    // basePath: the file to record into, without extension (the format
    // decides it). Returns false if recording could not start.
    Q_INVOKABLE bool start(const QString &basePath);
    // Stops; finished() follows once the file is complete.
    Q_INVOKABLE void stop();
    // Stops and throws the recording away.
    Q_INVOKABLE void cancel();

signals:
    void recordingChanged();
    void durationChanged();
    void errorChanged();
    void finished(const QString &path, int duration);
    void failed(const QString &message);

private:
    bool startEncoder(const QString &basePath);
    bool startWav(const QString &basePath);
    void finishEncoder();
    void finishWav();
    void deliver(const QString &path);
    void setError(const QString &error);
    void setRecording(bool recording, bool finishing = false);
    void dropRecorder();

    QAudioRecorder *m_recorder = nullptr;
    QAudioInput *m_input = nullptr;
    QPointer<QIODevice> m_inputDevice;
    QFile m_wav;
    qint64 m_wavBytes = 0;
    QString m_basePath;
    QString m_path;
    bool m_recording = false;
    bool m_finishing = false;
    bool m_cancelled = false;
    int m_duration = 0;
    QElapsedTimer m_clock;
    QTimer m_tick;
    QTimer m_finishTimeout;
    QString m_error;
};

#endif
