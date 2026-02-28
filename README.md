<div align="center">
    <h1>IIRA</h1>
    <h3>A high-performance systems programming language built by students.</h3><br>
    <div>
        <img src="https://img.shields.io/badge/OPEN_SOURCE-161b22?style=for-the-badge&logo=opensourceinitiative&logoColor=7ee787&labelColor=21262d">
        <img src="https://img.shields.io/badge/LINUX-161b22?style=for-the-badge&logo=linux&logoColor=ffa657&labelColor=21262d">
        <img src="https://img.shields.io/badge/C_LANGUAGE-161b22?style=for-the-badge&logo=c&logoColor=58a6ff&labelColor=21262d">
        <img src="https://img.shields.io/badge/XMAKE-161b22?style=for-the-badge&logo=make&logoColor=d2a8ff&labelColor=21262d">
    </div>
</div><br><br>

> *"Hey you, you're finally awake. You were trying to cross the border into Compiler Development, right?"*

Hi, I'm František Lednický and this is IIRA. A programming language built by a students, for students.

> [!note]
> Right now, this is a "solo" project. It's just me working on it.

The goal is to create something that other students can dive into to see how compilers work. Most modern compilers are massive behemoths that take 40 minutes just to compile. IIRA on the other hand doesn't aim to be like C++, Rust, Zig, or Carbon. It's meant to be used for learning how compilers for such languages work by building their younger, **more approachable** sibling.

> [!warning]
> While I am excited to share my progress, please note that IIRA is its very **early stages**. I welcome early feedback and discussions, but I recommend against using IIRA for anything beyond simple experimentation. If you find a bug or have a suggestion, feel free to open an issue.


<br><h2>Design</h2>

> *"A human's concept of love requires admiration, attraction, devotion, and respect. Conclusion; I am 50% in Love."*

*TODO: Simple Design Document for the IIRA's philosophy and syntax.*


<br><h2>Architecture</h2>

Standard compiler architecture looks something like this:
```
           ------------     -----------     -------------     ----------
source --> | frontend | --> | backend | --> | assembler | --> | linker | --> a.out
           ------------     -----------     -------------     ----------
                       --^---         ---^----           --^---
                       qbe il         assembly           object
```

The frontend is responsible for parsing the language and constructing a machine-independent **Intermediate Representation (IR)** represented in **Intermediate Language (IL)**. The backend then optimizes this IR, and turns it into compilable machine-specific assembly.

> [!note]
> IIRA uses [QBE](https://c9x.me/compile/docs.html) as it's backend, which means that we can focus **purely** onto building the frontend.

IIRA's frontend architecture looks like this:
```
                ---------     -----------     ----------------------     -----------------     --------------
source.iira --> | lexer | --> | pareser | --> | semantics analyzer | --> | graph builder | --> | il emitter | --> source.qbe
                ---------     -----------     ----------------------     -----------------     --------------
                         --^---       -----^-----                -----^-----              --^--
                         tokens       syntax tree                syntax tree              graph
```

*TODO: More detailed description.*


<br><h2>Repository Layout</h2>

There are three projects in this repository (for now):
1. **unfinity** (lib): Minimal utility library
2. **iira** (lib): Shared components for IIRA's compiler, language server, and formatter
3. **iirac**: The IIRA compiler frontend

> [!note]
> There are no external dependencies aside from ASan, UBSan, and the C standard library (libc).

**unfinity** includes:
- *uf_common* - Common functionality used across other modules
- *uf_memory* - Wrappers for standard memory allocation/deallocation functions
- *uf_logger* - Simple logging module designed for printing to terminal
- *uf_containers* - Container structures for efficient data handling

**iira** includes:
- *ii_lexer* - IIRA's lexical analyzer
- TODO

**iirac** includes:
- *iic_arguments* - Argument parsing
- TODO

> [!important]
> IIRA uses [Xmake](https://xmake.io/) as the main build system. You have to install it first via you package manager.


<br><h2>Contribute</h2>

Anyone regardless of experience can join. Whether you want to fix a typo in the docs, optimize a semantics analyzer pass, or design a new module, you are **welcome** here.

But before you open a Pull Request:
- Check the **issues**. I don't want to end up with two people trying to kill the same boss.
- If your code looks like a *Skyrim* mod list with 500 conflicts, it’s not getting merged.
- Unlike Valve, I actually plan to reach v3.0 someday. **Maintainable** code is crucial for that to happen.
- I’m a student too, so I might not review a PR instantly. **Be patient**; I’m probably studying (or playing Diablo).

I believe in giving credit where it's due. Under the **Mozilla Public License 2.0** licensing model, your contributions belong to the **community**, but the ownership of your work **stays with you**. Every time you contribute a new file or lead a major change, your name goes at the top of that file. You aren't just an anonymous "contributor #69"; you are an **author**.


<br><h2>License</h2>

IIRA is licensed under the MPL 2.0. This is a weak-copyleft license that protects the compiler source code while allowing you to use the language to build any application (proprietary or open source) without restrictions. See the [LICENSE](LICENSE) file for more information.

