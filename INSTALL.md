<div align="center">
    <h1>IIRA</h1>
    <h3>Getting your environment ready to build some compilers.</h3>
    <div>
        <img src="https://img.shields.io/badge/OPEN_SOURCE-161b22?style=for-the-badge&logo=opensourceinitiative&logoColor=7ee787&labelColor=21262d">
        <img src="https://img.shields.io/badge/LINUX-161b22?style=for-the-badge&logo=linux&logoColor=ffa657&labelColor=21262d">
        <img src="https://img.shields.io/badge/C_LANGUAGE-161b22?style=for-the-badge&logo=c&logoColor=58a6ff&labelColor=21262d">
        <img src="https://img.shields.io/badge/XMAKE-161b22?style=for-the-badge&logo=make&logoColor=d2a8ff&labelColor=21262d">
    </div>
</div><br><br>

> *"It's dangerous to go alone! Take this... installation guide."*

So, you want to build IIRA? Awesome. This guide will walk you through setting up your environment, compiling the frontend, and running your first IIRA program.

> [!note]
> IIRA is built primarily for Linux (specifically Fedora/RHEL). But don't worry—if you're gaming on Windows, I've got you covered with WSL.


<br><h2>Inventory Check (Prerequisites)</h2>

Before we can start crafting, make sure you have the following installed:

| Requirement | Version | Description |
|-------------|---------|-------------|
| GCC or Clang | 11+ (GCC) / 13+ (Clang) | C23-compatible compiler |
| Git | Any recent version | For downloading the source code |
| Xmake | Latest | The build system (vital) |
| QBE | Latest | Our compiler backend (built from source) |
| ASan / UBSan | N/A | Memory and undefined behavior sanitizers (Required for Debug mode) |



<br><h2>Equipping Your Build Tools</h2>

Depending on your Operating System, gathering your tools looks a bit different.


<br><h3>Fedora / RHEL</h3>

If you are on Fedora, you can grab everything easily:
```bash
sudo dnf install gcc clang git
```

For RHEL 8+ or CentOS Stream:
```bash
sudo dnf install gcc gcc-c++ clang git libasan libubsan
```

*Stuck in the past on RHEL 7?* You'll need the DevToolset for a modern GCC:
```bash
sudo dnf install centos-release-scl
sudo dnf install devtoolset-11-gcc devtoolset-11-gcc-c++ clang
scl enable devtoolset-11 bash
```


<br><h3>Windows (WSL)</h3>

Don't try to build this natively on Windows - it doesn't work. Use the Windows Subsystem for Linux (WSL2).
1. Open PowerShell as Admin and run: `wsl --install`
2. Restart your PC, install **Ubuntu** from the Microsoft Store.
3. Open your new WSL terminal and grab the tools:

```bash
sudo apt update
sudo apt install build-essential git gcc clang libasan8 libubsan1
```


<br><h3>Installing Xmake</h3>

IIRA uses Xmake. It isn't typically in standard package repos, so grab it via their official script:
```bash
curl -fsSL https://xmake.io/shget.text | bash
source ~/.bashrc  # or ~/.zshrc if don't know what's good
```


<br><h2>Building QBE</h2>

> [!important]
> Remember the architecture from the README? IIRA only handles the frontend. We rely entirely on [QBE](https://c9x.me/compile/) to turn our Intermediate Language (IL) into actual machine code. 

QBE isn't standard, so we have to build it from source.

```bash
git clone https://c9x.me/qbe.git
cd qbe
make
sudo cp qbe /usr/local/bin/ # or ~/.local/share/bin/ if you're fancy
```

Verify you equipped it correctly:
```bash
qbe --version
```


<br><h2>Building IIRA</h2>

Finally, let's build the compiler.

```bash
git clone https://github.com/hlasitahudbabezzvuku/IIRA.git
cd IIRA
xmake f -m release  # optional, but recommended
xmake build
```

If everything worked, your shiny new compiler binary is sitting at `build/linux/x86_64/debug/iirac`. 

For convenience, add it to your PATH:
```bash
echo 'export PATH="$PWD/build/linux/x86_64/debug:$PATH"' >> ~/.bashrc
source ~/.bashrc
```


<br><h2>Using the Compiler</h2>

Let's test it:
```bash
iirac ./examples/integration/test_main_function.iira | qbe -o test.s && qbe > test.s && gcc test.s -o test
./test
```


<br><h3>The Compilation Pipeline</h3>

If you want to see exactly what's happening under the hood, IIRA lets you inspect every phase of compilation. The full pipeline looks like this:

```

You can view the intermediate outputs to see how the compiler thinks:
```bash
iirac input.iira --show-lexer-output     # See the raw tokens
iirac input.iira --show-parser-output    # See the Abstract Syntax Tree (AST)
iirac input.iira --show-analyzer-output  # See the Typed AST (TAST)
iirac input.iira                         # See the QBE IL output
iirac input.iira | qbe                   # See the assembly output
```

You can also save the QBE output to study it, or compile it manually using your favorite C compiler:
```bash
iirac input.iira > output.qbe
qbe output.qbe -o output.s
gcc output.s -o output
./output
```


<br><h2>Running Tests</h2>

> *"It works on my machine!"* - Someone I liked...

The project includes a test suite that validates the compiler against example programs. To run the gauntlet:

```bash
./test.sh
```

- **PASS**: The code compiled and ran flawlessly (or failed successfully for intentional error tests).
- **FAIL**: We broke something. Time to debug.

Want to add your own tests? Drop `.iira` files into the `examples/` subdirectories. If it's a test that *should* fail (like a syntax error), just prefix the file with `fail_` (e.g., `fail_syntax_error.iira`).


<br><h2>Troubleshooting</h2>

Things rarely work perfectly on the first try. Here are a few common game-over screens and how to beat them:

- **`iirac: command not found`**
  Your terminal doesn't know where the compiler is. Make sure you added the build directory to your PATH, or just run it directly using `./build/linux/x86_64/debug/iirac`.

- **`qbe: command not found`**
  You either skipped the QBE step or it didn't install to your PATH. Head back to the [Building QBE](#the-backend-building-qbe) section.

- **Could not find ASan or UBSan**
  You need to install ASan and UBSan: `sudo dnf in libasan libubsan`

- **Compilation errors with GCC**
  IIRA requires a C23-compatible compiler. Run `gcc --version`. If it's less than version 11, you need to update.

- **Slow compilation?**
  Run `xmake -j$(nproc)` to use all your CPU cores.

- **Permission Denied when running the output?**
  You forgot to make the generated binary executable. Just cast `chmod +x your_output_file` on it.
