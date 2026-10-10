# ---------------------------------------------------------------------------
# Makefile for the Inventory Management System
# ---------------------------------------------------------------------------
#   make          build the interactive program  (./inventory)
#   make test     build and run the test suite   (./run_tests)
#   make clean    remove build output
# ---------------------------------------------------------------------------

CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
SRCDIR   = src

# Every class the program and the tests share.
CORE = $(SRCDIR)/Inventory.cpp $(SRCDIR)/Product.cpp $(SRCDIR)/Supplier.cpp \
       $(SRCDIR)/Order.cpp $(SRCDIR)/User.cpp $(SRCDIR)/Date.cpp \
       $(SRCDIR)/CSVUtil.cpp $(SRCDIR)/ChartUtil.cpp

all: inventory

inventory: $(SRCDIR)/main.cpp $(CORE)
	$(CXX) $(CXXFLAGS) -o inventory $(SRCDIR)/main.cpp $(CORE)

run_tests: $(SRCDIR)/tests.cpp $(CORE)
	$(CXX) $(CXXFLAGS) -o run_tests $(SRCDIR)/tests.cpp $(CORE)

test: run_tests
	./run_tests

clean:
	rm -f inventory run_tests

.PHONY: all test clean
