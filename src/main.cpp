#include <cmath>
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <thread>
#include <sstream>

#include "stdafx.h"

#pragma region DOWNLOADER

size_t WRITE_CALLBACK(void *contents, const size_t size, const size_t nmemb, std::string *out) {
    out->append(static_cast<char *>(contents), size * nmemb);

    return size * nmemb;
}

size_t WRITE_FILE_CALLBACK(const void *ptr, const size_t size, const size_t nmemb, FILE *stream) {
    const size_t written = fwrite(ptr, size, nmemb, stream);

    return written;
}

int PROGRESS_CALLBACK(void *ptr, const curl_off_t dltotal, const curl_off_t dlnow, const curl_off_t ultotal,
                      const curl_off_t ulnow) {
    if (dltotal <= 0) {
        return 0;
    }

    const auto *data = static_cast<ProgressData *>(ptr);
    const double percentage = static_cast<double>(dlnow) / static_cast<double>(dltotal) * 100.0;
    const auto now_time = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsed = now_time - data->start_time;
    const double bytes_per_sec = (elapsed.count() > 0) ? (static_cast<double>(dlnow) / elapsed.count()) : 0.0;
    const std::string bar = progress_bar(50, percentage);
    const std::string current_size = format(static_cast<double>(dlnow));
    const std::string total_size = format(static_cast<double>(dltotal));
    const std::string speed_str = format(bytes_per_sec, true);

    std::cout << "\r[" << GREEN << "+" << RESET << "] "
            << std::left << std::setw(15) << data->product << " "
            << "[" << CYAN << bar << RESET << "] "
            << std::fixed << std::setprecision(1) << std::right << std::setw(5)
            << percentage << "% "
            << "(" << std::setw(8) << current_size << " / " << std::setw(8) << total_size << ") "
            << YELLOW << std::setw(10) << speed_str << RESET << "    " << std::flush;

    return 0;
}

std::string get(const std::string &url) {
    CURL *curl = curl_easy_init();

    if (!curl) {
        return "";
    }

    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WRITE_CALLBACK);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    return response;
}

bool get(const std::string &url, const std::string &path, const std::string &product) {
    CURL *curl = curl_easy_init();

    if (!curl) {
        return false;
    }

    FILE *file;

#ifdef _WIN32
    if (const errno_t err = fopen_s(&file, path.c_str(), "wb"); err != 0) {
        file = nullptr;
    }
#else
    file = fopen(path.c_str(), "wb");
#endif

    if (!file) {
        curl_easy_cleanup(curl);

        return false;
    }

    ProgressData p_data{product, std::chrono::steady_clock::now()};

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WRITE_FILE_CALLBACK);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, PROGRESS_CALLBACK);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &p_data);

    const CURLcode response = curl_easy_perform(curl);

    fclose(file);
    curl_easy_cleanup(curl);

    std::cout << std::endl;

    if (response != CURLE_OK) {
        std::cerr << RED << std::endl << "Download failed: " << curl_easy_strerror(response) << RESET << std::endl;
        std::filesystem::remove(path);
        return false;
    }

    return true;
}

std::vector<List> extract(const std::string &string) {
    using json = nlohmann::json;
    std::vector<List> lists;

    for (auto data = json::parse(string); auto &[code, releases]: data.items()) {
        if (!releases.is_array() || releases.empty()) {
            continue;
        }

        const json &latest = releases[0];

        if (!latest.contains("downloads")) {
            continue;
        }

        const auto &download = latest["downloads"];

        if (!download.contains("linux") || !download.contains("windows")) {
            continue;
        }

        List item;
        item.product = get_product_name(code);
        item.code = code;
        item.version = latest.value("version", "unknown");
        item.build = latest.value("build", "unknown");
        item.date = latest.value("date", "unknown");

#ifdef __linux__
        Download linux;
        linux.link = download["linux"].value("link", "");
        linux.size = download["linux"].value("size", 0LL);
        linux.checksum = download["linux"].value("checksum", "");

        item.downloads["linux"] = linux;
#else
        Download windows;
        windows.link = download["windows"].value("link", "");
        windows.size = download["windows"].value("size", 0LL);
        windows.checksum = download["windows"].value("checksum", "");

        item.downloads["windows"] = windows;
#endif
        lists.push_back(std::move(item));
    }

    return lists;
}

