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

The goal is to create something that other students can dive into to see how compilers work. Most modern compilers are massive behemoths that take 40 minutes just to compile. IIRA on the other hand doesn't aim to be like C++, Rust, Zig, or Carbon. It's meant to be used for learning how their compilers work by building their younger, slightly more approachable sibling.

> [!warning]
> While I am excited to share my progress, please note that IIRA is its very **early stages**. I welcome early feedback and discussions, but I recommend against using IIRA for anything beyond simple experimentation. If you find a bug or have a suggestion, feel free to open an issue.


<br><h2>Design</h2>

> *"A human's concept of love requires admiration, attraction, devotion, and respect. Conclusion; I am 50% in Love."*

TODO


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


<h4>1. Lexer</h4>

It scans the entirety of the source file buffer in one pass, generating a flat array of **Tokens**. This helps the cache-locality of the data while also making the whole architecture a lot simpler.

<h4>2. Parser</h4>

It consumes the Token Vector, creating the **Abstract Syntax Tree (AST)**. Statements and block-level constructs (Tables, Type declarations) are parsed via standard [Recursive Descent](https://en.wikipedia.org/wiki/Recursive_descent_parser/) parser. Expressions (math, dot-notation method calls) are delegated to the [Operator Precedence](https://en.wikipedia.org/wiki/Operator-precedence_parser/) parser. The AST node structures are put into a linear memory arena. This guarantees cache-local child node resolution via standard C pointers. When encountering an error, it will insert an error placeholder and continue parsing until it synchronizes to the next statement boundary (e.g., `;` or `}`) to report multiple errors per compilation unit.


<h4>3. Semantics Analyzer</h4>

It will ensure that the parsed AST "makes sense". If it finds any errors, it will report them to the **Diagnostics Engine**, and continue the same way the Parser did.


<h4>4. Graph Builder & IL Emitter</h4>

The IR building is implemented as a **Two-Pass Intermediate Object Model**. There are two modules, each handling one pass:
- Pass 1 (Lowering - Graph Builder): It translates the AST nodes into an in-memory graph of QBE structures, thus contructing a **QBE Object Graph**. Memory is again managed via a linear memory arena.
- Pass 2 (Serialization - IL Emitter): It traverses the QBE Object Graph and builds strictly formatted textual QBE IL. 


<br><h2>Repository Layout</h2>

There are two projects in this repository:
1. UnFinity: My in-house minimal utility library
2. iirac: The IIRA compiler itself

> [!note]
> There are no external dependencies aside from ASan, UBSan, and the C standard library (libc).

**UnFinity** has several modules:
- uf_common - Common functionality used across other modules
- uf_memory - Wrappers for standard memory allocation/deallocation functions
- uf_logger - Simple logging module designed for printing to terminal
- uf_containers - Container structures for efficient data handling

**iirac** also has several modules:
- arguments: Argument parsing
- lexer: The lexer implementation
- TODO

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

