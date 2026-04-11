#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>

#include "stdafx.h"

#define BLACK   "\033[30m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define RESET   "\033[0m"

#pragma region COMMAND_PARSER

std::map<std::string_view, std::function<void(const std::vector<std::string_view>)> > commands;

void add(const std::string_view flag, std::function<void(const std::vector<std::string_view>)> args) {
    commands[flag] = std::move(args);
}

void parse(const int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::string_view arg = argv[i]; commands.contains(arg)) {
            std::vector<std::string_view> params;
#ifdef VERSION_ONE
            while (i + 1 < argc && argv[i + 1][0] != '-')
#else
            while (i + 1 < argc)
#endif
            {
#ifndef VERSION_ONE
                if (commands.contains(argv[i + 1])) {
                    break;
                }
#endif
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
                (trimmed.find("trial.") != std::string_view::npos && trimmed.find("availability") !=
                 std::string_view::npos);

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
#ifdef _WIN32
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
        if (!entry.is_directory() || entry.path().filename() == "consentOptions")
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
    if (const auto running = running_products(JETBRAINS_TARGET_EXECUTABLES); !running.empty()) {
        std::cout << "Please Close the Following Processes Before Continuing." << std::endl << std::endl;

        for (const auto &executable: running) {
            std::cout << RED << executable << RESET << std::endl;
        }

        return EXIT_FAILURE;
    }
#else
    // Todo: add Linux checking for instance
#endif

#ifdef _WIN32
    std::filesystem::path base = std::filesystem::path(home()) / "JetBrains";
#else
    std::filesystem::path base = std::filesystem::path(home()) / ".config" / "JetBrains";
#endif

    add("-h", [](const auto &args) {
        std::cout << "Usage: jb-reset [options]\n-s: Show status\n-r <name|--all/all>: Reset trial\n-h: Help" <<
                std::endl;
    });

#ifdef DEBUG
    // test
    add("-t", [&base](const auto &args) {
        std::cout << "User Profile: " << home() << std::endl;
        std::cout << "JetBrains Path: " << base.string() << std::endl;

        for (const auto &entry: std::filesystem::directory_iterator(base)) {
            if (entry.is_directory() && entry.path().filename() != "consentOptions") {
                std::cout << "Products: " << entry.path().filename().string() << std::endl;
            }
        }

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
    });
#endif

    add("-s", [&base](const auto &args) {
        list(base);
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
