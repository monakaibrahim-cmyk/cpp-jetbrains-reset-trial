# JetBrains Trial Resetter

![Linux Build Status](https://github.com/monakaibrahim-cmyk/cpp-jetbrains-reset-trial/actions/workflows/linux.yml/badge.svg)
![Windows Build Status](https://github.com/monakaibrahim-cmyk/cpp-jetbrains-reset-trial/actions/workflows/windows.yml/badge.svg)

A lightweight C++ utility designed to manage and reset evaluation periods for JetBrains IDEs on Linux. It cleans up local configuration files and global Java user preferences to allow for a fresh environment.

### Features
- **Status Overview**: View all installed JetBrains products, their trial status, and days remaining.
- **Targeted Reset**: Reset the trial for a specific IDE(e.g., `PyCharm2023.2`).
- **Deep Clean**: Automatically purges global Java user preferences associated with JetBrains.
- **Install Product**: Installs an installer or archive and automatically remove the file after installation.

### Prerequisites
To build this tool, you need:
- A C++23 compatible compiler (GCC or Clang).
- Standard C++ Library
- Cmake
- VCPKG
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
cmake --preset windows-clion-release
cmake --build --preset windows-clion-release --parallel
```

# Usage
Run the binary from the terminal using the following flags

| Flag                             | Description                                   |
|----------------------------------|-----------------------------------------------|
| `-h, --help`                     | Show available commands.                      |
| `-t, --test`                     | Run internal test suite.                      |
| `-i, --install <name/code>`      | Download and install a JetBrains product.     |
| `-l, --list --online`            | List local products or fetch online releases. |
| `-r, --reset <name/(all/--all)>` | Reset the evaluation period for products.     |

# How it Works
1. File Deletion: Removes the `eval` directory within the IDE's configuration path.
2. XML Patching: Parses `other.xml` to remove specific entries related to `evlsprt` and `trial.state`.
3. Global Purge: Deletes the `~/.java/.userPrefs/jetbrains` directory to clear registry-like keys.

# Disclaimer
> [!WARNING]
> This tool is for educational and personal development purposes only. Please support the developers at JetBrains by purchasing a formal license for professional use.
