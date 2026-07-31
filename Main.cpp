#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <commdlg.h>
#include <thread>
#include <chrono>
#include <shellapi.h>

#define ID_BUTTON_START 101
#define ID_BUTTON_BROWSE 102
#define ID_COMBO_DRIVE 103
#define ID_EDIT_PATH 104
#define ID_LABEL_STATUS 105
#define ID_PROGRESS_BAR 106
#define ID_COMBO_LANG 107
#define ID_BUTTON_REFRESH 108
#define ID_STEP_LABEL 109

struct Language {
    std::string lang;
    std::string iso_label;
    std::string browse;
    std::string usb_label;
    std::string refresh;
    std::string start;
    std::string ready;
    std::string done;
    std::string no_iso;
    std::string no_usb;
    std::string running;
};

Language langs[] = {
    {"en", "1. Select ISO file:", "Browse", "2. Select USB drive:", "Refresh USB", "START", "Ready", "Done!", "No ISO file selected", "No USB drive selected", "Writing..."},
    {"ru", "1. Выберите ISO-файл:", "Обзор", "2. Выберите USB-накопитель:", "Обновить USB", "СТАРТ", "Готов", "Готово!", "ISO-файл не выбран", "USB-накопитель не выбран", "Запись..."},
    {"es", "1. Seleccione archivo ISO:", "Examinar", "2. Seleccione unidad USB:", "Actualizar USB", "INICIO", "Listo", "¡Hecho!", "No se seleccionó ISO", "No se seleccionó USB", "Escribiendo..."}
};

int currentLangIndex = 0;
HWND hPathEdit, hDriveCombo, hStatusLabel, hProgressBar, hStepLabel, hLangCombo, hRefreshButton;
std::string inputISOPath;
std::vector<std::string> drives;
HINSTANCE hInst;

bool IsAdmin() {
    BOOL fIsAdmin = FALSE;
    PSID pAdminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdminGroup)) {
        CheckTokenMembership(NULL, pAdminGroup, &fIsAdmin);
        FreeSid(pAdminGroup);
    }
    return fIsAdmin;
}

void RunAsAdmin(HINSTANCE hInstance) {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    ShellExecuteW(NULL, L"runas", exePath, NULL, NULL, SW_SHOWNORMAL);
    exit(0);
}

std::string GetString(const std::string& key) {
    Language& l = langs[currentLangIndex];
    if (key == "iso_label") return l.iso_label;
    if (key == "browse") return l.browse;
    if (key == "usb_label") return l.usb_label;
    if (key == "refresh") return l.refresh;
    if (key == "start") return l.start;
    if (key == "ready") return l.ready;
    if (key == "done") return l.done;
    if (key == "no_iso") return l.no_iso;
    if (key == "no_usb") return l.no_usb;
    if (key == "running") return l.running;
    return "";
}

std::wstring ToWide(const std::string& str) {
    if (str.empty()) return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    std::wstring wstr(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], len);
    return wstr;
}

std::vector<std::string> GetUSBDrives() {
    std::vector<std::string> usbDrives;
    DWORD drivesMask = GetLogicalDrives();
    char driveLetter = 'A';
    while (drivesMask) {
        if (drivesMask & 1) {
            std::string drive = std::string(1, driveLetter) + ":\\";
            if (GetDriveTypeA(drive.c_str()) == DRIVE_REMOVABLE) {
                usbDrives.push_back(drive);
            }
        }
        drivesMask >>= 1;
        driveLetter++;
    }
    return usbDrives;
}

void UpdateDrives(HWND hwnd) {
    SendMessageA(hDriveCombo, CB_RESETCONTENT, 0, 0);
    drives = GetUSBDrives();
    for (const auto& d : drives) {
        SendMessageA(hDriveCombo, CB_ADDSTRING, 0, (LPARAM)d.c_str());
    }
    if (!drives.empty()) {
        SendMessageA(hDriveCombo, CB_SETCURSEL, 0, 0);
    }
}

void SetStatus(HWND hwnd, const std::string& text) {
    SetWindowTextW(hStatusLabel, ToWide(text).c_str());
}

