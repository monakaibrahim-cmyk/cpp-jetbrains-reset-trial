#pragma once

#include <algorithm>
#include <chrono>
#include <functional>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

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

inline std::string toLower(const std::string &s) {
    std::string result = s;

    std::ranges::transform(result, result.begin(), [](const unsigned char c) {
        return std::tolower(c);
    });

    return result;
}

#ifdef _WIN32
#include <shlobj.h>
#include <tlhelp32.h>
#include <windows.h>
#endif

#ifdef _WIN32
inline const std::vector<std::string> JETBRAINS_IDS_FILE =
{
    "PermanentDeviceId",
    "PermanentUserId",
    "bl",
    "crl"
};

inline constexpr std::array<std::string_view, 2> JETBRAINS_REGISTRY_PATH =
{
    "Software\\JavaSoft",
    "Software\\JetBrains"
};

inline std::unordered_set<std::string> build_target_set(const std::vector<std::string> &targets) {
    std::unordered_set<std::string> set;
    set.reserve(targets.size());

    for (auto target: targets) {
        target = toLower(target);

        if (!target.ends_with(".exe")) {
            target += ".exe";
        }

        set.insert(target);
    }

    return set;
}

inline std::vector<std::string> running_products(const std::vector<std::string> &targets) {
    const auto sets = build_target_set(targets);
    std::vector<std::string> products;
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

    return products;
}

#else

#endif
