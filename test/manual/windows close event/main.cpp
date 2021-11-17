
#include <iostream>

#include <windows.h>
#include <tlhelp32.h>

#include <memory>
#include <vector>

PROCESSENTRY32W getProcess(std::wstring_view name) {

    std::unique_ptr<HANDLE,decltype(&CloseHandle)> snapShot(new HANDLE, &CloseHandle);
    *snapShot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);

    PROCESSENTRY32W pEntry = {};
    pEntry.dwSize = sizeof( pEntry );
    BOOL ok = Process32FirstW(*snapShot, &pEntry);
    while (ok)
    {
        if (name == pEntry.szExeFile)
        {
            std::wcout << pEntry.szExeFile << " " << pEntry.th32ProcessID << std::endl;
            return pEntry;
        }
        ok = Process32NextW(*snapShot, &pEntry);
    }
    return {};
}

std::wstring getWindowTitle(HWND hwnd) {
    std::wstring buf(1024, '\0');
    const auto size = GetWindowTextW(hwnd, buf.data(), buf.size());
    if (!size) {
        return {};
    }
    buf.resize(size);
    return buf;
}

struct WindowData {
    WindowData(DWORD pid)
        : pid(pid)
    {}
    DWORD pid;
    std::vector<HWND> windows;
};

WindowData getHWND(DWORD pid) {
    WindowData data(pid);
    EnumWindows([](HWND hwnd,LPARAM lParam) -> BOOL{
        WindowData* data = reinterpret_cast<WindowData*>(lParam);
        DWORD lpdwProcessId;
        GetWindowThreadProcessId(hwnd,&lpdwProcessId);
        if(lpdwProcessId==data->pid)
        {
            data->windows.push_back(hwnd);
            std::wcout << data->pid << " " << hwnd << std::endl;
        }
        return true;
    }, reinterpret_cast<LPARAM>(&data));
    return data;
}

WindowData getHWND(std::wstring_view name) {
    std::wcout << name << std::endl;
    auto process = getProcess(name);
    return getHWND(process.th32ProcessID);
}

template <class T>
void broadcast(const WindowData &dest, UINT msg, WPARAM wParam,T lParam) {
    for (const auto w : dest.windows) {
        auto result = SendMessageW(w, msg, wParam, reinterpret_cast<LPARAM>(&lParam));
        std::wcout << "Window: " << getWindowTitle(w) << " HWND: " << w << " Result: " << result << std::endl;
    }
}

int main()
{
    const auto commandLine = GetCommandLineW();
    int argc;
    wchar_t **argv = CommandLineToArgvW(commandLine, &argc);

    const auto process = getHWND(argc == 2 ? argv[1] : L"owncloud.exe");
//    const auto process = getHWND(L"WINDOW_TEST.exe");

    std::wcout << "Query: WM_QUERYENDSESSION" << std::endl;
    broadcast(process, WM_QUERYENDSESSION, 0, ENDSESSION_CLOSEAPP);
    std::wcout << "Kill: WM_ENDSESSION" << std::endl;
    broadcast(process, WM_ENDSESSION, 1, ENDSESSION_CLOSEAPP);
    return 0;
}
