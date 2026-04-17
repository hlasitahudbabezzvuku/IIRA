# Installing IIRA on Linux

This guide covers installing and using the IIRA compiler on Linux systems, with specific instructions for Fedora, RHEL, and related distributions. It also includes guidance for Windows (via WSL) and macOS users.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Installing Build Tools on Fedora/RHEL](#installing-build-tools-on-fedorarhel)
- [Installing QBE from Source](#installing-qbe-from-source)
- [Building IIRA](#building-iira)
- [Verifying the Installation](#verifying-the-installation)
- [Using the Compiler](#using-the-compiler)
- [Full Compilation Pipeline](#full-compilation-pipeline)
- [Running the Test Suite](#running-the-test-suite)
- [Setting Up the Development Environment](#setting-up-the-development-environment)
- [Troubleshooting](#troubleshooting)

---

## Prerequisites

Before building IIRA, ensure you have the following installed:

| Requirement | Version | Description |
|-------------|---------|-------------|
| GCC or Clang | 11+ (GCC) / 13+ (Clang) | C23-compatible compiler |
| Git | Any recent version | Source control |
| Xmake | Latest | Build system |
| QBE | Latest | Backend compiler (must be built from source) |

---

## Installing Build Tools on Fedora/RHEL

### Fedora

Install the required packages using `dnf`:

```bash
sudo dnf install gcc clang git
```

### RHEL / CentOS / AlmaLinux

For RHEL 8+ or CentOS Stream, you may need to enable additional repositories for modern compiler versions:

```bash
sudo dnf install gcc gcc-c++ clang git
```

For RHEL 7 or older CentOS versions, enable the DevToolset for GCC 11+:

```bash
sudo dnf install centos-release-scl
sudo dnf install devtoolset-11-gcc devtoolset-11-gcc-c++ clang
scl enable devtoolset-11 bash
```

### Installing Xmake

Xmake is not typically available in standard Fedora/RHEL repositories. Install it via the official script:

```bash
curl -fsSL https://xmake.io/shget.text | bash
```

After installation, restart your terminal or source your profile:

```bash
source ~/.bashrc  # or ~/.zshrc
```

Verify the installation:

```bash
xmake --version
```

---

## Installing QBE from Source

QBE is the backend compiler used by IIRA. It is not available in standard package repositories and must be built from source.

### Clone and Build QBE

```bash
git clone https://c9x.me/qbe.git
cd qbe
make
```

This produces a `qbe` binary in the current directory.

### Install QBE System-Wide

```bash
sudo cp qbe /usr/local/bin/
```

Or install to your home directory for a local installation:

```bash
mkdir -p ~/.local/bin
cp qbe ~/.local/bin/
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

### Verify QBE Installation

```bash
qbe --version
```

---

## Building IIRA

Clone the repository and build:

```bash
git clone https://github.com/yourusername/IIRA.git
cd IIRA
xmake build
```

The compiler binary will be created at:

```
build/linux/x86_64/debug/iirac
```

### Adding IIRA to Your PATH

For convenience, add the binary to your PATH:

```bash
echo 'export PATH="$PWD/build/linux/x86_64/debug:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

Or create a symbolic link:

```bash
sudo ln -s "$PWD/build/linux/x86_64/debug/iirac" /usr/local/bin/iirac
```

---

## Verifying the Installation

Run the compiler with the `--version` flag:

```bash
iirac --version
```

Or compile a simple program:

```bash
echo 'fn main() { printf("Hello, IIRA!\n"); }' > hello.iira
iirac hello.iira | qbe -o hello
./hello
```

---

## Using the Compiler

### Basic Usage

```bash
iirac <input_file>
```

By default, the compiler outputs QBE IR to stdout.

### Command-Line Options

| Option | Description |
|--------|-------------|
| `-h`, `--help` | Print help message and exit |
| `-v`, `--version` | Print version information |
| `-s`, `--no-warn` | Suppress all warnings |
| `--verbose` | Enable verbose output |
| `--debug` | Enable debug output |
| `--trace` | Enable tracing output |
| `-o`, `--output <name>` | Specify output name (without suffix) |
| `--max-errors <n>` | Maximum errors before exiting |
| `--show-lexer-output` | Print generated tokens to stdout |
| `--show-parser-output` | Print parsed AST to stdout |
| `--show-analyzer-output` | Print analyzed TAST to stdout |
| `--stop-after-lexer` | Stop after lexical analysis |
| `--stop-after-parser` | Stop after parsing phase |
| `--stop-after-analyzer` | Stop after semantic analysis |

### Viewing Intermediate Outputs

You can inspect the compilation pipeline at each stage:

```bash
# View lexer output (tokens)
iirac input.iira --show-lexer-output

# View parser output (AST)
iirac input.iira --show-parser-output

# View analyzer output (typed AST)
iirac input.iira --show-analyzer-output

# Stop after specific phases
iirac input.iira --stop-after-lexer
iirac input.iira --stop-after-parser
iirac input.iira --stop-after-analyzer
```

---

## Full Compilation Pipeline

### Method 1: Direct Pipeline

The simplest method pipes the compiler output directly to QBE:

```bash
iirac input.iira | qbe -o output
chmod +x output
./output
```

### Method 2: Save QBE Output First

For inspection or debugging, save the intermediate QBE code:

```bash
iirac input.iira > output.qbe
qbe output.qbe -o output
./output
```

View the QBE output before compiling:

```bash
iirac input.iira > output.qbe
cat output.qbe
```

### Method 3: Compile with GCC or Clang

QBE outputs assembly, which can be compiled with your preferred compiler:

```bash
# Generate QBE IR
iirac input.iira > output.qbe

# Compile QBE IR to assembly with QBE
qbe output.qbe -o output.s

# Assemble with GCC
gcc output.s -o output
./output

# Or with Clang
clang output.s -o output
./output
```

### Complete Example

```bash
# Create a simple IIRA program
cat > factorial.iira << 'EOF'
fn main() {
    int n = 5;
    int result = 1;
    for (int i = 1; i <= n; i = i + 1) {
        result = result * i;
    }
    printf("Factorial of %d is %d\n", n, result);
}
EOF

# Compile and run
iirac factorial.iira | qbe -o factorial
./factorial
```

Expected output:

```
Factorial of 5 is 120
```

---

## Running the Test Suite

The project includes a test suite that validates the compiler against example programs:

```bash
./test.sh
```

This script:
1. Compiles all example programs in `examples/*/*.iira`
2. Runs error test cases (files prefixed with `fail_`) and verifies they produce errors
3. Reports PASS/FAIL status for each test

### Understanding Test Results

- **PASS (green)**: Program compiled and ran successfully (or failed as expected for error tests)
- **FAIL (red)**: Compilation or execution failed unexpectedly

### Adding Your Own Tests

Place `.iira` files in the `examples/` subdirectories:
- `examples/integration/` - Full program tests
- `examples/declarations/` - Declaration tests
- `examples/expressions/` - Expression tests
- `examples/inheritance/` - Inheritance tests

For error tests, prefix the filename with `fail_`:

```
examples/integration/fail_syntax_error.iira
```

---

## Setting Up the Development Environment

### Windows (WSL)

Windows Subsystem for Linux provides a full Linux kernel on Windows. This is the recommended approach for Windows users.

#### Step 1: Enable WSL

Open PowerShell as Administrator and run:

```powershell
wsl --install
```

Restart your computer when prompted.

#### Step 2: Install a Linux Distribution

From the Microsoft Store, install **Ubuntu** (or your preferred distribution).

#### Step 3: Install Build Tools

```bash
sudo apt update
sudo apt install build-essential git gcc clang
```

#### Step 4: Install Xmake

```bash
curl -fsSL https://xmake.io/shget.text | bash
source ~/.bashrc
```

#### Step 5: Install QBE

```bash
git clone https://c9x.me/qbe.git
cd qbe
make
sudo cp qbe /usr/local/bin/
```

#### Step 6: Build IIRA

```bash
git clone https://github.com/yourusername/IIRA.git
cd IIRA
xmake build
```

#### Step 7: Use VS Code (Recommended)

Install **VS Code** and the **WSL extension**. This allows you to edit code in Windows while running the compiler in WSL.

1. Open VS Code
2. Install the "WSL" extension
3. Click "Open Folder in WSL" and navigate to your IIRA directory
4. Open an integrated terminal (Ctrl+`) - it will be a bash shell in WSL

### macOS

macOS requires Homebrew for most development tools.

#### Step 1: Install Homebrew

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

#### Step 2: Install Build Tools

```bash
brew install gcc git
```

macOS includes Clang by default (via Xcode Command Line Tools).

#### Step 3: Install Xmake

```bash
brew install xmake
```

#### Step 4: Install QBE

```bash
git clone https://c9x.me/qbe.git
cd qbe
make
sudo cp qbe /usr/local/bin/
```

#### Step 5: Build IIRA

```bash
git clone https://github.com/yourusername/IIRA.git
cd IIRA
xmake build
```

#### Step 6: Configure Compiler (if needed)

macOS may use Apple Clang, which may have limited C23 support. If you encounter issues:

```bash
brew install gcc
export CC=gcc-14  # Use installed GCC version
xmake clean
xmake build
```

---

## Troubleshooting

### "iirac: command not found"

The compiler is not in your PATH. Either:
1. Add the build directory to your PATH:
   ```bash
   export PATH="$PWD/build/linux/x86_64/debug:$PATH"
   ```
2. Or use the full path:
   ```bash
   ./build/linux/x86_64/debug/iirac input.iira
   ```

### "qbe: command not found"

QBE is not installed or not in your PATH. Verify the installation:

```bash
which qbe
ls -l ~/.local/bin/qbe
```

If missing, reinstall QBE (see [Installing QBE from Source](#installing-qbe-from-source)).

### "xmake: command not found"

Xmake installation failed or was not sourced. Reinstall:

```bash
curl -fsSL https://xmake.io/shget.text | bash
source ~/.bashrc
```

### Compilation Errors with GCC

IIRA requires a C23-compatible compiler. Check your GCC version:

```bash
gcc --version
```

If using an older GCC on RHEL/CentOS, enable a newer toolset:

```bash
scl enable devtoolset-11 bash
```

### QBE Compilation Errors

QBE may produce errors if the generated IR is invalid. Use `--show-analyzer-output` to inspect the intermediate representation:

```bash
iirac input.iira --show-analyzer-output > output.qbe 2>&1
qbe output.qbe
```

### Slow Compilation

For faster incremental builds:

```bash
xmake -j$(nproc)  # Use all CPU cores
```

### Permission Denied When Running Output

Make sure the compiled binary is executable:

```bash
chmod +x output
./output
```

### Test Suite Failures

If `./test.sh` reports unexpected failures:

1. Ensure all dependencies are installed and working:
   ```bash
   iirac --version
   qbe --version
   ```

2. Clean and rebuild:
   ```bash
   xmake clean
   xmake build
   ```

3. Run a specific test manually:
   ```bash
   iirac examples/integration/factorial.iira | qbe -o factorial
   ./factorial
   ```

### macOS: Header Files Not Found

Ensure Xcode Command Line Tools are installed:

```bash
xcode-select --install
```

---

## Additional Resources

- [IIRA Documentation](../README.md) - Language reference and examples
- [QBE Documentation](https://c9x.me/compile/docs.html) - Backend compiler reference
- [Xmake Documentation](https://xmake.io/#/) - Build system guide
