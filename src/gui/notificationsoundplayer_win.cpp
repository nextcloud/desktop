/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// Belt-and-braces guard for source-scanning tools. CMake already restricts
// this translation unit to Windows builds, but the project's clang-tidy CI
// (`.github/workflows/clang-tidy-review.yml`) runs `clang-tidy-diff` over
// every new .cpp file in the diff on a Linux container, regardless of the
// CMake gate. Without this `#if`, the analyzer chokes on <windows.h>. Same
// pattern is used in src/gui/application.cpp.
#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xaudio2.h>

#include "notificationsoundplayer.h"
#include "notificationsoundplayer_p.h"

#include <QByteArray>
#include <QFile>
#include <QLoggingCategory>
#include <QString>

#include <cstring>

namespace OCC {

Q_LOGGING_CATEGORY(lcNotificationSoundPlayerWin, "nextcloud.gui.notificationsoundplayer.win", QtInfoMsg)

namespace {

// Parse a RIFF/WAVE PCM file into a WAVEFORMATEX descriptor and the byte
// range covering the audio samples. Accepts 8/16/24-bit PCM, mono or stereo,
// at any sample rate. Rejects everything else with a logged reason so a
// future asset swap surfaces an actionable error instead of silent silence.
bool parseWavFile(const QByteArray &bytes,
                  WAVEFORMATEX &format,
                  quint32 &audioDataOffset,
                  quint32 &audioDataSize)
{
    const auto fail = [](const char *reason) {
        qCWarning(lcNotificationSoundPlayerWin) << "Cannot parse WAV file:" << reason;
        return false;
    };

    const auto totalSize = static_cast<quint32>(bytes.size());
    if (totalSize < 44) {
        return fail("file is shorter than the smallest possible RIFF/WAVE header");
    }
    const char *data = bytes.constData();
    if (std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0) {
        return fail("not a RIFF/WAVE file");
    }

    quint32 offset = 12;
    bool fmtFound = false;
    bool dataFound = false;

    while (offset + 8 <= totalSize) {
        char chunkId[4];
        std::memcpy(chunkId, data + offset, 4);
        quint32 chunkSize = 0;
        std::memcpy(&chunkSize, data + offset + 4, 4);
        offset += 8;
        if (offset + chunkSize > totalSize) {
            return fail("chunk extends past end of file");
        }

        if (std::memcmp(chunkId, "fmt ", 4) == 0) {
            if (chunkSize < 16) {
                return fail("fmt chunk too small");
            }
            quint16 formatTag = 0;
            quint16 channels = 0;
            quint32 sampleRate = 0;
            quint32 byteRate = 0;
            quint16 blockAlign = 0;
            quint16 bitsPerSample = 0;
            std::memcpy(&formatTag, data + offset, 2);
            std::memcpy(&channels, data + offset + 2, 2);
            std::memcpy(&sampleRate, data + offset + 4, 4);
            std::memcpy(&byteRate, data + offset + 8, 4);
            std::memcpy(&blockAlign, data + offset + 12, 2);
            std::memcpy(&bitsPerSample, data + offset + 14, 2);

            if (formatTag != WAVE_FORMAT_PCM) {
                return fail("only uncompressed PCM is supported");
            }
            if (channels != 1 && channels != 2) {
                return fail("only mono and stereo are supported");
            }
            if (bitsPerSample != 8 && bitsPerSample != 16 && bitsPerSample != 24) {
                return fail("only 8-, 16- and 24-bit PCM are supported");
            }

            std::memset(&format, 0, sizeof(format));
            format.wFormatTag = WAVE_FORMAT_PCM;
            format.nChannels = channels;
            format.nSamplesPerSec = sampleRate;
            format.nAvgBytesPerSec = byteRate;
            format.nBlockAlign = blockAlign;
            format.wBitsPerSample = bitsPerSample;
            format.cbSize = 0;
            fmtFound = true;
        } else if (std::memcmp(chunkId, "data", 4) == 0) {
            audioDataOffset = offset;
            audioDataSize = chunkSize;
            dataFound = true;
        }

        offset += chunkSize;
        // RIFF chunks are padded to even size.
        if (chunkSize & 1u) {
            ++offset;
        }
    }

    if (!fmtFound) {
        return fail("no fmt chunk");
    }
    if (!dataFound) {
        return fail("no data chunk");
    }
    return true;
}

class VoiceCallback : public IXAudio2VoiceCallback
{
public:
    std::function<void()> onBufferEnd;