#pragma endregion

#pragma region COMMAND_PARSER

std::map<std::string_view, std::function<void(const std::vector<std::string_view>)> > commands;

void add(const std::string_view flag, std::function<void(const std::vector<std::string_view>)> args) {
    commands[flag] = std::move(args);
}

void alias(const std::string_view a, const std::string_view b) {
    commands[a] = commands[b];
}

void parse(const int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::string_view arg = argv[i]; commands.contains(arg)) {
            std::vector<std::string_view> params;

            while (i + 1 < argc) {
                if (commands.contains(argv[i + 1])) {
                    break;
                }
                params.emplace_back(argv[++i]);
            }

            commands[arg](params);
        } else {
            std::cerr << GREEN << "Unknown Option" << RESET << ": " << RED << arg << RESET << std::endl;
        }
    }
}

#pragma endregion

#pragma region JB_RESET

std::string home() {
#ifdef _WIN32
    // const char* user = getenv("USERPROFILE");
    // return user ? std::string(user) : "";

    PWSTR temp;

    if (const HRESULT result = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &temp); SUCCEEDED(result)) {
        const std::filesystem::path path(temp);
        CoTaskMemFree(temp);

        return path.string();
    }

    return "";
#else
    const char *home = getenv("HOME");
    return home ? std::string(home) : "";
#endif
}

std::string_view trim(const std::string_view string) {
    const auto start = string.find_first_not_of(" \t\r\n");

    if (start == std::string_view::npos) {
        return "";
    }

    const auto end = string.find_last_not_of(" \t\r\n");

    return string.substr(start, end - start + 1);
}

