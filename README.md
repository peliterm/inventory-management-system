# Inventory Management System

[![CI](https://github.com/peliterm/inventory-management-system/actions/workflows/ci.yml/badge.svg)](https://github.com/peliterm/inventory-management-system/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Standard](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)

**[Watch the demonstration video](https://github.com/peliterm/inventory-management-system/releases/latest)** (10 minutes)

A console-based inventory management system in C++17, built with the standard
library and nothing else. It tracks products, suppliers, and orders for a small
distributor; enforces role-based access; imports and exports CSV in bulk; and
produces six reports with text-mode charts.

Every commit is built by three jobs: g++ on Linux with warnings as errors, the
same build under AddressSanitizer and UndefinedBehaviorSanitizer, and the Visual
Studio solution under MSVC on Windows. All three run the full suite of **372
unit and integration checks**.

---

## What it looks like

```
  All Products
==================================================================================
ID        NAME                      CATEGORY          QTY      PRICE        VALUE
----------------------------------------------------------------------------------
P001      Steel Bolt M8             Fasteners         420       0.75       315.00
P002      Hex Nut M8                Fasteners          38       0.30        11.40  LOW
P003      Flat Washer M8            Fasteners         860       0.12       103.20
P004      Alloy Bracket             Brackets           24      14.95       358.80
P005      Steel Angle 2in           Brackets            7      22.40       156.80  LOW
P006      Hydraulic Seal            Seals             112       8.60       963.20
P007      O-Ring, Nitrile           Seals              15       1.85        27.75  LOW
----------------------------------------------------------------------------------
   7 product(s).
```

A console program has no drawing surface, so the charts are built from
characters, each bar scaled against the largest value in its series:

```
   Inventory value by category
   Fasteners  #################                         429.60
   Brackets   #####################                     515.60
   Seals      ########################################  990.95
```

---

## Running it

**Linux or macOS**, with any C++17 compiler:

```bash
make          # builds ./inventory
make test     # builds and runs the 372-check suite
./inventory
```

**Windows**: open `InventorySystem.sln` in Visual Studio 2022, set the
configuration to **Release x64**, and press **Ctrl + F5**. Full instructions,
including how to build the projects from scratch, are in
[COMPILING.md](COMPILING.md).

Run from the repository root so the relative `data/` paths resolve. There are no
dependencies, no database, and no configuration: all state lives in the four CSV
files in `data/`, loaded at start-up and written back on exit.

Sample accounts: `admin` / `admin123`, `mpelitera` / `manager1`,
`clerk1` / `clerk1pw`.

---

## Design

Nine classes, with one of them owning all the state.

```
main.cpp            menus, input validation, screen output
     |
Inventory           owns every vector; the only class that moves stock
     |
     +-- Product    price, quantity, reorder level
     +-- Supplier   contact details, supplied products
     +-- Order      lines, lifecycle, dates
     +-- User       role, permissions, password digest

Date  ·  CSVUtil  ·  ChartUtil      support, used throughout
```

`main.cpp` does nothing but menus and input checking. The four domain classes
each know their own rules and nothing about each other. Every operation that
touches more than one record type lives in `Inventory`, which means there is a
single door into the data rather than several.

Three decisions worth calling out:

**Permissions are asked by name, not by role.** No menu asks what role the
signed-in user holds. It asks whether they hold a named permission, and one
static function answers. Adding a fourth role means editing that function and
nothing else.

**Stock moves before the status commits.** A status change is two things at
once, a lifecycle move and a stock movement, and they have to agree. The stock
movement is validated against every line on the order and carried out first; if
it cannot be completed in full, the order is left exactly where it was. The
status and the shelf can never end up disagreeing.

**Stock is committed at placement, not at shipping.** A sales order deducts
stock the moment it is accepted, so two orders placed minutes apart cannot both
commit the last unit. The cost is that a cancellation has to give the stock
back, which is why cancellation is a real transition rather than a delete.

---

## Testing

`make test` runs 372 checks: seven unit sections, one per class, and seven
integration sections that drive several classes together through `Inventory`.
The checks are not spread evenly, and that is deliberate — orders and bulk CSV
import carry the most, because those are the two places where data changes
shape.

A deliberate attempt to break the finished program, followed by an independent
review pass, turned up fourteen real defects. The three worst:

- **Stock from nothing.** Two order lines of two billion units each overflowed a
  signed int, passed the availability check, and raised stock from 100 to
  294,967,396. Signed overflow is undefined behavior, so the fix cannot compute
  the sum and then inspect it — the bounds are checked in a wider type first.
- **The return that returned nothing.** A delivered purchase order could be
  marked RETURNED and keep the stock it had brought in. The order said returned
  and the shelf disagreed. This is what produced the transactional status change
  described above.
- **The login that only worked on one compiler.** Password digests were built
  with `std::hash`, which is implementation-defined. The accounts file written
  by g++ would have rejected every login under MSVC. Replaced with FNV-1a
  implemented in this project, so the digests mean the same thing everywhere,
  and pinned to literal values in the test suite.

Each of the fourteen is now a named regression test.

---

## Repository layout

```
src/           nine .cpp and eight .h files, one pair per class
data/          four sample CSV files
.github/       the CI workflow
Makefile       g++ / clang build
*.sln, *.vcxproj   Visual Studio 2022 solution, two projects
COMPILING.md   build instructions for both toolchains
```

---

## About

Built as the final project for CPSC 225, Embry-Riddle Aeronautical University,
August 2026 term. Published after the term closed. The code is mine; it is
shared as a portfolio piece and is not intended for submission by anyone else.

Licensed under the [MIT License](LICENSE).
