#include "volumecontrol.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <QDebug>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

bool VolumeControl::SetProcessMute(DWORD processId, bool mute) {
    CoInitialize(nullptr);

    IMMDeviceEnumerator* deviceEnumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioSessionManager2* sessionManager = nullptr;
    IAudioSessionEnumerator* sessionEnumerator = nullptr;
    int sessionCount = 0;
    bool anyMuted = false;

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

            if (pid == processId || IsEdgeProcess(pid)) {
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
    return anyMuted;  // 只要有一个会话被静音就算成功
}

bool VolumeControl::SetProcessMuteWithTimeout(DWORD processId, bool mute, int timeoutMs) {
    std::atomic<bool> done{ false };
    std::atomic<bool> result{ false };

    std::thread worker([&]() {
        result = SetProcessMute(processId, mute);
        done = true;
        });

    auto start = std::chrono::steady_clock::now();
    while (!done) {
        if (std::chrono::steady_clock::now() - start > std::chrono::milliseconds(timeoutMs)) {
            // 超时分离线程
            worker.detach();
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (worker.joinable()) worker.join();
    return result.load();
}

bool VolumeControl::IsEdgeProcess(DWORD pid)
{
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) return false;
    wchar_t path[MAX_PATH]{};
    DWORD len = MAX_PATH;
    BOOL ok = QueryFullProcessImageNameW(hProc, 0, path, &len);
    CloseHandle(hProc);
    if (!ok || !len) return false;
    const wchar_t* tail = path + len;
    while (tail > path && tail[-1] != L'\\') --tail;
    return (wcsicmp(tail, L"msedge.exe") == 0);
}