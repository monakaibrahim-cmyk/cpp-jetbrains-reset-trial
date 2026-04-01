#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <string_view>
#include <string>
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

#pragma region COMMAND_PARSER

std::map<std::string_view, std::function<void(const std::vector<std::string_view>)>> commands;

void add(std::string_view flag, std::function<void(const std::vector<std::string_view>)> args)
{
    commands[flag] = std::move(args);
}

void parse(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];

        if (commands.count(arg))
        {
            std::vector<std::string_view> params;

            while (i + 1 < argc && argv[i + 1][0] != '-')
            {
                params.push_back(argv[++i]);
            }

            commands[arg](params);
        }
        else
        {
            std::cerr << GREEN << "Unknown Option" << RESET << ": " << RED << arg << RESET << std::endl;
        }
    }
}

#pragma endregion

#pragma region JB_RESET

std::string home()
{
    const char* home = getenv("HOME");
    return home ? std::string(home) : "";
}

std::string_view trim(std::string_view string)
{
    auto start = string.find_first_not_of(" \t\r\n");

    if (start == std::string_view::npos)
    {
        return "";
    }

    auto end = string.find_last_not_of(" \t\r\n");

    return string.substr(start, end - start + 1);
}

std::optional<long long> timestamp(const std::string& content)
{
    std::smatch match;

    std::regex pattern(R"jb("trial\.state\.free\.trial\.expiration\.date"\s*:\s*"(\d+)")jb");
    
    if (std::regex_search(content, match, pattern) && match.size() > 1)
    {
        try
        {
            return std::stoll(match.str(1));
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

std::string clean(const std::string& content)
{
    std::stringstream stream(content);
    std::string line, result;
    bool skip = false;

    while (std::getline(stream, line))
    {
        std::string_view trimmed = trim(line);

        bool val = 
            (trimmed.find("evlsprt") != std::string_view::npos) ||
            (trimmed.find("trial.state") != std::string_view::npos) ||
            (trimmed.find("trial.") != std::string_view::npos && trimmed.find("availability") != std::string_view::npos);
        
        if (val)
        {
            skip = true;
            continue;
        }

        if (skip && (trimmed == "," || trimmed.empty()))
        {
            skip = false;
            continue;
        }

        skip = false;
        result += line + "\n";
    }

    return result;
}

void reset(const std::filesystem::path& product)
{
    std::cout << "[" << YELLOW <<  "*" << RESET << "] Resetting " << GREEN << product.filename() << RESET << std::endl;

    if (std::filesystem::exists(std::filesystem::path(product / "eval")))
    {
        std::filesystem::remove_all(std::filesystem::path(product / "eval"));

        std::cout << "[" << GREEN << "+" RESET "] Deleted eval folder" << std::endl;
    }

    if (std::filesystem::exists(std::filesystem::path(product / "options" / "other.xml")))
    {
        std::ifstream in(std::filesystem::path(product / "options" / "other.xml"));
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();

        std::string cleaned = clean(content);

        if (content.length() != cleaned.length())
        {
            std::ofstream out(std::filesystem::path(product / "options" / "other.xml"));

            out << cleaned;

            std::cout << "[" << GREEN "+" << RESET << "] Patched " << GREEN << "other.xml" << RESET << std::endl;
        }
    }
}

void purge()
{
    if (std::filesystem::exists(std::filesystem::path(home()) / ".java" / ".userPrefs" / "jetbrains"))
    {
        std::filesystem::remove_all(std::filesystem::path(home()) / ".java" / ".userPrefs" / "jetbrains");

        std::cout << "[" << GREEN << "+" << RESET << "] Purged global Java preferences" << std::endl;
    }
}

void list(const std::filesystem::path& base)
{
    if (!std::filesystem::exists(base))
    {
        std::cerr << "Directory doesn't exists! '" << base << "'" << std::endl;
        return;
    }

    std::cout << std::left 
        << std::setw(30) << GREEN "PRODUCT" RESET
        << std::setw(20) << GREEN "STATUS" RESET
        << GREEN "DAYS LEFT" RESET << std::endl;

    std::cout << std::string(65, '-') << std::endl;

    for (const auto& entry : std::filesystem::directory_iterator(base))
    {
        if (!entry.is_directory())
        {
            continue;
        }

        std::filesystem::path xml = entry.path() / "options" / "other.xml";

        if (std::filesystem::exists(xml))
        {
            std::ifstream in(xml);
            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            auto time = timestamp(content);

            if (time)
            {
                std::string name = GREEN + entry.path().filename().string() + RESET;
            
                std::cout << std::left << std::setw(30) << name;

                auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                long long diff_ms = *time - now_ms;
                double precise_days = static_cast<double>(diff_ms) / 86400000.0;
                int remaining_days = static_cast<int>(std::ceil(precise_days)) - 3;

                if (remaining_days > 0)
                {
                    std::cout << std::setw(20) << GREEN "ACTIVE" RESET << YELLOW << remaining_days << RESET << std::endl;
                }
                else
                {
                    std::cout << std::setw(20) << RED << "EXPIRED" << "N/A" << RESET << std::endl;
                }
            }
            else
            {
                std::cout << std::setw(30) << GREEN "eval" RESET << std::setw(20) << (std::filesystem::exists(entry.path() / "eval") ? "ACTIVE" : "EXPIRED") << "N/A" << std::endl;
            }     
        }
        else
        {
            std::cout << std::setw(20) << "No config" << "N/A" << std::endl;
        }
    }
}

#pragma endregion

int main(int argc, char** argv)
{
    std::filesystem::path base = std::filesystem::path(home()) / ".config" / "JetBrains";

    add("-h", [](const auto& args) {
        std::cout << "Usage: jb-reset [options]\n-s: Show status\n-r <name|--all>: Reset trial\n-h: Help" << std::endl;
    });

    add("-s", [&](const auto& args) {
        list(base);
    });

    add("-r", [&](const auto& args) {
        if (args.empty())
        {
            return;
        }

        if (args[0] == "all")
        {
            for (const auto& entry : std::filesystem::directory_iterator(base))
            {
                if (entry.is_directory())
                {
                    reset(entry.path());
                }
            }

            purge();
        }
        else
        {
            std::filesystem::path target = base / args[0];

            if (std::filesystem::exists(target))
            {
                reset(target);
                purge();
            }
        }
    });

    if (argc == 1)
    {
        char* temp[] = { (char*)"", (char*)"-h" };

        parse(2, temp);
    }
    else
    {
        parse(argc, argv);
    }

    return 0;
}
