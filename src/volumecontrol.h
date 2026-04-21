#pragma once
#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <vector>
#include <QString>

class VolumeControl {
public:
    static bool SetProcessMute(DWORD processId, bool mute);
    static bool SetProcessMuteWithTimeout(DWORD processId, bool mute, int timeoutMs = 1000);
    static bool SetProcessVolume(DWORD processId, float volume); // 0.0 to 1.0
    static bool SetProcessVolumeWithTimeout(DWORD processId, float volume, int timeoutMs = 1000);
    static float GetProcessVolume(DWORD processId);
    static float GetProcessVolumeWithTimeout(DWORD processId, int timeoutMs = 1000);
private:
    static QString GetExeName(DWORD pid);
};