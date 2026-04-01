# JetBrains Trial Resetter

![Build Status](https://github.com/monakaibrahim-cmyk/cpp-jetbrains-reset-trial/actions/workflows/build.yml/badge.svg?branch=linux)

A lightweight C++ utility designed to manage and reset evaluation periods for JetBrains IDEs on Linux. It cleans up local configuration files and global Java user preferences to allow for a fresh environment.

### Features
- **Status Overview**: View all installed JetBrains products, their trial status, and days remaining.
- **Targeted Reset**: Reset the trial for a specific IDE(e.g., `PyCharm2023.2`).
- **Deep Clean**: Automatically purges global Java user preferences associated with JetBrains.

### Prerequisites
To build this tool, you need:
- A C++23 compatible compiler (GCC or Clang).
- Standard C++ Library
- Cmake
- Linux Environment (uses `HOME` environment variables and `~/.config` paths)

# Build Instructions
### 1. Clone:
```bash
git clone https://github.com/monakaibrahim-cmyk/cpp-jetbrains-reset-trial.git
cd cpp-jetbrains-reset-trial
```

### 2. Compile:
Use the following command to compile the project using GCC:
```bash
cmake -B build
cmake --build build
```

# Usage
Run the binary from the terminal using the following flags

| Flag | Description |
| --- | --- |
| `-h` | Show the help menu. |
| `-s` | Lists products, status, and days remaining. |
| `-r <name>` | Resets the trial for the specified product name. |
| `-r all` | Resets trials for every JetBrains product found. |

# How it Works
1. File Deletion: Removes the `eval` directory within the IDE's configuration path.
2. XML Patching: Parses `other.xml` to remove specific entries related to `evlsprt` and `trial.state`.
3. Global Purge: Deletes the `~/.java/.userPrefs/jetbrains` directory to clear registry-like keys.

# Disclaimer
[!WARNING]
This tool is for educational and personal development purposes only. Please support the developers at JetBrains by purchasing a formal license for professional use.