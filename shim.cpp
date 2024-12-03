#include <windows.h>
#include <shellapi.h>
#include <filesystem>

namespace fs = std::filesystem;

static int exit_code = 0;
static HANDLE h_job = nullptr;
static HANDLE h_process = nullptr;
static HANDLE h_thread = nullptr;

static fs::path os_executable() {
	unsigned int bs = 128;
	std::wstring link_path(bs, wchar_t(0));
	while (true) {
		auto r = GetModuleFileNameW(
			nullptr,
			link_path.data(),
			bs
		);
		if (r < bs) {
			break;
		}
		bs += bs;
		link_path.resize(bs, wchar_t(0));
	}
	return link_path;
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
		return true;
	}
	return bool(HIWORD(hr));
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
	if (!gui) {
		wchar_t* cmd = join_cmd(path, args);
		DWORD ExitCode = 0;
		PROCESS_INFORMATION proc_info{};
		STARTUPINFOW start_info{};
		GetStartupInfoW(&start_info);
		CreateProcessW(
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
		ShellExecuteExW(&exec_info);
		h_process = exec_info.hProcess;
	}
}

static void exit_on_error(fs::path const& lp, std::error_code const& ec) {
	std::fputws(lp.c_str(), stderr);
	std::fputc(10, stderr);
	std::fputs(ec.message().c_str(), stderr);
	std::fputc(10, stderr);
	exit(ec.value());
}


static wchar_t const* resolve_exe_path() {
	auto lp = os_executable();
	std::error_code ec{};
	auto target = fs::read_symlink(lp, ec);
	if (ec) {
		exit_on_error(lp, ec);
	}
	target.replace_filename(lp.filename());
	lp = fs::read_symlink(target, ec);
	if (ec) {
		exit_on_error(target, ec);
	}
	if (!fs::is_regular_file(lp, ec)) {
		exit_on_error(lp, ec);
	}
	return lp.c_str();
}


int wmain(int argc, wchar_t** argv) {
	wchar_t const* args = join_args(argv);
	auto path = resolve_exe_path();

	set_termination_job();
	run_process(path, args, is_gui(path));

	h_process ? CloseHandle(h_process) : 0;
	h_job ? CloseHandle(h_job) : 0;
	h_thread ? CloseHandle(h_thread) : 0;

	return exit_code;
}