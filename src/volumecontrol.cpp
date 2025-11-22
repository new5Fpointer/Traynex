#include "volumecontrol.h"
#include <QDebug>
#include <thread>
#include <atomic>
#include <chrono>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

QString VolumeControl::GetExeName(DWORD pid)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return {};
    wchar_t path[MAX_PATH]{};
    DWORD len = MAX_PATH;
    QueryFullProcessImageNameW(h, 0, path, &len);
    CloseHandle(h);
    const wchar_t* name = wcsrchr(path, L'\\');
    return QString::fromWCharArray(name ? name + 1 : path).toLower();
}

bool VolumeControl::SetProcessMute(DWORD processId, bool mute)
{
    CoInitialize(nullptr);
    IMMDeviceEnumerator* deviceEnumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioSessionManager2* sessionManager = nullptr;
    IAudioSessionEnumerator* sessionEnumerator = nullptr;
    int sessionCount = 0;
    bool anyMuted = false;
    QString targetExe = GetExeName(processId);

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&deviceEnumerator);
    if (FAILED(hr)) goto cleanup;

    hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (FAILED(hr)) goto cleanup;

    hr = device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&sessionManager);
    if (FAILED(hr)) goto cleanup;

    hr = sessionManager->GetSessionEnumerator(&sessionEnumerator);
    if (FAILED(hr)) goto cleanup;

    hr = sessionEnumerator->GetCount(&sessionCount);
    if (FAILED(hr)) goto cleanup;

    if (targetExe.isEmpty()) goto cleanup;

    for (int i = 0; i < sessionCount; ++i) {
        IAudioSessionControl* control = nullptr;
        IAudioSessionControl2* control2 = nullptr;
        ISimpleAudioVolume* volume = nullptr;

        hr = sessionEnumerator->GetSession(i, &control);
        if (FAILED(hr)) continue;

        hr = control->QueryInterface(&control2);
        if (SUCCEEDED(hr)) {
            DWORD pid = 0;
            control2->GetProcessId(&pid);
            if (pid && GetExeName(pid) == targetExe) {   // 同一款软件全部静音
                hr = control->QueryInterface(&volume);
                if (SUCCEEDED(hr)) {
                    volume->SetMute(mute, nullptr);
                    volume->Release();
                    anyMuted = true;
                }
            }
            control2->Release();
        }
        control->Release();
    }

cleanup:
    if (sessionEnumerator) sessionEnumerator->Release();
    if (sessionManager) sessionManager->Release();
    if (device) device->Release();
    if (deviceEnumerator) deviceEnumerator->Release();
    CoUninitialize();
    return anyMuted;
}

bool VolumeControl::SetProcessMuteWithTimeout(DWORD processId, bool mute, int timeoutMs)
{
    std::atomic<bool> done{ false };
    std::atomic<bool> result{ false };

    std::thread worker([&]() {
        result = SetProcessMute(processId, mute);
        done = true;
        });

    auto start = std::chrono::steady_clock::now();
    while (!done) {
        if (std::chrono::steady_clock::now() - start > std::chrono::milliseconds(timeoutMs)) {
            worker.detach();
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (worker.joinable()) worker.join();
    return result.load();
}