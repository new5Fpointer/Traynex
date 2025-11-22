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
private:
    static QString GetExeName(DWORD pid);
};