#include <shlwapi.h>
#include <shellapi.h>
#include <windows.h>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

static HANDLE h_job = nullptr;
static HANDLE h_process = nullptr;
static HANDLE h_thread = nullptr;

static std::wstring os_executable() {
    unsigned int buf_size = 128;
    std::wstring symlink_path(buf_size, wchar_t(0));
    while (true) {
        auto length = GetModuleFileNameW(
            nullptr,
            symlink_path.data(),
            buf_size
        );
        if (length < buf_size) {
            break;
        }
        buf_size += buf_size;
        symlink_path.resize(buf_size);
    }
    return symlink_path;
}

[[noreturn]]
static void exit_on_error(std::wstring const& file_path, std::error_code const& ec) {
    std::wcout << file_path << wchar_t(10);
    std::cout << ec.message() << char(10);
    ExitProcess(EXIT_FAILURE);
}


static std::wstring resolve_symlink(fs::path const& symlink_path) {
    std::error_code ec{};
    auto target_path = fs::read_symlink(symlink_path, ec);
    if (ec) {
        exit_on_error(symlink_path, ec);
    }
    target_path.replace_filename(symlink_path.filename());
    auto exe_path = fs::read_symlink(target_path, ec);
    if (ec) {
        exit_on_error(target_path, ec);
    }
    if (!fs::is_regular_file(exe_path, ec)) {
        exit_on_error(exe_path, ec);
    }
    return exe_path;
}

static bool is_gui_application(std::wstring const& exe_path) {
    SHFILEINFOW sfi{};
    auto hr = SHGetFileInfoW(
        exe_path.data(),
        0,
        &sfi,
        sizeof(SHFILEINFOW),
        SHGFI_EXETYPE
    );
    return LOWORD(hr) == 0 || HIWORD(hr) != 0;
}

static void set_termination_job() {
    h_job = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_info{};
    job_info.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
    SetInformationJobObject(
        h_job,
        JobObjectExtendedLimitInformation,
        &job_info,
        sizeof(job_info)
    );
}

[[noreturn]]
static void cleanup_and_exit(unsigned int exit_code) {
    h_process ? CloseHandle(h_process) : 0;
    h_job ? CloseHandle(h_job) : 0;
    h_thread ? CloseHandle(h_thread) : 0;
    ExitProcess(exit_code);
}


[[noreturn]]
static void start_gui_app(std::wstring const& exe_path, std::wstring& cmd) {
    SHELLEXECUTEINFOW exec_info{};
    exec_info.cbSize = sizeof(exec_info);
    exec_info.fMask = SEE_MASK_NOCLOSEPROCESS;
    exec_info.nShow = SW_SHOW;
    exec_info.lpFile = exe_path.data();
    exec_info.lpParameters = PathGetArgsW(cmd.data());
    exec_info.lpVerb = L"open";
    auto result = ShellExecuteExW(&exec_info);
    h_process = exec_info.hProcess;
    result ? cleanup_and_exit(EXIT_SUCCESS) : cleanup_and_exit(EXIT_FAILURE);
}

[[noreturn]]
static void start_console_app(std::wstring const& exe_path, std::wstring& cmd) {
    DWORD ExitCode{ EXIT_FAILURE };
    PROCESS_INFORMATION proc_info{};
    STARTUPINFOW start_info{};
    GetStartupInfoW(&start_info);
    auto result = CreateProcessW(
        exe_path.data(),
        cmd.data(),
        nullptr,
        nullptr,
        true,
        CREATE_SUSPENDED,
        nullptr,
        nullptr,
        &start_info,
        &proc_info
    );
    result ? void(0) : cleanup_and_exit(EXIT_FAILURE);
    h_process = proc_info.hProcess;
    h_thread = proc_info.hThread;
    ResumeThread(h_thread);
    SetConsoleCtrlHandler(nullptr, TRUE);
    AssignProcessToJobObject(h_job, h_process);
    WaitForSingleObject(h_process, INFINITE);
    GetExitCodeProcess(h_process, &ExitCode);
    cleanup_and_exit(ExitCode);
}

int main() {
    std::wstring cmd = GetCommandLineW();
    auto exe_path = resolve_symlink(os_executable());
    set_termination_job();
    if (!is_gui_application(exe_path)) {
        start_console_app(exe_path, cmd);
    }
    else {
        FreeConsole();
        start_gui_app(exe_path, cmd);
    }
}

