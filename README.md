# Windows Symlink Shim

## Introduction

Windows Shim is a mechanism designed to simplify the process of launching executable files while addressing some of the limitations associated with symbolic links. By using Shim, you can more efficiently and reliably manage executable files and their dependencies, avoiding the path management and DLL loading issues that can arise when using symbolic links.

This project provides a cleaner, more streamlined approach to launching executable files, avoiding the problems symbolic links might cause when handling complex dependencies, especially when dynamically loading DLLs.

## Why Shim is Needed?

On Windows systems, executable file paths often need to be added to the `PATH` environment variable to make running programs easier. However, this practice leads to the `PATH` variable growing larger and more difficult to manage, as more programs are added. Additionally, traditional symbolic links have their own limitations, particularly when it comes to dynamically loading DLLs, as they cannot effectively handle program dependencies and path-related issues.

The Shim mechanism provides an intermediary layer that centralizes the management of executable files, avoiding these problems. It helps reduce confusion in the `PATH` environment variable and offers better management of multiple software executables.

## Why Not Use Symbolic Links?

Symbolic links are suitable for statically compiled programs because these programs do not rely on DLLs in their own paths. However, for programs that need to dynamically load DLLs, symbolic links may not work as expected, especially for those programs that rely on DLLs located in their own directory. Symbolic links do not handle the program's own dependency paths, which means that at runtime, the program may fail to find and load its own DLLs, causing startup failures.

Unlike symbolic links, the Shim mechanism is better suited for managing these dynamic dependencies and can centralize the management of executable files, helping to avoid the issues caused by path management confusion.

## How to Use the Shim

### Step 1: Create a Symbolic Link in the Directory Where `shim.exe` is Located

First, you need to create a symbolic link in the directory where the `shim.exe` binary is located that points to the actual target executable. This symbolic link will act as a configuration file, which Shim will use to parse and correctly launch the target program. This design is intended to be compatible with package managers (such as Winget) that manage programs via symbolic links.

**Example**:
```console
mklink "C:\path\to\shim\my_program.exe" "C:\path\to\target\program.exe"
```

### Step 2: Create a Symbolic Link to the Shim Executable

Next, you need to create a symbolic link to the Shim launcher program. *The name of this symbolic link must match the name of the symbolic link created in Step 1.*

**Example**:
```console
mklink "C:\bin\my_program.exe" "C:\path\to\shim\shim.exe"
```

### Step 3: Run the Program

Now, you can run the program by executing the symbolic link created in Step 2 (which points to `shim.exe`), just like you would a regular executable.

**Example**:
```console
C:\bin\my_program.exe
```

Shim will:

1. Parse the filename of the symbolic link that calls shim.exe.
2. Read the target of the symbolic link in its own directory that has the same name obtained in step 1, which points to the program to be executed.
3. Launch the target program (either as a GUI or a console application, depending on the program type).

## License

This project is open-source under the BSD 3-Clause License. For more information, please refer to the [LICENSE](LICENSE.txt) file.
