#pragma once

#include <algorithm>
#include <chrono>
#include <functional>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#define BLACK   "\033[30m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define RESET   "\033[0m"

struct Download {
    std::string link;
    long long size{};
    std::string checksum;
};

struct List {
    std::string product;
    std::string code;
    std::string version;
    std::string build;
    std::string date;

    std::map<std::string, Download> downloads;
};

#ifdef _WIN32
#include <shlobj.h>
#include <tlhelp32.h>
#include <windows.h>

inline constexpr std::array<std::string_view, 2> JETBRAINS_REGISTRY_PATH =
{
    "Software\\JavaSoft",
    "Software\\JetBrains"
};
#endif

inline const std::vector<std::string> JETBRAINS_IDS_FILE =
{
    "PermanentDeviceId",
    "PermanentUserId",
    "bl",
    "crl"
};

inline const std::vector<std::string> JETBRAINS_PRODUCT =
{
    "IntelliJIdea",
    "PyCharm",
    "WebStorm",
    "PhpStorm",
    "CLion",
    "GoLand",
    "Rider",
    "DataGrip",
    "RubyMine",
    "RustRover",
    "AndroidStudio",
    "Fleet",
};


inline std::unordered_map<std::string, std::string> JETBRAINS_PRODUCT_NAME = {
    {"IIU", "IntelliJIdea"},
    {"PCP", "PyCharm"},
    {"WS", "WebStorm"},
    {"PS", "PhpStorm"},
    {"CL", "CLion"},
    {"GO", "GoLand"},
    {"RD", "Rider"},
    {"DG", "DataGrip"},
    {"RM", "RubyMine"},
    {"RR", "RustRover"},
    {"AS", "AndroidStudio"},
    {"FL", "Fleet"}
};

inline const std::vector<std::string> JETBRAINS_TARGET_EXECUTABLES =
{
    "idea64",
    "pycharm64",
    "webstorm64",
    "phpstorm64",
    "rider64",
    "datagrip64",
    "clion64",
    "goland64",
    "rubymine64",
    "idea",
    "pycharm",
    "webstorm",
    "phpstorm",
    "rider",
    "datagrip",
    "clion",
    "goland",
    "rubymine"
};

inline std::string strip(const std::string &ansi) {
    std::string temp;
    bool in_escape = false;

    for (const char c: ansi) {
        if (c == '\033') {
            in_escape = true;
        } else if (in_escape && c == 'm') {
            in_escape = false;
        } else {
            temp += c;
        }
    }

    return temp;
}

inline std::string padding(const std::string &string, size_t width) {
    const size_t length = strip(string).size();

    if (length >= width) {
        return string;
    }

    return string + std::string(width - length, ' ');
}

inline std::string toLower(const std::string &s) {
    std::string result = s;

    std::ranges::transform(result, result.begin(), [](const unsigned char c) {
        return std::tolower(c);
    });

    return result;
}

inline std::string get_product_name(const std::string &code) {
    if (const auto it = JETBRAINS_PRODUCT_NAME.find(code); it != JETBRAINS_PRODUCT_NAME.end()) {
        return it->second;
    }

    return "";
}

inline std::unordered_set<std::string> build_target_set(const std::vector<std::string> &targets) {
    std::unordered_set<std::string> set;
    set.reserve(targets.size());

    for (auto target: targets) {
        target = toLower(target);
#ifdef _WIN32
        if (!target.ends_with(".exe")) {
            target += ".exe";
        }
#endif

        set.insert(target);
    }

    return set;
}

inline std::vector<std::string> running_products(const std::vector<std::string> &targets) {
    const auto sets = build_target_set(targets);
    std::vector<std::string> products;

#ifdef _WIN32
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snapshot == INVALID_HANDLE_VALUE) {
        return products;
    }

    PROCESSENTRY32 process{};
    process.dwSize = sizeof(process);

    if (Process32First(snapshot, &process)) {
        do {
            if (std::string name = toLower(process.szExeFile); sets.contains(name)) {
                products.push_back(name);
            }
        } while (Process32Next(snapshot, &process));
    }

    CloseHandle(snapshot);
#else
    try {
        for (const auto &entry: std::filesystem::directory_iterator("/proc")) {
            if (const std::string pid = entry.path().filename().string(); !std::ranges::all_of(pid, ::isdigit)) {
                continue;
            }

            std::ifstream executable(entry.path() / "comm");

            if (std::string process; std::getline(executable, process)) {
                process = toLower(process);

                if (sets.contains(process)) {
                    if (std::ranges::find(products, process) == products.end()) {
                        products.push_back(process);
                    }
                }
            }
        }
    } catch (...) {
    }

#endif

    return products;
}
