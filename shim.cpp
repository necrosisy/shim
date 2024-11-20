#include <windows.h>
#include <shellapi.h>
#include <filesystem>

namespace fs = std::filesystem;

static int exit_code = 0;
static HANDLE h_job = nullptr;
static HANDLE h_process = nullptr;
static HANDLE h_thread = nullptr;

static wchar_t const* os_executable() {
	unsigned int bs = 1 << 6;
	wchar_t* buf = nullptr;
	while (true) {
		buf = new wchar_t[bs]();
		DWORD r = GetModuleFileNameW(
			nullptr,
			buf,
			bs
		);
		if (r < bs) {
			break;
		}
		delete[] buf;
		bs += bs;
	}

	return buf;
}

static wchar_t const* join_args(wchar_t const* const* argv) {
	wchar_t const* first_arg = argv[0];
	wchar_t const* cmd_line = GetCommandLineW();
	wchar_t const* param = cmd_line;
	int space_count = 0;
	for (wchar_t const* c = first_arg; *c != L'\0'; c++) {
		if (*c == L' ') {
			space_count++;
		}
	}
	while (*param) {
		if (*param == L' ') {
			if (space_count) {
				space_count--;
			}
			else {
				break;
			}
		}
		param++;
	}
	return param;
}

static wchar_t* join_cmd(wchar_t const* path, wchar_t const* args) {
	size_t l1 = std::wcslen(path);
	size_t l2 = std::wcslen(args);
	auto cmd = new wchar_t[l1 + l2 + 4]();
	cmd[0] = L'\"';
	cmd[l1 + 1] = L'\"';
	cmd[l1 + 2] = L' ';
	std::memcpy(cmd + 1, path, l1 * sizeof(wchar_t));
	std::memcpy(cmd + l1 + 3, args, l2 * sizeof(wchar_t));
	return cmd;
}

static bool is_gui(wchar_t const* real_path) {
	SHFILEINFOW fi{};
	DWORD_PTR hr = SHGetFileInfoW(
		real_path,
		0,
		&fi,
		sizeof(fi),
		SHGFI_EXETYPE
	);
	if (!hr) {
		//std::fputs("filesystem_error", stderr);
		exit(-1);
	}
	return (bool)(BOOL)HIWORD(hr);
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

static void run_process(wchar_t const* path, wchar_t const* args, bool gui) {
	BOOL r = TRUE;
	if (!gui) {
		wchar_t* cmd = join_cmd(path, args);
		DWORD ExitCode = 0;
		PROCESS_INFORMATION proc_info{};
		STARTUPINFOW start_info{};
		GetStartupInfoW(&start_info);
		r = CreateProcessW(
			path,
			cmd,
			nullptr,
			nullptr,
			true,
			CREATE_SUSPENDED,
			nullptr,
			nullptr,
			&start_info,
			&proc_info
		);
		delete[] cmd;
		h_process = proc_info.hProcess;
		h_thread = proc_info.hThread;
		ResumeThread(h_thread);
		SetConsoleCtrlHandler(nullptr, TRUE);
		AssignProcessToJobObject(h_job, h_process);
		WaitForSingleObject(h_process, INFINITE);
		GetExitCodeProcess(h_process, &ExitCode);
		exit_code = (int)ExitCode;
	}
	else {
		FreeConsole();
		SHELLEXECUTEINFOW exec_info{};
		exec_info.cbSize = sizeof(exec_info);
		exec_info.fMask = SEE_MASK_NOCLOSEPROCESS;
		exec_info.nShow = SW_SHOW;
		exec_info.lpFile = path;
		exec_info.lpParameters = args;
		exec_info.lpVerb = L"open";
		r = ShellExecuteExW(&exec_info);
		h_process = exec_info.hProcess;
	}
	if (!r) {
		//std::fputs("System_error", stderr);
		exit_code = -1;
	}
}

static fs::path resolve_exe_path() {
	wchar_t const* link_path = os_executable();
	fs::path fs_link_path(link_path);
	delete[] link_path;

	if (!fs::is_symlink(fs_link_path)) {
		//std::fputs("symlink_status", stderr);
		exit(-1);
	}

	auto target = fs::read_symlink(fs_link_path);
	target.replace_filename(fs_link_path.filename());
	if (!fs::is_symlink(target)) {
		//std::fputs("symlink_status", stderr);
		exit(-1);
	}

	return fs::read_symlink(target);
}


int wmain(int argc, wchar_t** argv) {
	wchar_t const* args = join_args(argv);
	auto path = resolve_exe_path();
	auto cpath = path.c_str();

	set_termination_job();
	run_process(cpath, args, is_gui(cpath));

	h_process ? CloseHandle(h_process) : 0;
	h_job ? CloseHandle(h_job) : 0;
	h_thread ? CloseHandle(h_thread) : 0;

	return exit_code;
}