// Part of harbour_icq_nina; GPL-3.0-or-later.
#include "voicerecorder.h"

#include <QAudioEncoderSettings>
#include <QAudioInput>
#include <QAudioRecorder>
#include <QDataStream>
#include <QDebug>
#include <QFileInfo>
#include <QUrl>

namespace {
const int kWavRate = 16000;

struct Choice { const char *codec; const char *container; const char *extension; };

// Best first: small files that browsers can play too.
const Choice kChoices[] = {
    { "opus", "ogg", "ogg" },
    { "vorbis", "ogg", "ogg" },
    { "aac", "mp4", "m4a" },
    { "mpeg", "mp4", "m4a" },
};

QString find(const QStringList &list, const char *what)
{
    for (const QString &s : list)
        if (s.contains(QLatin1String(what), Qt::CaseInsensitive))
            return s;
    return QString();
}

QByteArray wavHeader(qint64 dataBytes)
{
    QByteArray h;
    QDataStream out(&h, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF", 4);
    out << quint32(36 + dataBytes);
    out.writeRawData("WAVEfmt ", 8);
    out << quint32(16) << quint16(1) << quint16(1) << quint32(kWavRate) << quint32(kWavRate * 2)
        << quint16(2) << quint16(16);
    out.writeRawData("data", 4);
    out << quint32(dataBytes);
    return h;
}
} // namespace

VoiceRecorder::VoiceRecorder(QObject *parent)
    : QObject(parent)
{
    m_tick.setInterval(200);
    connect(&m_tick, &QTimer::timeout, this, [this]() {
        emit durationChanged();
        if (m_recording && duration() >= maxDuration())
            stop();
    });
    m_finishTimeout.setSingleShot(true);
    m_finishTimeout.setInterval(4000);
    connect(&m_finishTimeout, &QTimer::timeout, this, [this]() {
        if (m_finishing)
            deliver(m_recorder ? QUrl(m_recorder->actualLocation()).toLocalFile() : m_path);
    });
}

VoiceRecorder::~VoiceRecorder()
{
    if (m_recording || m_finishing)
        cancel();
}

int VoiceRecorder::duration() const
{
    return m_recording ? int(m_clock.elapsed()) : m_duration;
}

void VoiceRecorder::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorChanged();
}

void VoiceRecorder::setRecording(bool recording, bool finishing)
{
    if (m_recording == recording && m_finishing == finishing)
        return;
    m_recording = recording;
    m_finishing = finishing;
    if (recording)
        m_tick.start();
    else
        m_tick.stop();
    emit recordingChanged();
    emit durationChanged();
}

bool VoiceRecorder::start(const QString &basePath)
{
    if (m_recording || m_finishing)
        return false;
    setError(QString());
    m_cancelled = false;
    m_basePath = basePath;
    m_duration = 0;
    if (!startEncoder(basePath) && !startWav(basePath)) {
        if (m_error.isEmpty())
            setError(tr("Cannot use the microphone."));
        return false;
    }
    m_clock.start();
    setRecording(true);
    return true;
}

bool VoiceRecorder::startEncoder(const QString &basePath)
{
    dropRecorder();
    m_recorder = new QAudioRecorder(this);
    if (!m_recorder->isAvailable()) {
        qInfo() << "voice: no encoder available, recording WAV";
        dropRecorder();
        return false;
    }
    const QStringList codecs = m_recorder->supportedAudioCodecs();
    const QStringList containers = m_recorder->supportedContainers();
    qInfo() << "voice: codecs" << codecs << "containers" << containers;
    QString codec, container;
    const char *extension = nullptr;
    for (const Choice &c : kChoices) {
        codec = find(codecs, c.codec);
        container = find(containers, c.container);
        if (!codec.isEmpty() && !container.isEmpty()) {
            extension = c.extension;
            break;
        }
    }
    if (!extension) {
        dropRecorder();
        return false;
    }
    QAudioEncoderSettings settings;
    settings.setCodec(codec);
    settings.setChannelCount(1);
    settings.setEncodingMode(QMultimedia::ConstantQualityEncoding);
    settings.setQuality(QMultimedia::NormalQuality);
    m_recorder->setEncodingSettings(settings, QVideoEncoderSettings(), container);
    m_path = basePath + QLatin1Char('.') + QLatin1String(extension);
    m_recorder->setOutputLocation(QUrl::fromLocalFile(m_path));

    connect(m_recorder, static_cast<void (QMediaRecorder::*)(QMediaRecorder::Error)>(&QMediaRecorder::error),
            this, [this](QMediaRecorder::Error) {
        if (!m_recorder)
            return;
        const QString why = m_recorder->errorString();
        qWarning() << "voice: encoder error" << why;
        // Failing right at the start: carry on with WAV instead.
        if (m_recording && m_clock.elapsed() < 3000) {
            QFile::remove(m_path);
            dropRecorder();
            if (startWav(m_basePath)) {
                m_clock.restart();
                return;
            }
        }
        const bool wasRecording = m_recording || m_finishing;
        dropRecorder();
        setRecording(false);
        setError(why.isEmpty() ? tr("Recording failed.") : why);
        if (wasRecording)
            emit failed(m_error);
    });
    connect(m_recorder, &QMediaRecorder::statusChanged, this, [this](QMediaRecorder::Status status) {
        if (m_finishing && (status == QMediaRecorder::LoadedStatus || status == QMediaRecorder::UnloadedStatus))
            finishEncoder();
    });
    m_recorder->record();
    return true;
}