std::optional<long long> timestamp(const std::string &content) {
    const std::regex pattern(R"jb("trial\.state\.free\.trial\.expiration\.date"\s*:\s*"(\d+)")jb");

    if (std::smatch match; std::regex_search(content, match, pattern) && match.size() > 1) {
        try {
            return std::stoll(match.str(1));
        } catch (...) {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

std::string clean(const std::string &content) {
    std::stringstream stream(content);
    std::string line, result;
    bool skip = false;

    while (std::getline(stream, line)) {
        std::string_view trimmed = trim(line);

        const bool val =
                (trimmed.find("evlsprt") != std::string_view::npos) ||
                (trimmed.find("trial.state") != std::string_view::npos) ||
                (trimmed.find("trial.") != std::string_view::npos &&
                 trimmed.find("availability") != std::string_view::npos);

        if (val) {
            skip = true;
            continue;
        }

        if (skip && (trimmed == "," || trimmed.empty())) {
            skip = false;
            continue;
        }

        skip = false;
        result += line + "\n";
    }

    return result;
}

void reset(const std::filesystem::path &product) {
    std::cout << "[" << YELLOW << "*" << RESET << "] Resetting " << GREEN << product.filename() << RESET << std::endl;

#ifdef __linux__
    if (std::filesystem::exists(std::filesystem::path(product / "eval"))) {
        std::filesystem::remove_all(std::filesystem::path(product / "eval"));

        std::cout << "[" << GREEN << "+" RESET "] Deleted eval folder" << std::endl;
    }
#endif

    if (std::filesystem::exists(std::filesystem::path(product / "options" / "other.xml"))) {
        std::ifstream in(std::filesystem::path(product / "options" / "other.xml"));
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();

        if (std::string cleaned = clean(content); content.length() != cleaned.length()) {
            std::ofstream out(std::filesystem::path(product / "options" / "other.xml"));

            out << cleaned;

            std::cout << "[" << GREEN "+" << RESET << "] Patched " << GREEN << "other.xml" << RESET << std::endl;
        } else {
            std::cout << GREEN << product.filename() << RESET << " is already patched!" << std::endl;
        }
    }
#ifdef _WIN32
    if (const std::filesystem::path key(product / "clion.key"); std::filesystem::exists(key)) {
        if (std::filesystem::remove(key)) {
            std::cout << "[" << GREEN "+" << RESET << "] Removed Trial License Key " << GREEN << key.filename() << RESET
                    << std::endl;
        } else {
            std::cout << GREEN << key.filename() << RESET << " is already deleted!" << std::endl;
        }
    }
#endif
}

void purge() {
    for (const std::filesystem::path path = std::filesystem::path(home()) / "JetBrains"; const auto &entry:
         std::filesystem::recursive_directory_iterator(path)) {
        if (entry.is_regular_file()) {
            std::string stem = entry.path().stem().string();

            if (const auto it = std::ranges::find(JETBRAINS_IDS_FILE, stem); it != JETBRAINS_IDS_FILE.end()) {
                if (std::filesystem::remove(entry.path())) {
                    std::cout << "[" << GREEN << "+" << RESET << "] Purged File '" << GREEN << entry.path().filename()
                            << RESET << "'." << std::endl;
                }
            }
        }
    }

#ifdef _WIN32
    std::cout << "[" << YELLOW << "*" << RESET << "] Purging Registry Keys." << std::endl;

    for (const auto &target: JETBRAINS_REGISTRY_PATH) {
        if (const LSTATUS status = RegDeleteTreeA(HKEY_CURRENT_USER, target.data()); status == ERROR_SUCCESS) {
            std::cout << "[" << GREEN << "+" << RESET << "] Purged Registry  Key '" << GREEN << target << RESET << "'."
                    << std::endl;
        } else if (status == ERROR_FILE_NOT_FOUND) {
            std::cout << "[" << YELLOW << "*" << RESET << "] Registry Key already cleaned." << std::endl;
        } else {
            std::cout << "[" << RED << "X" << RESET << "] Could not delete '" << GREEN << target << RESET << "' Code: "
                    << YELLOW << status << RESET << "." << std::endl;
        }
    }
#else
    if (std::filesystem::exists(std::filesystem::path(home()) / ".java" / ".userPrefs" / "jetbrains")) {
        std::filesystem::remove_all(std::filesystem::path(home()) / ".java" / ".userPrefs" / "jetbrains");

        std::cout << "[" << GREEN << "+" << RESET << "] Purged global Java preferences" << std::endl;
    }
#endif
}

void list(const std::filesystem::path &base) {
    if (!std::filesystem::exists(base)) {
        std::cerr << "Directory doesn't exists! '" << base << "'" << std::endl;
        return;
    }

    std::cout << std::left
            << std::setw(30) << GREEN "PRODUCT" RESET
            << std::setw(20) << GREEN "STATUS" RESET
            << GREEN "DAYS LEFT" RESET << std::endl;

    std::cout << std::string(65, '-') << std::endl;

    for (const auto &entry: std::filesystem::directory_iterator(base)) {
#ifdef _WIN32
        if (!entry.is_directory() || entry.path().filename() == "consentOptions" || entry.path().filename() == "acp-agents")
#else
        if (!entry.is_directory())
#endif
        {
            continue;
        }


        if (std::filesystem::path xml = entry.path() / "options" / "other.xml"; std::filesystem::exists(xml)) {
            std::ifstream in(xml);
            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

            if (const auto time = timestamp(content)) {
                std::string name = GREEN + entry.path().filename().string() + RESET;

                std::cout << std::left << std::setw(30) << name;

                const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                const long long diff_ms = *time - now_ms;
                const double precise_days = static_cast<double>(diff_ms) / 86400000.0;

                if (const int remaining_days = static_cast<int>(std::ceil(precise_days)) - 3; remaining_days > 0) {
                    std::cout << std::setw(20) << GREEN "ACTIVE" RESET << YELLOW << remaining_days << RESET <<
                            std::endl;
                } else {
                    std::cout << std::setw(20) << RED << "EXPIRED" << "N/A" << RESET << std::endl;
                }
            } else {
                std::cout << std::setw(30) << GREEN "eval" RESET << std::setw(20) << (
                    std::filesystem::exists(entry.path() / "eval") ? "ACTIVE" : "EXPIRED") << "N/A" << std::endl;
            }
        } else {
            std::cout << std::setw(20) << "No config" << "N/A" << std::endl;
        }
    }
}

#pragma endregion

int main(const int argc, char **argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);

    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(hStdin, &mode);
    mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
    mode |= ENABLE_PROCESSED_INPUT;
    SetConsoleMode(hStdin, mode);
    FlushConsoleInputBuffer(hStdin);

    CONSOLE_CURSOR_INFO ci;
    ci.bVisible = FALSE;
    ci.dwSize = 1;
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ci);
#else
    static struct termios oldt, newt;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    std::cout << "\033[?25l" << std::flush;

#endif
#ifndef DEBUG
    if (const auto running = running_products(JETBRAINS_TARGET_EXECUTABLES); !running.empty()) {
        std::cout << "Please Close the Following Processes Before Continuing." << std::endl << std::endl;

        for (const auto &executable: running) {
            std::cout << RED << executable << RESET << std::endl;
        }

        return EXIT_FAILURE;
    }
#endif

#ifdef _WIN32
    std::filesystem::path base = std::filesystem::path(home()) / "JetBrains";
#else
    std::filesystem::path base = std::filesystem::path(home()) / ".config" / "JetBrains";
#endif

    add("-h", [&](const auto &args) {
        constexpr int width = 35;

        std::cout << YELLOW << "Usage: " << WHITE << "jb-reset " << CYAN << "[options]" << RESET << std::endl <<
                std::endl;
        std::cout << std::left
                << std::setw(50) << (GREEN "  COMMAND" RESET)
                << (GREEN "DESCRIPTION" RESET) << std::endl;

        for (int i = 0; i < 70; ++i) {
            std::cout << "─";
        }

        std::cout << std::endl;

        auto print = [&](const std::string &cmd, const std::string &desc) {
            std::cout << std::left << std::setw(width) << cmd << desc << std::endl;
        };

        print("  -h, --help", "Show available commands.");

#ifdef DEBUG
        print("  -t, --test", "Run internal test suite.");
#endif

        print("  -s, --show", "Show installed products and trial status.");
        print("  -i, --install <name/code>", "Download and install a JetBrains product.");
        print("  -l, --list --online", "List local products or fetch online releases.");
        print("  -r, --reset <name|all>", "Reset the evaluation period for products.");

        std::cout << std::endl << YELLOW << "Example:" << RESET << " jb-reset -i clion" << std::endl;
    });

#ifdef DEBUG
    // test
    add("-t", [&base](const auto &args) {
        std::cout << "User Profile: " << home() << std::endl;
        std::cout << "JetBrains Path: " << base.string() << std::endl;

        for (const auto &entry: std::filesystem::directory_iterator(base)) {
#ifdef __linux__
            if (entry.is_directory()) {
#else
                if (entry.is_directory() && entry.path().filename() != "consentOptions") {
#endif
                std::cout << "Products: " << entry.path().filename().string() << std::endl;
            }
        }

#ifdef _WIN32
        for (const std::filesystem::path path = std::filesystem::path(home()) / "JetBrains"; const auto &entry:
             std::filesystem::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                std::string stem = entry.path().stem().string();

                if (const auto it = std::ranges::find(JETBRAINS_IDS_FILE, stem); it != JETBRAINS_IDS_FILE.end()) {
                    if (std::filesystem::remove(entry.path())) {
                        std::cout << "File: " << entry.path().filename().string() << std::endl;
                    }
                }
            }
        }

        for (const auto &entry: JETBRAINS_REGISTRY_PATH) {
            HKEY key;

            if (const LSTATUS status = RegOpenKeyExA(HKEY_CURRENT_USER, entry.data(), 0, KEY_READ, &key);
                status == ERROR_SUCCESS) {
                std::cout << "Registry Key: 'Computer\\HKEY_CURRENT_USER\\" << entry << "'." << std::endl;

                RegCloseKey(key);
            }
        }
#endif
    });
#endif

    add("-s", [&base](const auto &args) {
        list(base);
    });

    add("-i", [&](const auto &args) {
        if (args.empty()) {
            std::cerr << "Usage: -i <product>" << std::endl;

            return;
        }

        const std::string response = get(
            "https://data.services.jetbrains.com/products/releases?code=IIU,PCP,WS,PS,CL,GO,RD,DG,RM,RR,AS,FL&latest=true&type=release");

        if (response.empty()) {
            std::cerr << "Failed to Fetch Data." << std::endl;

            return;
        }

        const auto list = extract(response);

        const std::string target = toLower(std::string(args[0]));
        bool found = false;
        std::string url;
        std::string name;

        for (const auto &[product, code, version, build, date, downloads]: list) {
            if (toLower(product).find(target) != std::string::npos || toLower(code) == target) {
                found = true;
                name = product;
#ifdef _WIN32
                if (downloads.contains("windows")) {
                    url = downloads.at("windows").link;
                }
#else
                if (downloads.contains("linux")) {
                    url = downloads.at("linux").link;
                }
#endif

                break;
            }
        }

        if (found && !url.empty()) {
            const std::string filename = url.substr(url.find_last_of("/\\") + 1);
#ifdef _WIN32
            if (!IS_PRODUCT_INSTALLED(name)) {
#else
                std::filesystem::path local = std::filesystem::path(home()) / ".local" / "share" / "JetBrains" / name;
                if (!std::filesystem::exists(local)) {
#endif
                if (get(url, filename, name)) {
#ifdef _WIN32
                    std::cout << std::endl << "[" << YELLOW << "*" << RESET << "] Launching installer for " << name <<
                            "." << std::endl;

                    SHELLEXECUTEINFOA shell = {sizeof(shell)};
                    shell.fMask = SEE_MASK_NOCLOSEPROCESS;
                    shell.lpVerb = "open";
                    shell.lpFile = filename.c_str();
                    shell.nShow = SW_SHOWNORMAL;

                    if (ShellExecuteExA(&shell)) {
                        std::cout << "[" << YELLOW << "*" << RESET << "] Waiting for installation to complete." <<
                                std::endl;

                        WaitForSingleObject(shell.hProcess, INFINITE);
                        CloseHandle(shell.hProcess);
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));

                        try {
                            std::filesystem::remove(filename);

                            std::cout << "[" << GREEN << "+" << RESET << "] Installer cleaned up successfully." <<
                                    std::endl;
                        } catch (const std::filesystem::filesystem_error &e) {
                            std::cerr << RED << "[-] Cleanup failed: " << e.what() << RESET << std::endl;
                            std::cerr << YELLOW << "[!] Manual cleanup required: " << filename << RESET << std::endl;
                        }
                    } else {
                        std::cout << "[" << GREEN << "+" << RESET << "] Installer process started." << std::endl;
                    }
#else
                    std::cout << std::endl << "[" << YELLOW << "*" << RESET << "] Extracting " << filename << "." <<
                            std::endl;

                    std::filesystem::path path = std::filesystem::path(home()) / ".local" / "share" / "JetBrains";
                    std::filesystem::create_directories(path);
                    std::string query = "tar -xzf " + filename + " -C " + path.string() + " --strip-components=1";

                    const int ret = std::system(query.c_str());

                    if (ret == 0) {
                        std::cout << "[" << GREEN << "+" << RESET << "] Extracted to: " << path << std::endl;

                        try {
                            std::filesystem::remove(filename);

                            std::cout << "[" << GREEN << "+" << RESET << "] Cleaned up archive." << std::endl;
                        } catch (const std::filesystem::filesystem_error &e) {
                            std::cerr << RED << "[-] Cleanup failed: " << e.what() << RESET << std::endl;
                            std::cerr << YELLOW << "[!] Manual cleanup required: " << filename << RESET << std::endl;
                        }
                    } else {
                        std::cerr << RED << "[" << RED << "-" << RESET << "] Extraction failed." << RESET << std::endl;
                    }
#endif
                } else {
                    std::cerr << RED << "Could not save the file." << RESET << std::endl;
                }
            } else {
                std::cerr << name << " is already Installed." << std::endl;
            }
        } else if (!found) {
            std::cerr << "No product found matching: " << target << std::endl;
        }
    });

    add("-l", [&](const auto &args) {
        if (args[0] == "--online") {
            const std::string response = get(
                "https://data.services.jetbrains.com/products/releases?code=IIU,PCP,WS,PS,CL,GO,RD,DG,RM,RR,AS,FL&latest=true&type=release");

            if (response.empty()) {
                std::cerr << "Failed to Fetch Data." << std::endl;
            }

            const auto list = extract(response);

            std::cout << "╭────────────────────┬────────┬────────────┬────────────────┬────────────╮" << std::endl;
            std::cout << "│      Product       │  Code  │   Version  │     Build      │    Date    │" << std::endl;
            std::cout << "├────────────────────┼────────┼────────────┼────────────────┼────────────┤" << std::endl;

            for (int i = 0; i < list.size(); i++) {
                const auto &[product, code, version, build, date, downloads] = list[i];
                auto colored_name = std::string(GREEN) + product + RESET;
                auto colored_code = std::string(YELLOW) + code + RESET;

                std::cout << "│ "
                        << padding(colored_name, 23) << " │ "
                        << padding(colored_code, 11) << " │ "
                        << padding(version, 10) << " │ "
                        << padding(build, 14) << " │ "
                        << padding(date, 10) << " │" << std::endl;

                if (i + 1 != list.size()) {
                    std::cout << "├────────────────────┼────────┼────────────┼────────────────┼────────────┤" <<
                            std::endl;
                }
            }

            std::cout << "╰────────────────────┴────────┴────────────┴────────────────┴────────────╯" << std::endl;
        }
    });

    add("-r", [&](const auto &args) {
        if (args.empty()) {
            return;
        }

        if (const std::string_view sub = args[0]; sub == "all" || sub == "--all") {
            for (const auto &entry: std::filesystem::directory_iterator(base)) {
#ifdef _WIN32
                if (entry.is_directory() || entry.path().filename() != "consentOptions")
#else
                if (entry.is_directory())
#endif
                {
                    reset(entry.path());
                }
            }

            purge();
        } else {
            const std::string target = toLower(std::string(args[0]));

            auto it = std::find_if(JETBRAINS_PRODUCT.begin(), JETBRAINS_PRODUCT.end(), [&](const std::string &product) {
                return toLower(product) == target;
            });

            if (it != JETBRAINS_PRODUCT.end()) {
                bool found = false;

                for (const auto &entry: std::filesystem::directory_iterator(base)) {
                    if (!entry.is_directory()) {
                        continue;
                    }

                    if (std::string name = toLower(entry.path().filename().string()); name.find(target) == 0) {
                        reset(entry.path());
                        found = true;
                    }
                }

                if (found) {
                    purge();
                }
            } else {
                std::cerr << "[" << RED << "-" << RESET << "] No product found matching: " << YELLOW << target << RESET
                        << std::endl;
            }
        }
    });

    alias("-help", "-h");
    alias("--help", "-h");
    alias("--h", "-h");

    alias("-test", "-t");
    alias("--test", "-t");
    alias("--t", "-t");

    alias("-show", "-s");
    alias("--show", "-s");
    alias("-s", "-s");

    alias("-install", "-i");
    alias("--install", "-i");
    alias("--i", "-i");

    alias("-list", "-l");
    alias("--list", "-l");
    alias("--", "-l");

    alias("-reset", "-r");
    alias("--reset", "-r");
    alias("--r", "-r");

    if (argc == 1) {
        char *temp[] =
        {
            const_cast<char *>(""),
            const_cast<char *>("-h")
        };

        parse(2, temp);
    } else {
        parse(argc, argv);
    }

    return EXIT_SUCCESS;
}
