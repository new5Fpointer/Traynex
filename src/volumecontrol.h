#pragma once
#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <vector>

class VolumeControl {
public:
    static bool SetProcessMute(DWORD processId, bool mute);
};