bool VoiceRecorder::startWav(const QString &basePath)
{
    QAudioFormat format;
    format.setSampleRate(kWavRate);
    format.setChannelCount(1);
    format.setSampleSize(16);
    format.setSampleType(QAudioFormat::SignedInt);
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setCodec(QStringLiteral("audio/pcm"));
    const QAudioDeviceInfo device = QAudioDeviceInfo::defaultInputDevice();
    if (device.isNull() || !device.isFormatSupported(format)) {
        setError(tr("No microphone found."));
        return false;
    }
    m_path = basePath + QStringLiteral(".wav");
    m_wav.setFileName(m_path);
    if (!m_wav.open(QIODevice::WriteOnly)) {
        setError(tr("Cannot save the recording."));
        return false;
    }
    m_wav.write(wavHeader(0));
    m_wavBytes = 0;
    m_input = new QAudioInput(device, format, this);
    m_inputDevice = m_input->start();
    if (!m_inputDevice) {
        delete m_input;
        m_input = nullptr;
        m_wav.close();
        m_wav.remove();
        setError(tr("Cannot use the microphone."));
        return false;
    }
    connect(m_inputDevice.data(), &QIODevice::readyRead, this, [this]() {
        if (!m_inputDevice)
            return;
        const QByteArray data = m_inputDevice->readAll();
        m_wavBytes += m_wav.write(data);
    });
    return true;
}

void VoiceRecorder::stop()
{
    if (!m_recording)
        return;
    m_duration = int(m_clock.elapsed());
    if (m_recorder) {
        setRecording(false, true);
        m_finishTimeout.start();
        m_recorder->stop();
        if (m_recorder && m_recorder->status() == QMediaRecorder::LoadedStatus)
            finishEncoder();
    } else {
        setRecording(false);
        finishWav();
    }
}

void VoiceRecorder::finishEncoder()
{
    if (!m_finishing)
        return;
    const QUrl actual = m_recorder ? m_recorder->actualLocation() : QUrl();
    deliver(actual.isLocalFile() ? actual.toLocalFile() : m_path);
}

void VoiceRecorder::finishWav()
{
    if (m_input) {
        m_input->stop();
        if (m_inputDevice)
            m_inputDevice->disconnect(this);
        m_input->deleteLater();
        m_input = nullptr;
    }
    if (m_wav.isOpen()) {
        m_wav.seek(0);
        m_wav.write(wavHeader(m_wavBytes));
        m_wav.close();
    }
    deliver(m_path);
}

void VoiceRecorder::deliver(const QString &path)
{
    m_finishTimeout.stop();
    const bool cancelled = m_cancelled;
    dropRecorder();
    setRecording(false);
    if (cancelled) {
        QFile::remove(path);
        return;
    }
    const QFileInfo fi(path);
    // Under half a second, or nothing written: not a message.
    if (m_duration < 500 || !fi.exists() || fi.size() < 200) {
        QFile::remove(path);
        setError(tr("Too short. Tap the microphone, speak, then tap send."));
        emit failed(m_error);
        return;
    }
    emit finished(path, m_duration);
}

void VoiceRecorder::cancel()
{
    if (!m_recording && !m_finishing)
        return;
    m_cancelled = true;
    if (m_recording)
        stop();
}

void VoiceRecorder::dropRecorder()
{
    if (m_recorder) {
        m_recorder->disconnect(this);
        if (m_recorder->state() != QMediaRecorder::StoppedState)
            m_recorder->stop();
        m_recorder->deleteLater();
        m_recorder = nullptr;
    }
}
