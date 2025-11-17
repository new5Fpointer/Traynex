#include "volumecontrol.h"
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

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&deviceEnumerator);
    int sessionCount = 0;
    if (FAILED(hr)) goto cleanup;

    hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (FAILED(hr)) goto cleanup;

    hr = device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&sessionManager);
    if (FAILED(hr)) goto cleanup;

    hr = sessionManager->GetSessionEnumerator(&sessionEnumerator);
    if (FAILED(hr)) goto cleanup;

    sessionEnumerator->GetCount(&sessionCount);

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

            if (pid == processId) {
                hr = control->QueryInterface(&volume);
                if (SUCCEEDED(hr)) {
                    volume->SetMute(mute, nullptr);
                    volume->Release();
                    control2->Release();
                    control->Release();
                    CoUninitialize();
                    return true;
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
    return false;
}