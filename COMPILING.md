# Compiling and Running

Two sets of instructions. **Visual Studio is the one the course requires**;
the command-line section is there so the project also builds on a Mac or
Linux machine.

---

## Visual Studio 2022 (Windows)

### Option A: open the supplied solution (fastest)

1. Unzip the project to a folder with no spaces in the path, for example
   `C:\Projects\InventorySystem`.
2. Double-click **`InventorySystem.sln`**. Visual Studio 2022 opens with two
   projects already configured:
   - **InventorySystem** — the interactive program
   - **InventoryTests** — the unit and integration test suite
3. If Visual Studio offers to retarget the solution, accept the defaults.
4. In the toolbar set the configuration to **Release** and the platform to
   **x64**.
5. In Solution Explorer, right-click **InventorySystem** and choose
   **Set as Startup Project**.
6. Press **Ctrl + F5** (Start Without Debugging).

The program opens a console window, loads the four files from `data\`, and
presents the login screen. Sign in as `admin` / `admin123`.

To run the tests instead, set **InventoryTests** as the startup project and
press Ctrl + F5 again. It prints a pass/fail count and nothing else.

> **Why Ctrl + F5 rather than F5:** Start Without Debugging keeps the console
> window open after the program ends. Plain F5 closes it the instant `main`
> returns, so the final summary disappears.

### Option B: build the project from scratch

Use this if the supplied solution will not open, or if the assignment asks you
to demonstrate creating the project yourself.

1. **File → New → Project**.
2. Choose **Console App** (C++), then **Next**.
3. Name it `InventorySystem`, pick a location, and click **Create**.
4. Visual Studio generates a starter `InventorySystem.cpp`. In Solution
   Explorer, right-click it and choose **Remove → Delete**; the supplied
   `main.cpp` replaces it.
5. Copy the `src` and `data` folders from this archive into the folder that
   contains the new `.vcxproj` file.
6. In Solution Explorer, right-click the project → **Add → Existing Item**.
   Select the `.cpp` files in `src` **except `tests.cpp`** and click **Add**.

   > Do not add every `.cpp` in the folder. `main.cpp` and `tests.cpp` each
   > define their own `main()`, so a project holding both fails to link with
   > *LNK2005: main already defined*. The program project takes `main.cpp` and
   > the eight class files; the test project takes `tests.cpp` and the same
   > eight class files.
7. Repeat **Add → Existing Item** and select all eight `.h` files in `src`.
8. Right-click the project → **Properties**, then set:

   | Page | Setting | Value |
   |------|---------|-------|
   | Configuration Properties → General | C++ Language Standard | ISO C++17 Standard (/std:c++17) |
   | C/C++ → General | Additional Include Directories | `$(ProjectDir)src` |
   | C/C++ → Preprocessor | Preprocessor Definitions | add `_CRT_SECURE_NO_WARNINGS` |
   | Debugging | Working Directory | `$(ProjectDir)` |
   | Linker → System | SubSystem | Console (/SUBSYSTEM:CONSOLE) |

   Set the configuration dropdown to **All Configurations** before changing
   these so Debug and Release both get them.
9. Set the platform to **x64**, then press **Ctrl + F5**.

To add the test project to the same solution, repeat steps 1–8 as a second
Console App named `InventoryTests`, adding `src\tests.cpp` **instead of**
`src\main.cpp`, plus the same eight class `.cpp` files and all eight headers.
Leave `main.cpp` out of this project for the same linker reason given above.

### The working directory matters

The program reads and writes `data\products.csv` and its three siblings using
**relative paths**. If Visual Studio launches it from a different folder, the
loader reports that it could not open the files and the system starts empty.

The supplied projects already set the debugger working directory to the
project folder. If you built from scratch and the data does not load, check
**Properties → Debugging → Working Directory** is `$(ProjectDir)` and that the
`data` folder sits beside the `.vcxproj` file.

### No database or extra configuration is required

All state lives in the four CSV files in `data\`. There is nothing to install,
no connection string to set, and no external library to download. The program
writes the files back on exit, so the next run resumes where the last one
stopped.

---

## Command line (g++ / clang, Linux or macOS)

```
make          # builds ./inventory
make test     # builds and runs ./run_tests
make clean    # removes both binaries
```

Requires a C++17 compiler. Developed against g++ 13.3 with `-Wall -Wextra`;
the build is warning-free. Run from the project root so the relative `data/`
paths resolve:

```
./inventory
```

### Checking it under the sanitizers

The test suite is clean under AddressSanitizer and UndefinedBehaviorSanitizer:

```
g++ -std=c++17 -g -fsanitize=address,undefined -o checked \
    src/tests.cpp src/Inventory.cpp src/Product.cpp src/Supplier.cpp \
    src/Order.cpp src/User.cpp src/Date.cpp src/CSVUtil.cpp src/ChartUtil.cpp
./checked
```

---

## Troubleshooting

| Symptom | Cause and fix |
|---|---|
| Console window closes instantly | You pressed F5. Use **Ctrl + F5**. |
| "Could not open data/products.csv" | Wrong working directory. See the section above. |
| `error C2039: 'localtime_s'` | The C++ language standard is not set to C++17. |
| Everything loads but there are no users | `data\users.csv` is missing. The program creates `admin` / `admin123` automatically in that case. |
| Build errors mentioning `std::` names | Add `$(ProjectDir)src` to Additional Include Directories. |
| The tests briefly create `test_*.csv` files | Expected. The import tests need real files on disk; the suite deletes them when it finishes. |