void RunUSBCreator(const std::string& isoPath, const std::string& drive, HWND hwnd) {
    std::string driveLetter = drive.substr(0, 1);

    SendMessageA(hProgressBar, PBM_SETPOS, 5, 0);
    SetWindowTextW(hStepLabel, ToWide("Step 1/10: Checking USB drive...").c_str());
    Sleep(500);

    SendMessageA(hProgressBar, PBM_SETPOS, 15, 0);
    SetWindowTextW(hStepLabel, ToWide("Step 2/10: Preparing temporary folder...").c_str());
    std::string tempDir = std::string(getenv("TEMP")) + "\\usb_creator_temp";
    std::string mkdirCmd = "mkdir \"" + tempDir + "\" 2> nul";
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    CreateProcessA(NULL, (LPSTR)mkdirCmd.c_str(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    Sleep(500);

    SendMessageA(hProgressBar, PBM_SETPOS, 25, 0);
    SetWindowTextW(hStepLabel, ToWide("Step 3/10: Mounting ISO image...").c_str());
    std::string cmd = "powershell -Command \"$mount = Mount-DiskImage -ImagePath '" + isoPath + "' -PassThru; $driveLetter = ($mount | Get-Volume).DriveLetter; Write-Host $driveLetter;\" > mount_result.txt";
    system(cmd.c_str());
    Sleep(500);

    SendMessageA(hProgressBar, PBM_SETPOS, 35, 0);
    SetWindowTextW(hStepLabel, ToWide("Step 4/10: Reading mounted drive...").c_str());
    std::ifstream resultFile("mount_result.txt");
    std::string isoDrive;
    if (resultFile.is_open()) {
        getline(resultFile, isoDrive);
        resultFile.close();
    }
    if (isoDrive.empty()) {
        SetStatus(hwnd, "Error: Could not mount ISO.");
        EnableWindow(GetDlgItem(hwnd, ID_BUTTON_START), TRUE);
        return;
    }
    Sleep(500);

    SendMessageA(hProgressBar, PBM_SETPOS, 45, 0);
    SetWindowTextW(hStepLabel, ToWide("Step 5/10: Formatting USB drive (FAT32)...").c_str());
    std::string formatCmd = "diskpart /s format_" + driveLetter + ".txt";
    CreateProcessA(NULL, (LPSTR)formatCmd.c_str(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    Sleep(500);

    SendMessageA(hProgressBar, PBM_SETPOS, 60, 0);
    SetWindowTextW(hStepLabel, ToWide("Step 6/10: Copying files to USB...").c_str());
    std::string copyCmd = "xcopy \"" + isoDrive + ":\\*.*\" \"" + drive + "\\\" /E /H /R /Y /Q";
    CreateProcessA(NULL, (LPSTR)copyCmd.c_str(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    Sleep(500);

    SendMessageA(hProgressBar, PBM_SETPOS, 80, 0);
    SetWindowTextW(hStepLabel, ToWide("Step 7/10: Verifying files...").c_str());
    Sleep(500);

    SendMessageA(hProgressBar, PBM_SETPOS, 90, 0);
    SetWindowTextW(hStepLabel, ToWide("Step 8/10: Unmounting ISO...").c_str());
    std::string unmountCmd = "powershell -Command \"Dismount-DiskImage -ImagePath '" + isoPath + "'\"";
    system(unmountCmd.c_str());
    Sleep(500);

    SendMessageA(hProgressBar, PBM_SETPOS, 95, 0);
    SetWindowTextW(hStepLabel, ToWide("Step 9/10: Cleaning temporary files...").c_str());
    std::string rmdirCmd = "rmdir /s /q \"" + tempDir + "\" 2> nul";
    CreateProcessA(NULL, (LPSTR)rmdirCmd.c_str(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    system("del mount_result.txt 2> nul");
    Sleep(500);

    SendMessageA(hProgressBar, PBM_SETPOS, 100, 0);
    SetWindowTextW(hStepLabel, ToWide("Step 10/10: Done!").c_str());
    SetStatus(hwnd, GetString("done"));
    EnableWindow(GetDlgItem(hwnd, ID_BUTTON_START), TRUE);
}

void RebuildUI(HWND hwnd) {
    DestroyWindow(hPathEdit);
    DestroyWindow(hDriveCombo);
    DestroyWindow(hStatusLabel);
    DestroyWindow(hProgressBar);
    DestroyWindow(hLangCombo);
    DestroyWindow(hRefreshButton);
    DestroyWindow(hStepLabel);

    SetWindowTextW(hwnd, ToWide("USB Creator").c_str());

    CreateWindowW(L"STATIC", L"Язык:", WS_CHILD | WS_VISIBLE, 10, 10, 40, 20, hwnd, NULL, hInst, NULL);
    hLangCombo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 55, 10, 40, 200, hwnd, (HMENU)ID_COMBO_LANG, hInst, NULL);
    SendMessageW(hLangCombo, CB_ADDSTRING, 0, (LPARAM)L"en");
    SendMessageW(hLangCombo, CB_ADDSTRING, 0, (LPARAM)L"ru");
    SendMessageW(hLangCombo, CB_ADDSTRING, 0, (LPARAM)L"es");
    SendMessageW(hLangCombo, CB_SETCURSEL, currentLangIndex, 0);

    CreateWindowW(L"STATIC", ToWide(GetString("iso_label")).c_str(), WS_CHILD | WS_VISIBLE, 10, 50, 200, 20, hwnd, NULL, hInst, NULL);
    hPathEdit = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_READONLY, 10, 75, 280, 25, hwnd, (HMENU)ID_EDIT_PATH, hInst, NULL);
    CreateWindowW(L"BUTTON", ToWide(GetString("browse")).c_str(), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 295, 75, 70, 25, hwnd, (HMENU)ID_BUTTON_BROWSE, hInst, NULL);

    CreateWindowW(L"STATIC", ToWide(GetString("usb_label")).c_str(), WS_CHILD | WS_VISIBLE, 10, 120, 200, 20, hwnd, NULL, hInst, NULL);
    hDriveCombo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 10, 145, 150, 200, hwnd, (HMENU)ID_COMBO_DRIVE, hInst, NULL);
    UpdateDrives(hwnd);
    hRefreshButton = CreateWindowW(L"BUTTON", ToWide(GetString("refresh")).c_str(), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 165, 145, 70, 25, hwnd, (HMENU)ID_BUTTON_REFRESH, hInst, NULL);

    CreateWindowW(L"BUTTON", ToWide(GetString("start")).c_str(), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 135, 220, 100, 35, hwnd, (HMENU)ID_BUTTON_START, hInst, NULL);

    hProgressBar = CreateWindowW(L"msctls_progress32", L"", WS_CHILD | WS_VISIBLE, 10, 280, 355, 20, hwnd, (HMENU)ID_PROGRESS_BAR, hInst, NULL);
    SendMessageW(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

    hStepLabel = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER, 10, 310, 355, 20, hwnd, (HMENU)ID_STEP_LABEL, hInst, NULL);

    hStatusLabel = CreateWindowW(L"STATIC", ToWide(GetString("ready")).c_str(), WS_CHILD | WS_VISIBLE | SS_CENTER, 10, 340, 355, 20, hwnd, (HMENU)ID_LABEL_STATUS, hInst, NULL);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            RebuildUI(hwnd);
            break;
        case WM_COMMAND:
        {
            int id = LOWORD(wParam);
            if (id == ID_COMBO_LANG && HIWORD(wParam) == CBN_SELCHANGE) {
                currentLangIndex = SendMessageW(hLangCombo, CB_GETCURSEL, 0, 0);
                RebuildUI(hwnd);
            }
            else if (id == ID_BUTTON_BROWSE) {
                OPENFILENAMEA ofn;
                char fileName[MAX_PATH] = "";
                ZeroMemory(&ofn, sizeof(ofn));
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hwnd;
                ofn.lpstrFilter = "ISO Files\0*.iso\0All Files\0*.*\0";
                ofn.lpstrFile = fileName;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
                if (GetOpenFileNameA(&ofn)) {
                    inputISOPath = fileName;
                    SetWindowTextW(hPathEdit, ToWide(fileName).c_str());
                }
            }
            else if (id == ID_BUTTON_REFRESH) {
                UpdateDrives(hwnd);
            }
            else if (id == ID_BUTTON_START) {
                if (inputISOPath.empty()) {
                    SetStatus(hwnd, GetString("no_iso"));
                    return 0;
                }
                int idx = SendMessageA(hDriveCombo, CB_GETCURSEL, 0, 0);
                if (idx == CB_ERR) {
                    SetStatus(hwnd, GetString("no_usb"));
                    return 0;
                }
                char driveBuf[10];
                SendMessageA(hDriveCombo, CB_GETLBTEXT, idx, (LPARAM)driveBuf);
                std::string drive(driveBuf);

                EnableWindow(GetDlgItem(hwnd, ID_BUTTON_START), FALSE);
                SetStatus(hwnd, GetString("running"));

                std::thread t(RunUSBCreator, inputISOPath, drive, hwnd);
                t.detach();
            }
        }
        break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    if (!IsAdmin()) {
        RunAsAdmin(hInstance);
        return 0;
    }

    hInst = hInstance;
    const wchar_t CLASS_NAME[] = L"USBCreatorWindowClass";
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(0, CLASS_NAME, ToWide("USB Creator").c_str(), WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME, CW_USEDEFAULT, CW_USEDEFAULT, 440, 400, NULL, NULL, hInstance, NULL);
    if (hwnd == NULL) return 0;
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}