    void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
    void STDMETHODCALLTYPE OnStreamEnd() override {}
    void STDMETHODCALLTYPE OnBufferStart(void *) override {}
    void STDMETHODCALLTYPE OnBufferEnd(void *) override
    {
        if (onBufferEnd) {
            onBufferEnd();
        }
    }
    void STDMETHODCALLTYPE OnLoopEnd(void *) override {}
    void STDMETHODCALLTYPE OnVoiceError(void *, HRESULT) override {}
};

class WinBackend : public NotificationSoundPlayer::Backend
{
public:
    WinBackend()
    {
        HRESULT hr = XAudio2Create(&_xaudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
        if (FAILED(hr)) {
            qCWarning(lcNotificationSoundPlayerWin) << "XAudio2Create failed, hr =" << QString::number(hr, 16);
            return;
        }
        hr = _xaudio2->CreateMasteringVoice(&_masteringVoice);
        if (FAILED(hr)) {
            qCWarning(lcNotificationSoundPlayerWin) << "CreateMasteringVoice failed, hr ="
                                                     << QString::number(hr, 16);
            _xaudio2->Release();
            _xaudio2 = nullptr;
            return;
        }
        _callback.onBufferEnd = [this]() {
            if (onFinished) {
                onFinished();
            }
        };
    }

    ~WinBackend() override
    {
        if (_sourceVoice) {
            _sourceVoice->Stop(0);
            _sourceVoice->FlushSourceBuffers();
            // DestroyVoice blocks until all callbacks for this voice complete,
            // so `_callback` is safe to drop afterwards.
            _sourceVoice->DestroyVoice();
            _sourceVoice = nullptr;
        }
        _callback.onBufferEnd = nullptr;
        if (_masteringVoice) {
            _masteringVoice->DestroyVoice();
            _masteringVoice = nullptr;
        }
        if (_xaudio2) {
            _xaudio2->Release();
            _xaudio2 = nullptr;
        }
    }

    void setSource(const QString &filesystemPath) override
    {
        if (!_xaudio2) {
            return;
        }

        QFile file(filesystemPath);
        if (!file.open(QIODevice::ReadOnly)) {
            qCWarning(lcNotificationSoundPlayerWin) << "Could not open" << filesystemPath
                                                     << ":" << file.errorString();
            return;
        }
        _audioBytes = file.readAll();
        file.close();

        if (!parseWavFile(_audioBytes, _waveFormat, _audioDataOffset, _audioDataSize)) {
            _audioBytes.clear();
            return;
        }

        if (_sourceVoice) {
            _sourceVoice->Stop(0);
            _sourceVoice->FlushSourceBuffers();
            _sourceVoice->DestroyVoice();
            _sourceVoice = nullptr;
        }

        HRESULT hr = _xaudio2->CreateSourceVoice(&_sourceVoice, &_waveFormat,
                                                  0, XAUDIO2_DEFAULT_FREQ_RATIO,
                                                  &_callback);
        if (FAILED(hr)) {
            qCWarning(lcNotificationSoundPlayerWin) << "CreateSourceVoice failed, hr ="
                                                     << QString::number(hr, 16);
            _audioBytes.clear();
        }
    }

    void play(int loops) override
    {
        if (!_sourceVoice || _audioBytes.isEmpty()) {
            return;
        }
        _sourceVoice->Stop(0);
        _sourceVoice->FlushSourceBuffers();

        XAUDIO2_BUFFER buffer = {};
        buffer.AudioBytes = _audioDataSize;
        buffer.pAudioData = reinterpret_cast<const BYTE *>(_audioBytes.constData() + _audioDataOffset);
        buffer.Flags = XAUDIO2_END_OF_STREAM;
        buffer.LoopBegin = 0;
        buffer.LoopLength = 0;
        // XAUDIO2_BUFFER::LoopCount counts ADDITIONAL plays after the initial
        // pass: LoopCount=0 plays once, LoopCount=8 plays 9 times.
        buffer.LoopCount = (loops > 1) ? static_cast<UINT32>(loops - 1) : 0;

        HRESULT hr = _sourceVoice->SubmitSourceBuffer(&buffer);
        if (FAILED(hr)) {
            qCWarning(lcNotificationSoundPlayerWin) << "SubmitSourceBuffer failed, hr ="
                                                     << QString::number(hr, 16);
            return;
        }
        hr = _sourceVoice->Start(0);
        if (FAILED(hr)) {
            qCWarning(lcNotificationSoundPlayerWin) << "Start failed, hr =" << QString::number(hr, 16);
        }
    }

    void stop() override
    {
        if (_sourceVoice) {
            _sourceVoice->Stop(0);
            _sourceVoice->FlushSourceBuffers();
        }
    }

    [[nodiscard]] bool handlesLoopsNatively() const override { return true; }
    [[nodiscard]] bool needsFilesystemPath() const override { return true; }

private:
    IXAudio2 *_xaudio2 = nullptr;
    IXAudio2MasteringVoice *_masteringVoice = nullptr;
    IXAudio2SourceVoice *_sourceVoice = nullptr;
    WAVEFORMATEX _waveFormat = {};
    QByteArray _audioBytes;
    quint32 _audioDataOffset = 0;
    quint32 _audioDataSize = 0;
    VoiceCallback _callback;
};

}

std::unique_ptr<NotificationSoundPlayer::Backend> createNotificationSoundPlayerBackend()
{
    return std::make_unique<WinBackend>();
}

}

#endif // defined(_WIN32)
