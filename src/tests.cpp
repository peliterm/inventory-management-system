// ---------------------------------------------------------------------------
// tests.cpp
// ---------------------------------------------------------------------------
// Unit and integration tests for the inventory management system.
//
// The suite is deliberately dependency-free: a handful of macros record each
// check and the run ends with a pass/fail count and a non-zero exit status on
// any failure, so it can be wired into a build without installing a framework.
//
// Unit tests exercise one class at a time. Integration tests drive several
// classes through the Inventory facade and assert on the combined effect, for
// example that placing a sale both records the order and moves the stock.
//
// Build:  make test
// Run:    ./run_tests
// ---------------------------------------------------------------------------

#include "ChartUtil.h"
#include "CSVUtil.h"
#include "Date.h"
#include "Inventory.h"
#include "Order.h"
#include "Product.h"
#include "Supplier.h"
#include "User.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

namespace {

int testsRun = 0;
int testsFailed = 0;
std::string currentSection;

void Section(const std::string& name) {
   currentSection = name;
   std::cout << "\n--- " << name << " ---\n";
}

// Records one check. Printing only failures keeps a passing run short enough
// to read, while a failure names the section and the expectation.
void Check(bool condition, const std::string& description) {
   ++testsRun;
   if (!condition) {
      ++testsFailed;
      std::cout << "  FAIL  [" << currentSection << "] " << description << "\n";
   }
}

// Floating-point comparison with a tolerance, since money arithmetic through
// doubles will not land on an exact value.
void CheckClose(double actual, double expected, const std::string& description) {
   Check(std::fabs(actual - expected) < 0.005, description);
}

// ---------------------------------------------------------------------------
// unit tests
// ---------------------------------------------------------------------------

void TestDate() {
   Section("Date");

   Date valid(2026, 2, 28);
   Check(valid.IsValid(), "28 February is valid in a common year");

   Date leap(2024, 2, 29);
   Check(leap.IsValid(), "29 February is valid in a leap year");

   Date notLeap(2026, 2, 29);
   Check(!notLeap.IsValid(), "29 February is rejected in a common year");

   Date century(1900, 2, 29);
   Check(!century.IsValid(), "1900 is not a leap year");

   Date century400(2000, 2, 29);
   Check(century400.IsValid(), "2000 is a leap year");

   Check(!Date(2026, 13, 1).IsValid(), "month 13 is rejected");
   Check(!Date(2026, 4, 31).IsValid(), "31 April is rejected");

   Date parsed;
   Check(Date::Parse("2026-10-09", parsed), "a well-formed date parses");
   Check(parsed.GetYear() == 2026 && parsed.GetMonth() == 10 && parsed.GetDay() == 9,
         "parsing recovers each field");
   Check(!Date::Parse("10/09/2026", parsed), "a slash-separated date is rejected");
   Check(!Date::Parse("2026-02-30", parsed), "an impossible date is rejected");

   // Regression: the year was never bounded, so a hand-edited file could
   // inject a date that does not format as YYYY-MM-DD.
   Check(!Date(1899, 1, 1).IsValid(), "a year below the accepted range is rejected");
   Check(!Date(10000, 1, 1).IsValid(), "a year above the accepted range is rejected");
   Check(!Date(-123, 1, 1).IsValid(), "a negative year is rejected");
   Check(Date(1900, 1, 1).IsValid(), "the first accepted year is valid");
   Check(Date(9999, 12, 31).IsValid(), "the last accepted year is valid");

   Check(Date(2026, 10, 9).ToString() == "2026-10-09", "formatting pads to two digits");

   Date start(2026, 1, 28);
   Check(start.AddDays(5).ToString() == "2026-02-02", "adding days crosses a month");
   Check(Date(2026, 12, 30).AddDays(5).ToString() == "2027-01-04",
         "adding days crosses a year");
   Check(Date(2024, 2, 28).AddDays(1).ToString() == "2024-02-29",
         "adding a day lands on the leap day");

   Check(Date(2026, 1, 1).DaysUntil(Date(2026, 1, 31)) == 30, "days between in a month");
   Check(Date(2026, 3, 1).DaysUntil(Date(2026, 1, 1)) == -59, "days between is signed");

   // A date converted to a serial and back must be unchanged.
   Date original(2019, 7, 4);
   Check(Date::FromSerial(original.ToSerial()).ToString() == original.ToString(),
         "serial conversion round-trips");

   Check(Date(2026, 1, 1) < Date(2026, 1, 2), "less-than orders by calendar");
   Check(Date(2026, 1, 1) == Date(2026, 1, 1), "equality compares all fields");
}

void TestCSVUtil() {
   Section("CSVUtil");

   std::vector<std::string> plain = CSVUtil::SplitLine("a,b,c");
   Check(plain.size() == 3 && plain.at(1) == "b", "a plain line splits into fields");

   std::vector<std::string> quoted = CSVUtil::SplitLine("\"Smith, John\",42,ok");
   Check(quoted.size() == 3, "a quoted comma does not split the field");
   Check(quoted.at(0) == "Smith, John", "the quoted field keeps its comma");

   std::vector<std::string> escaped = CSVUtil::SplitLine("\"He said \"\"hi\"\"\",x");
   Check(escaped.at(0) == "He said \"hi\"", "a doubled quote becomes one quote");

   std::vector<std::string> empties = CSVUtil::SplitLine("a,,c");
   Check(empties.size() == 3 && empties.at(1).empty(), "an empty field is preserved");

   Check(CSVUtil::Escape("plain") == "plain", "a plain field is not quoted");
   Check(CSVUtil::Escape("has,comma") == "\"has,comma\"", "a comma forces quoting");
   Check(CSVUtil::Escape("has\"quote") == "\"has\"\"quote\"", "a quote is doubled");

   // Escaping then splitting must give back the original field.
   std::string tricky = "Acme, \"The\" Supplier";
   std::vector<std::string> roundTrip = CSVUtil::SplitLine(CSVUtil::Escape(tricky));
   Check(roundTrip.size() == 1 && roundTrip.at(0) == tricky, "escape and split round-trip");

   int number = 0;
   Check(CSVUtil::ToInt("42", number) && number == 42, "a clean integer parses");
   Check(CSVUtil::ToInt("-7", number) && number == -7, "a negative integer parses");
   Check(!CSVUtil::ToInt("42abc", number), "trailing text is rejected");
   Check(!CSVUtil::ToInt("", number), "an empty field is rejected");
   Check(!CSVUtil::ToInt("3.5", number), "a decimal is not an integer");

   double value = 0.0;
   Check(CSVUtil::ToDouble("3.50", value) && std::fabs(value - 3.5) < 0.0001,
         "a decimal parses");
   Check(!CSVUtil::ToDouble("3.5x", value), "trailing text is rejected");

   // Regression: a spreadsheet writes a byte order mark ahead of the first
   // line, which must not survive into the first field.
   std::string withBOM = "\xEF\xBB\xBFproductID";
   Check(CSVUtil::StripBOM(withBOM) == "productID", "a UTF-8 BOM is stripped");
   Check(CSVUtil::StripBOM("plain") == "plain", "a line with no BOM is unchanged");
   Check(CSVUtil::StripBOM("") == "", "an empty line survives BOM stripping");

   // Regression: packed sub-fields must survive the delimiters they use.
   Check(CSVUtil::UnescapeDelims(CSVUtil::EscapeDelims("Kit: Deluxe")) == "Kit: Deluxe",
         "a colon round-trips through delimiter escaping");
   Check(CSVUtil::UnescapeDelims(CSVUtil::EscapeDelims("Bolt; Long")) == "Bolt; Long",
         "a semicolon round-trips through delimiter escaping");
   Check(CSVUtil::UnescapeDelims(CSVUtil::EscapeDelims("back\\slash")) == "back\\slash",
         "a backslash round-trips through delimiter escaping");
   Check(CSVUtil::UnescapeDelims(CSVUtil::EscapeDelims("all\\three:and;more")) ==
         "all\\three:and;more", "all three escaped characters round-trip together");

   std::vector<std::string> pieces = CSVUtil::SplitEscaped("a:b:c", ':');
   Check(pieces.size() == 3, "an unescaped delimiter splits");
   std::vector<std::string> protectedPieces =
         CSVUtil::SplitEscaped(CSVUtil::EscapeDelims("a:b") + ":c", ':');
   Check(protectedPieces.size() == 2, "an escaped delimiter does not split");

   // Regression: a value that does not fit in an int must be rejected, not
   // silently truncated into a different number.
   int big = 0;
   Check(!CSVUtil::ToInt("3000000000", big), "a value beyond int range is rejected");
   Check(!CSVUtil::ToInt("-3000000000", big), "a negative value beyond int range is rejected");
   Check(!CSVUtil::ToInt("99999999999999999999", big), "a value beyond long range is rejected");
   Check(CSVUtil::ToInt("2147483647", big) && big == 2147483647, "int max still parses");

   // Regression: strtod accepts the words "inf" and "nan", and one of either
   // would turn every total that reaches it into nan.
   double special = 0.0;
   Check(!CSVUtil::ToDouble("nan", special), "not-a-number is rejected");
   Check(!CSVUtil::ToDouble("inf", special), "infinity is rejected");
   Check(!CSVUtil::ToDouble("-inf", special), "negative infinity is rejected");
   Check(!CSVUtil::ToDouble("1e400", special), "an overflowing literal is rejected");
   Check(CSVUtil::ToDouble("1e300", special), "a large but finite value still parses");

   // Regression: a line break cannot survive a line-oriented reader, so it is
   // folded rather than quoted into a record that cannot be read back.
   std::string folded = CSVUtil::Escape("Line1\nLine2");
   Check(folded.find('\n') == std::string::npos, "a newline is folded out of a field");
   Check(CSVUtil::SplitLine(folded).size() == 1, "the folded field stays one field");

   Check(CSVUtil::Trim("  padded  ") == "padded", "trim removes spaces");
   Check(CSVUtil::Trim("line\r") == "line", "trim removes a carriage return");
   Check(CSVUtil::ContainsIgnoreCase("Steel Bolt", "bolt"), "search ignores case");
   Check(!CSVUtil::ContainsIgnoreCase("Steel Bolt", "nut"), "search rejects a non-match");
}

void TestProduct() {
   Section("Product");

   Product bolt("Steel Bolt", "P001", 100, 0.75, "Fasteners", "S001", 25);
   Check(bolt.GetName() == "Steel Bolt", "the name is stored");
   Check(bolt.GetProductID() == "P001", "the ID is stored");
   Check(bolt.GetQuantity() == 100, "the quantity is stored");
   CheckClose(bolt.GetPrice(), 0.75, "the price is stored");
   Check(bolt.GetCategory() == "Fasteners", "the category is stored");
   Check(bolt.GetSupplierID() == "S001", "the supplier link is stored");
   CheckClose(bolt.GetStockValue(), 75.0, "stock value is quantity times price");

   Product negative("Bad", "P002", -5, -1.0, "X");
   Check(negative.GetQuantity() == 0, "a negative quantity is clamped to zero");
   CheckClose(negative.GetPrice(), 0.0, "a negative price is clamped to zero");

   Check(bolt.UpdateStock(-40), "stock can be reduced within what is on hand");
   Check(bolt.GetQuantity() == 60, "the reduction is applied");
   Check(!bolt.UpdateStock(-100), "a reduction past zero is refused");
   Check(bolt.GetQuantity() == 60, "a refused reduction leaves stock unchanged");
   Check(bolt.UpdateStock(10), "stock can be increased");
   Check(bolt.GetQuantity() == 70, "the increase is applied");

   Check(!bolt.IsLowStock(), "70 units is above a reorder level of 25");
   bolt.SetQuantity(25);
   Check(bolt.IsLowStock(), "stock equal to the reorder level counts as low");
   bolt.SetQuantity(10);
   Check(bolt.IsLowStock(), "stock below the reorder level counts as low");

   // Regression: the old guard computed quantity + delta to decide whether
   // quantity + delta would overflow, which is undefined behaviour.
   Product huge("Huge", "P900", 2000000000, 1.0, "X");
   Check(!huge.UpdateStock(2000000000), "an increase that would overflow is refused");
   Check(huge.GetQuantity() == 2000000000, "the refused increase left stock alone");
   Check(huge.UpdateStock(100), "a safe increase still works");

   // Regression: a non-finite price would poison every total it reached.
   Product poison("Poison", "P901", 10, 0.0, "X");
   poison.SetPrice(std::nan(""));
   Check(poison.GetPrice() == 0.0, "a not-a-number price is refused");
   poison.SetPrice(-5.0);
   Check(poison.GetPrice() == 0.0, "a negative price is refused");

   // Regression: writing the price to two decimals lost sub-cent unit prices
   // on every save, and the program saves on exit.
   Product subCent("Washer", "P902", 1000, 0.004, "Fasteners", "S001", 10);
   Product subCentBack;
   Check(Product::ParseCSV(subCent.ToCSV(), subCentBack), "a sub-cent price record parses");
   CheckClose(subCentBack.GetPrice(), 0.004, "the sub-cent unit price survives the save");
   CheckClose(subCentBack.GetStockValue(), 4.00, "the stock value survives the save");

   Product restored;
   Check(Product::ParseCSV(bolt.ToCSV(), restored), "a product CSV record parses");
   Check(restored.GetProductID() == bolt.GetProductID(), "the ID survives the round-trip");
   Check(restored.GetQuantity() == bolt.GetQuantity(), "the quantity survives");
   CheckClose(restored.GetPrice(), bolt.GetPrice(), "the price survives");
   Check(restored.GetReorderLevel() == bolt.GetReorderLevel(), "the reorder level survives");

   Product commaName("Widget, Large", "P003", 5, 2.50, "Parts", "S001", 2);
   Product commaBack;
   Check(Product::ParseCSV(commaName.ToCSV(), commaBack), "a name with a comma parses");
   Check(commaBack.GetName() == "Widget, Large", "the comma in the name survives");

   // Regression: the old guard tested quantity < -delta, and negating the most
   // negative int overflows before the comparison is ever made.
   Product edge("Edge Case", "P900", 100, 1.00, "Test", "S001", 10);
   Check(!edge.UpdateStock(std::numeric_limits<int>::min()),
         "the most negative adjustment is refused");
   Check(edge.GetQuantity() == 100, "the refused adjustment left the quantity alone");
   Check(!edge.UpdateStock(std::numeric_limits<int>::max()),
         "an adjustment past the maximum is refused");
   Check(edge.GetQuantity() == 100, "the refused increase left the quantity alone");

   Product broken;
   Check(!Product::ParseCSV("P001,Name,Cat", broken), "too few fields is rejected");
   Check(!Product::ParseCSV("P001,Name,Cat,notanumber,1.00,S001,5", broken),
         "a non-numeric quantity is rejected");
}

void TestSupplier() {
   Section("Supplier");

   Supplier acme("Acme Industrial", "S001", "sales@acme.example");
   Check(acme.GetSupplierName() == "Acme Industrial", "the name is stored");
   Check(acme.GetProductCount() == 0, "a new supplier carries nothing");

   Check(acme.AddProduct("P001"), "a product can be attached");
   Check(acme.AddProduct("P002"), "a second product can be attached");
   Check(!acme.AddProduct("P001"), "attaching the same product twice is refused");
   Check(acme.GetProductCount() == 2, "the count reflects distinct products");
   Check(acme.SuppliesProduct("P002"), "an attached product is found");
   Check(!acme.SuppliesProduct("P999"), "an unattached product is not found");

   Check(acme.RemoveProduct("P001"), "an attached product can be detached");
   Check(!acme.RemoveProduct("P001"), "detaching twice is refused");
   Check(acme.GetProductCount() == 1, "the count drops after detaching");

   acme.UpdateInfo("", "new@acme.example");
   Check(acme.GetSupplierName() == "Acme Industrial", "a blank name leaves the name alone");
   Check(acme.GetContactInfo() == "new@acme.example", "the contact is replaced");

   acme.UpdateInfo("Acme Global", "");
   Check(acme.GetSupplierName() == "Acme Global", "a supplied name replaces the old one");
   Check(acme.GetContactInfo() == "new@acme.example", "a blank contact leaves it alone");

   // Regression: a product ID holding a semicolon must not split the packed
   // product list.
   Supplier odd("Odd Vendor", "S009", "x@y.example");
   odd.AddProduct("P;1");
   odd.AddProduct("P2");
   Supplier oddBack;
   Check(Supplier::ParseCSV(odd.ToCSV(), oddBack), "a supplier with an awkward ID parses");
   Check(oddBack.GetProductCount() == 2, "both product IDs survive");
   Check(oddBack.SuppliesProduct("P;1"), "the semicolon in the ID survives");

   Supplier restored;
   Check(Supplier::ParseCSV(acme.ToCSV(), restored), "a supplier CSV record parses");
   Check(restored.GetSupplierID() == "S001", "the ID survives the round-trip");
   Check(restored.GetProductCount() == acme.GetProductCount(), "the product list survives");
   Check(restored.SuppliesProduct("P002"), "the attached product survives");
}

void TestUser() {
   Section("User and permissions");

   User admin("amiller", "Alice Miller", "secret123", Role::ADMIN);
   Check(admin.CheckPassword("secret123"), "the correct password is accepted");
   Check(!admin.CheckPassword("Secret123"), "a wrong-case password is rejected");
   Check(!admin.CheckPassword(""), "an empty password is rejected");
   Check(admin.GetPasswordHash() != "secret123", "the password is not stored in the clear");

   // Regression: the digest used to come from std::hash, which is
   // implementation-defined, so data/users.csv written by one compiler
   // rejected every login under another. These literals pin the algorithm:
   // if the hash ever stops being compiler-independent, this fails here
   // rather than at the grader's login prompt.
   User sample("admin", "Default Administrator", "admin123", Role::ADMIN);
   Check(sample.GetPasswordHash() == "7c6d405becc46029",
         "the admin digest matches the value stored in data/users.csv");
   Check(sample.GetPasswordHash().size() == 16,
         "the digest is a fixed 16 hex digits on every platform");

   User manager("mpelitera", "Michael Pelitera", "manager1", Role::MANAGER);
   Check(manager.GetPasswordHash() == "1e47daa6d38cbfa0",
         "the manager digest matches the value stored in data/users.csv");

   User clerkAccount("clerk1", "Dana Ortiz", "clerk1pw", Role::CLERK);
   Check(clerkAccount.GetPasswordHash() == "956cbbf670172ed0",
         "the clerk digest matches the value stored in data/users.csv");

   // Two accounts sharing a password must not share a stored digest, which is
   // what salting with the username buys.
   User other("bjones", "Bob Jones", "secret123", Role::CLERK);
   Check(admin.GetPasswordHash() != other.GetPasswordHash(),
         "the same password hashes differently for different users");

   admin.SetPassword("newpass");
   Check(admin.CheckPassword("newpass"), "a changed password is accepted");
   Check(!admin.CheckPassword("secret123"), "the old password stops working");

   Check(User::RoleHasPermission(Role::ADMIN, Permission::MANAGE_USERS),
         "an admin may manage users");
   Check(User::RoleHasPermission(Role::ADMIN, Permission::MANAGE_PRODUCTS),
         "an admin may manage products");

   Check(!User::RoleHasPermission(Role::MANAGER, Permission::MANAGE_USERS),
         "a manager may not manage users");
   Check(User::RoleHasPermission(Role::MANAGER, Permission::MANAGE_PRODUCTS),
         "a manager may manage products");
   Check(User::RoleHasPermission(Role::MANAGER, Permission::IMPORT_EXPORT),
         "a manager may import and export");

   Check(User::RoleHasPermission(Role::CLERK, Permission::UPDATE_STOCK),
         "a clerk may adjust stock");
   Check(User::RoleHasPermission(Role::CLERK, Permission::MANAGE_ORDERS),
         "a clerk may work orders");
   Check(!User::RoleHasPermission(Role::CLERK, Permission::MANAGE_PRODUCTS),
         "a clerk may not add or remove products");
   Check(!User::RoleHasPermission(Role::CLERK, Permission::MANAGE_SUPPLIERS),
         "a clerk may not manage suppliers");
   Check(!User::RoleHasPermission(Role::CLERK, Permission::IMPORT_EXPORT),
         "a clerk may not bulk import or export");

   Role parsed = Role::CLERK;
   Check(User::RoleFromString("manager", parsed) && parsed == Role::MANAGER,
         "a role name parses regardless of case");
   Check(!User::RoleFromString("supervisor", parsed), "an unknown role is rejected");

   User restored;
   Check(User::ParseCSV(other.ToCSV(), restored), "a user CSV record parses");
   Check(restored.GetUsername() == "bjones", "the username survives the round-trip");
   Check(restored.CheckPassword("secret123"), "the stored hash still verifies after a round-trip");
}

void TestOrder() {
   Section("Order");

   Date placed(2026, 3, 1);
   Order sale("ORD-0001", OrderType::SALE, placed, "Northside Hardware");

   Check(sale.GetStatus() == OrderStatus::PROCESSING, "a new order starts processing");
   Check(sale.GetEstimatedDelivery() == placed.AddDays(Order::DEFAULT_LEAD_DAYS),
         "the estimate defaults to the standard lead time");

   Check(sale.AddProduct("P001", "Steel Bolt", 10, 0.75), "a line can be added");
   Check(sale.AddProduct("P002", "Hex Nut", 20, 0.30), "a second line can be added");
   Check(!sale.AddProduct("P003", "Washer", 0, 0.10), "a zero quantity is refused");
   Check(!sale.AddProduct("P003", "Washer", -5, 0.10), "a negative quantity is refused");

   Check(sale.GetOrderedProducts().size() == 2, "the order holds two lines");
   Check(sale.GetTotalUnits() == 30, "units are summed across lines");
   CheckClose(sale.GetOrderTotal(), 13.50, "the total is the sum of the line totals");

   // Adding the same product again combines quantities rather than creating a
   // duplicate line, which keeps the order readable and the totals correct.
   Check(sale.AddProduct("P001", "Steel Bolt", 5, 0.75), "the same product adds again");
   Check(sale.GetOrderedProducts().size() == 2, "a repeat product does not add a line");
   Check(sale.GetTotalUnits() == 35, "the repeat quantity is combined");

   // Regression: merging two huge lines used to wrap the quantity negative,
   // and a negative quantity then passed every availability check.
   Order overflow("ORD-0099", OrderType::SALE, placed, "Greedy");
   Check(overflow.AddProduct("P001", "Steel Bolt", 2000000000, 1.00),
         "a single huge line is accepted");
   Check(!overflow.AddProduct("P001", "Steel Bolt", 2000000000, 1.00),
         "a merge that would overflow the line quantity is refused");
   Check(overflow.GetTotalUnits() == 2000000000, "the refused merge left the line alone");

   // Regression: GetTotalUnits returned an int, so two large lines on separate
   // products summed to a negative number on the reports.
   Order wide("ORD-0098", OrderType::SALE, placed, "Bulk Buyer");
   Check(wide.AddProduct("P001", "Steel Bolt", 2000000000, 1.00), "first large line");
   Check(wide.AddProduct("P002", "Hex Nut", 2000000000, 1.00), "second large line");
   Check(wide.GetTotalUnits() == 4000000000LL, "units sum without wrapping negative");

   // Regression: order lines saved two decimal places while products saved
   // four, so a sub-cent price was lost the first time the file was written.
   Order pennies("ORD-0097", OrderType::SALE, placed, "Precision Buyer");
   Check(pennies.AddProduct("P001", "Steel Bolt", 100, 0.0049), "a sub-cent price is accepted");
   Order penniesBack;
   Check(Order::ParseCSV(pennies.ToCSV(), penniesBack), "the order round trips");
   CheckClose(penniesBack.GetOrderTotal(), 0.49, "the sub-cent price survives the save");

   Check(sale.RemoveProduct("P002"), "a line can be removed");
   Check(!sale.RemoveProduct("P002"), "removing twice is refused");
   Check(sale.GetOrderedProducts().size() == 1, "the line count drops");

   // Lifecycle rules.
   Check(!sale.UpdateStatus(OrderStatus::DELIVERED, placed),
         "processing cannot jump straight to delivered");
   Check(sale.UpdateStatus(OrderStatus::SHIPPED, placed), "processing may ship");
   Check(!sale.AddProduct("P004", "Screw", 1, 0.20),
         "a shipped order refuses new lines");
   Check(!sale.RemoveProduct("P001"), "a shipped order refuses line removal");

   Date arrived(2026, 3, 12);
   Check(sale.UpdateStatus(OrderStatus::DELIVERED, arrived), "shipped may deliver");

   Date actual;
   Check(sale.GetDeliveryDate(actual), "a delivered order reports a delivery date");
   Check(actual == arrived, "the recorded delivery date is the one supplied");
   Check(sale.WasLate(), "delivery after the estimate counts as late");
   Check(sale.GetDeliveryVariance() == 4, "lateness is measured in days");

   Check(!sale.UpdateStatus(OrderStatus::SHIPPED, arrived),
         "a delivered order cannot go back to shipped");
   Check(sale.UpdateStatus(OrderStatus::RETURNED, arrived), "a delivered order may be returned");
   Check(!sale.UpdateStatus(OrderStatus::DELIVERED, arrived), "returned is terminal");

   // An order delivered on the promised day is on time, not late.
   Order punctual("ORD-0002", OrderType::PURCHASE, placed, "S001");
   punctual.AddProduct("P001", "Steel Bolt", 100, 0.50);
   punctual.UpdateStatus(OrderStatus::SHIPPED, placed);
   punctual.UpdateStatus(OrderStatus::DELIVERED, punctual.GetEstimatedDelivery());
   Check(!punctual.WasLate(), "delivery on the estimate is not late");
   Check(punctual.GetDeliveryVariance() == 0, "an on-time delivery has zero variance");

   Order early("ORD-0003", OrderType::PURCHASE, placed, "S001");
   early.AddProduct("P001", "Steel Bolt", 10, 0.50);
   early.UpdateStatus(OrderStatus::SHIPPED, placed);
   early.UpdateStatus(OrderStatus::DELIVERED, placed.AddDays(2));
   Check(!early.WasLate(), "an early delivery is not late");
   Check(early.GetDeliveryVariance() < 0, "an early delivery has negative variance");

   Order cancelled("ORD-0004", OrderType::SALE, placed, "Walk-in");
   cancelled.AddProduct("P001", "Steel Bolt", 1, 0.75);
   Check(cancelled.UpdateStatus(OrderStatus::CANCELLED, placed), "processing may cancel");
   Check(!cancelled.UpdateStatus(OrderStatus::SHIPPED, placed), "cancelled is terminal");

   // Regression: a product name holding a colon or a semicolon used to split
   // the packed line field, which silently returned an order with no lines.
   Order awkward("ORD-0005", OrderType::SALE, placed, "Cust");
   awkward.AddProduct("P010", "Kit: Deluxe", 2, 5.00);
   awkward.AddProduct("P011", "Bolt; Long", 3, 2.00);

   Order awkwardBack;
   Check(Order::ParseCSV(awkward.ToCSV(), awkwardBack),
         "an order whose names hold delimiters parses");
   Check(awkwardBack.GetOrderedProducts().size() == 2, "both awkward lines survive");
   CheckClose(awkwardBack.GetOrderTotal(), 16.00, "the awkward order total is correct");
   Check(awkwardBack.GetOrderedProducts().at(0).productName == "Kit: Deluxe",
         "a colon in a product name survives");
   Check(awkwardBack.GetOrderedProducts().at(1).productName == "Bolt; Long",
         "a semicolon in a product name survives");

   // Regression: a corrupted line field must fail the record rather than
   // quietly producing an order with missing contents.
   Order corrupt;
   Check(!Order::ParseCSV(
            "ORD-0006,SALE,Cust,2026-03-01,2026-03-08,,PROCESSING,P1:Name:notanumber:1.00",
            corrupt),
         "a non-numeric quantity fails the record instead of dropping the line");
   Check(!Order::ParseCSV(
            "ORD-0007,SALE,Cust,2026-03-01,2026-03-08,,PROCESSING,P1:Name:2",
            corrupt),
         "a line segment with too few parts fails the record");

   Order noLines;
   Check(Order::ParseCSV("ORD-0008,SALE,Cust,2026-03-01,2026-03-08,,PROCESSING,", noLines),
         "an order with an empty line field still parses");
   Check(noLines.GetOrderedProducts().empty(), "that order correctly holds no lines");

   Order restored;
   Check(Order::ParseCSV(punctual.ToCSV(), restored), "an order CSV record parses");
   Check(restored.GetOrderID() == "ORD-0002", "the ID survives the round-trip");
   Check(restored.GetStatus() == OrderStatus::DELIVERED, "the status survives");
   Check(restored.GetOrderedProducts().size() == 1, "the lines survive");
   CheckClose(restored.GetOrderTotal(), punctual.GetOrderTotal(), "the total survives");

   Date restoredDate;
   Check(restored.GetDeliveryDate(restoredDate), "the delivered flag survives");
   Check(restoredDate == punctual.GetEstimatedDelivery(), "the delivery date survives");
}

void TestChartUtil() {
   Section("ChartUtil");

   std::vector<ChartUtil::Bar> bars;
   bars.push_back(ChartUtil::Bar("Alpha", 100.0));
   bars.push_back(ChartUtil::Bar("Beta", 50.0));

   std::string chart = ChartUtil::HorizontalBarChart(bars, 20, 0);
   Check(chart.find("Alpha") != std::string::npos, "the chart labels each bar");
   Check(chart.find("#") != std::string::npos, "the chart draws bars");

   // The largest value fills the width, and half that value fills half of it.
   size_t alphaLine = chart.find("Alpha");
   size_t betaLine = chart.find("Beta");
   Check(alphaLine < betaLine, "bars appear in the order supplied");

   std::vector<ChartUtil::Bar> empty;
   Check(ChartUtil::HorizontalBarChart(empty, 20, 0).find("no data") != std::string::npos,
         "an empty series reports no data instead of drawing");

   // A series of all zeros has no scale; the chart must not divide by zero.
   std::vector<ChartUtil::Bar> zeros;
   zeros.push_back(ChartUtil::Bar("Zero", 0.0));
   zeros.push_back(ChartUtil::Bar("AlsoZero", 0.0));
   std::string zeroChart = ChartUtil::HorizontalBarChart(zeros, 20, 0);
   Check(zeroChart.find("Zero") != std::string::npos, "an all-zero series still labels");

   Check(ChartUtil::MakeBar(5).size() == 5, "a bar is the requested length");
   Check(ChartUtil::MakeBar(0).empty(), "a zero-length bar is empty");

   std::string trend = ChartUtil::TrendLine(bars, 4);
   Check(trend.find("Alp") != std::string::npos, "the trend line abbreviates labels");
}

// ---------------------------------------------------------------------------
// integration tests
// ---------------------------------------------------------------------------
// These drive several classes together through the Inventory facade. Each one
// builds its own small world so the tests stay independent of each other.
// ---------------------------------------------------------------------------

// Builds a two-supplier, three-product inventory for the tests below.
Inventory BuildSampleInventory() {
   Inventory inventory;
   std::string message;

   inventory.AddSupplier(Supplier("Acme Industrial", "S001", "sales@acme.example"), message);
   inventory.AddSupplier(Supplier("Borealis Metals", "S002", "orders@borealis.example"), message);

   inventory.AddProduct(Product("Steel Bolt", "P001", 100, 0.75, "Fasteners", "S001", 25), message);
   inventory.AddProduct(Product("Hex Nut", "P002", 40, 0.30, "Fasteners", "S001", 50), message);
   inventory.AddProduct(Product("Alloy Bracket", "P003", 12, 14.95, "Brackets", "S002", 5), message);

   return inventory;
}

void TestInventoryProducts() {
   Section("Integration: product and supplier linkage");

   Inventory inventory = BuildSampleInventory();
   std::string message;

   Check(inventory.GetProducts().size() == 3, "three products are on file");
   Check(inventory.GetSuppliers().size() == 2, "two suppliers are on file");

   // Adding a product with a supplier ID must also register it on the supplier.
   Supplier acme;
   Check(inventory.GetSupplier("S001", acme), "the supplier is retrievable");
   Check(acme.GetProductCount() == 2, "the supplier gained both of its products");
   Check(acme.SuppliesProduct("P001"), "the supplier lists the first product");

   Check(!inventory.AddProduct(Product("Duplicate", "P001", 1, 1.0, "X"), message),
         "a duplicate product ID is refused");
   Check(!inventory.AddProduct(Product("No ID", "", 1, 1.0, "X"), message),
         "a blank product ID is refused");

   // A supplier cannot be deleted while the catalogue still points at it.
   Check(!inventory.RemoveSupplier("S001", message),
         "a supplier with products cannot be removed");

   Check(inventory.RemoveProduct("P001", message), "a product with no open orders is removable");
   Check(inventory.RemoveProduct("P002", message), "the second product is removable");
   Check(inventory.RemoveSupplier("S001", message), "the supplier is removable once empty");

   Supplier gone;
   Check(!inventory.GetSupplier("S001", gone), "the removed supplier is no longer found");

   // Relinking must move the product between supplier lists, not copy it.
   Inventory fresh = BuildSampleInventory();
   Check(fresh.LinkProductToSupplier("P001", "S002", message), "a product can be relinked");

   Supplier oldOwner;
   Supplier newOwner;
   fresh.GetSupplier("S001", oldOwner);
   fresh.GetSupplier("S002", newOwner);
   Check(!oldOwner.SuppliesProduct("P001"), "the previous supplier drops the product");
   Check(newOwner.SuppliesProduct("P001"), "the new supplier gains the product");

   Product moved;
   fresh.GetProduct("P001", moved);
   Check(moved.GetSupplierID() == "S002", "the product records its new supplier");

   // The Inventory error path must be safe even when Product rejects INT_MIN.
   Inventory boundary;
   boundary.AddProduct(Product("Boundary", "EDGE", 100, 1.0, "Test"), message);
   Check(!boundary.AdjustStock("EDGE", std::numeric_limits<int>::min(), message),
         "the inventory rejects the minimum integer adjustment");
   Check(message.find("2147483648") != std::string::npos,
         "the rejected decrease reports its positive magnitude without overflow");
   Product unchanged;
   boundary.GetProduct("EDGE", unchanged);
   Check(unchanged.GetQuantity() == 100, "the minimum adjustment leaves stock unchanged");
   Check(!boundary.AdjustStock("EDGE", std::numeric_limits<int>::max(), message),
         "the inventory rejects an increase past its maximum");
   Check(message.find("Cannot increase") != std::string::npos,
         "an excessive increase is described as an increase, not a decrease");
   boundary.GetProduct("EDGE", unchanged);
   Check(unchanged.GetQuantity() == 100, "the excessive increase leaves stock unchanged");
}

void TestInventorySearch() {
   Section("Integration: search and filter");

   Inventory inventory = BuildSampleInventory();

   ProductFilter all;
   Check(inventory.SearchProducts(all).size() == 3, "an empty filter returns everything");

   ProductFilter byName;
   byName.nameContains = "bolt";
   Check(inventory.SearchProducts(byName).size() == 1, "a name search ignores case");

   ProductFilter byID;
   byID.nameContains = "P00";
   Check(inventory.SearchProducts(byID).size() == 3, "the name search also matches IDs");

   ProductFilter byCategory;
   byCategory.category = "fasteners";
   Check(inventory.SearchProducts(byCategory).size() == 2, "a category filter ignores case");

   ProductFilter bySupplier;
   bySupplier.supplierID = "S002";
   Check(inventory.SearchProducts(bySupplier).size() == 1, "a supplier filter narrows results");

   ProductFilter byPrice;
   byPrice.minPrice = 1.00;
   Check(inventory.SearchProducts(byPrice).size() == 1, "a minimum price filters out cheap items");

   byPrice.minPrice = -1.0;
   byPrice.maxPrice = 1.00;
   Check(inventory.SearchProducts(byPrice).size() == 2, "a maximum price filters out dear items");

   ProductFilter combined;
   combined.category = "Fasteners";
   combined.maxPrice = 0.50;
   Check(inventory.SearchProducts(combined).size() == 1, "filters combine");

   // P002 has 40 on hand against a reorder level of 50, so it is the only
   // product currently below its threshold.
   ProductFilter low;
   low.lowStockOnly = true;
   Check(inventory.SearchProducts(low).size() == 1, "the low-stock filter finds the one item");
   Check(inventory.GetLowStockProducts().size() == 1, "the low-stock list agrees");
}

void TestInventoryOrders() {
   Section("Integration: order processing and stock movement");

   Inventory inventory = BuildSampleInventory();
   std::string message;
   Date today(2026, 5, 1);

   // A sale claims its stock the moment it is placed.
   Order sale(inventory.NextOrderID(), OrderType::SALE, today, "Northside Hardware");
   sale.AddProduct("P001", "Steel Bolt", 30, 0.75);
   Check(inventory.PlaceOrder(sale, message), "a sale within stock is accepted");

   Product bolt;
   inventory.GetProduct("P001", bolt);
   Check(bolt.GetQuantity() == 70, "placing the sale removed the stock");

   // A sale for more than is on hand must be refused outright, and must not
   // have moved any stock on the way to being refused.
   Order tooBig("ORD-9999", OrderType::SALE, today, "Greedy Buyer");
   tooBig.AddProduct("P003", "Alloy Bracket", 5, 14.95);
   tooBig.AddProduct("P001", "Steel Bolt", 5000, 0.75);
   Check(!inventory.PlaceOrder(tooBig, message), "a sale beyond stock is refused");

   Product bracket;
   inventory.GetProduct("P003", bracket);
   Check(bracket.GetQuantity() == 12, "the refused order moved no stock at all");
   inventory.GetProduct("P001", bolt);
   Check(bolt.GetQuantity() == 70, "the refused order left the other line untouched");

   Check(!inventory.PlaceOrder(sale, message), "a duplicate order ID is refused");

   Order empty("ORD-8888", OrderType::SALE, today, "Nobody");
   Check(!inventory.PlaceOrder(empty, message), "an order with no lines is refused");

   Order unknown("ORD-8887", OrderType::SALE, today, "Nobody");
   unknown.AddProduct("P404", "Ghost", 1, 1.00);
   Check(!inventory.PlaceOrder(unknown, message), "an order naming an unknown product is refused");

   // A product on an open order cannot be deleted out from under it.
   Check(!inventory.RemoveProduct("P001", message),
         "a product on an open order cannot be removed");

   // Advancing a sale through shipping and delivery does not move stock again,
   // because the stock left when the order was placed.
   Check(inventory.UpdateOrderStatus("ORD-0001", OrderStatus::SHIPPED, today, message),
         "the sale can be shipped");
   inventory.GetProduct("P001", bolt);
   Check(bolt.GetQuantity() == 70, "shipping does not move stock a second time");

   Check(inventory.UpdateOrderStatus("ORD-0001", OrderStatus::DELIVERED,
                                     today.AddDays(6), message),
         "the sale can be delivered");
   inventory.GetProduct("P001", bolt);
   Check(bolt.GetQuantity() == 70, "delivering a sale does not move stock again");

   // Regression: a delivery used to be applied line by line with no check on
   // the increases and no attention to whether UpdateStock succeeded. One line
   // could land, the next could silently fail at the integer limit, and the
   // order still reported DELIVERED.
   {
      Inventory overflowInv = BuildSampleInventory();
      std::string note;

      Product packed;
      overflowInv.GetProduct("P002", packed);
      packed.SetQuantity(std::numeric_limits<int>::max());
      Check(overflowInv.UpdateProduct("P002", packed, note), "P002 is parked at the limit");

      Order arriving(overflowInv.NextOrderID(), OrderType::PURCHASE, today, "S001");
      arriving.AddProduct("P001", "Steel Bolt", 5, 0.50);
      arriving.AddProduct("P002", "Hex Nut", 5, 0.10);
      Check(overflowInv.PlaceOrder(arriving, note), "the purchase order is placed");

      const std::string arrivingID = arriving.GetOrderID();
      Check(overflowInv.UpdateOrderStatus(arrivingID, OrderStatus::SHIPPED, today, note),
            "the purchase can ship");
      Check(!overflowInv.UpdateOrderStatus(arrivingID, OrderStatus::DELIVERED,
                                           today.AddDays(3), note),
            "a delivery that cannot be applied in full is refused");

      Product first;
      overflowInv.GetProduct("P001", first);
      Check(first.GetQuantity() == 100, "the line that could have landed did not move");

      Product second;
      overflowInv.GetProduct("P002", second);
      Check(second.GetQuantity() == std::numeric_limits<int>::max(),
            "the line at the limit did not move either");

      Order unchanged;
      Check(overflowInv.GetOrder(arrivingID, unchanged), "the order is still there");
      Check(unchanged.GetStatus() == OrderStatus::SHIPPED,
            "the refused delivery left the status where it was");
   }

   // Returning the sale puts the stock back.
   Check(inventory.UpdateOrderStatus("ORD-0001", OrderStatus::RETURNED,
                                     today.AddDays(10), message),
         "a delivered sale can be returned");
   inventory.GetProduct("P001", bolt);
   Check(bolt.GetQuantity() == 100, "a return restores the stock");

   Check(!inventory.UpdateOrderStatus("ORD-0001", OrderStatus::SHIPPED, today, message),
         "a returned order cannot be advanced again");
   Check(!inventory.UpdateOrderStatus("ORD-NONE", OrderStatus::SHIPPED, today, message),
         "an unknown order ID is refused");

   // A purchase adds stock only once it is delivered.
   Inventory second = BuildSampleInventory();
   Order purchase(second.NextOrderID(), OrderType::PURCHASE, today, "S001");
   purchase.AddProduct("P002", "Hex Nut", 200, 0.25);
   Check(second.PlaceOrder(purchase, message), "a purchase order is accepted");

   Product nut;
   second.GetProduct("P002", nut);
   Check(nut.GetQuantity() == 40, "placing a purchase does not add stock yet");

   second.UpdateOrderStatus("ORD-0001", OrderStatus::SHIPPED, today, message);
   second.GetProduct("P002", nut);
   Check(nut.GetQuantity() == 40, "shipping a purchase does not add stock");

   second.UpdateOrderStatus("ORD-0001", OrderStatus::DELIVERED, today.AddDays(5), message);
   second.GetProduct("P002", nut);
   Check(nut.GetQuantity() == 240, "delivering a purchase adds the stock");

   // Cancelling a sale releases the stock it was holding.
   Inventory third = BuildSampleInventory();
   Order toCancel(third.NextOrderID(), OrderType::SALE, today, "Changed Mind");
   toCancel.AddProduct("P003", "Alloy Bracket", 4, 14.95);
   third.PlaceOrder(toCancel, message);

   third.GetProduct("P003", bracket);
   Check(bracket.GetQuantity() == 8, "the sale took the stock");

   third.UpdateOrderStatus("ORD-0001", OrderStatus::CANCELLED, today, message);
   third.GetProduct("P003", bracket);
   Check(bracket.GetQuantity() == 12, "cancelling the sale gave the stock back");

   // Regression: a returned purchase used to keep the stock it brought in,
   // leaving the order status and the inventory permanently disagreeing.
   Inventory fifth = BuildSampleInventory();
   Order inbound(fifth.NextOrderID(), OrderType::PURCHASE, today, "S001");
   inbound.AddProduct("P002", "Hex Nut", 100, 0.25);
   fifth.PlaceOrder(inbound, message);
   fifth.UpdateOrderStatus("ORD-0001", OrderStatus::SHIPPED, today, message);
   fifth.UpdateOrderStatus("ORD-0001", OrderStatus::DELIVERED, today.AddDays(5), message);

   Product afterDelivery;
   fifth.GetProduct("P002", afterDelivery);
   Check(afterDelivery.GetQuantity() == 140, "the delivered purchase added its stock");

   Check(fifth.UpdateOrderStatus("ORD-0001", OrderStatus::RETURNED, today.AddDays(9), message),
         "a delivered purchase can be returned");
   Product afterReturn;
   fifth.GetProduct("P002", afterReturn);
   Check(afterReturn.GetQuantity() == 40, "returning the purchase removed the stock again");

   // Regression: a status change whose stock movement cannot be carried out
   // must leave the order where it was rather than recording a status the
   // inventory does not reflect.
   Inventory sixth = BuildSampleInventory();
   Order inbound2(sixth.NextOrderID(), OrderType::PURCHASE, today, "S001");
   inbound2.AddProduct("P003", "Alloy Bracket", 5, 10.00);
   sixth.PlaceOrder(inbound2, message);
   sixth.UpdateOrderStatus("ORD-0001", OrderStatus::SHIPPED, today, message);
   sixth.UpdateOrderStatus("ORD-0001", OrderStatus::DELIVERED, today, message);

   // Sell everything so the purchase can no longer be sent back.
   Order clearOut("ORD-5000", OrderType::SALE, today, "Buyer");
   clearOut.AddProduct("P003", "Alloy Bracket", 17, 14.95);
   Check(sixth.PlaceOrder(clearOut, message), "the whole shelf can be sold");

   Check(!sixth.UpdateOrderStatus("ORD-0001", OrderStatus::RETURNED, today, message),
         "a return with nothing left to send back is refused");
   Order unchanged;
   sixth.GetOrder("ORD-0001", unchanged);
   Check(unchanged.GetStatus() == OrderStatus::DELIVERED,
         "the refused return left the order status alone");

   // Regression: a product named on any order, including a closed one, must
   // not be removable, because a closed order can still be returned.
   Inventory seventh = BuildSampleInventory();
   Order closed(seventh.NextOrderID(), OrderType::SALE, today, "Buyer");
   closed.AddProduct("P001", "Steel Bolt", 4, 0.75);
   seventh.PlaceOrder(closed, message);
   seventh.UpdateOrderStatus("ORD-0001", OrderStatus::SHIPPED, today, message);
   seventh.UpdateOrderStatus("ORD-0001", OrderStatus::DELIVERED, today.AddDays(7), message);
   Check(!seventh.RemoveProduct("P001", message),
         "a product on a delivered order cannot be removed");

   // Regression: a supplier named on a purchase order must not vanish either,
   // or that order silently drops out of the efficiency report.
   Inventory eighth = BuildSampleInventory();
   Order vendorOrder(eighth.NextOrderID(), OrderType::PURCHASE, today, "S002");
   vendorOrder.AddProduct("P003", "Alloy Bracket", 5, 10.00);
   eighth.PlaceOrder(vendorOrder, message);
   eighth.RemoveProduct("P003", message);   // refused, but prove the supplier guard separately
   Check(!eighth.RemoveSupplier("S002", message),
         "a supplier named on a purchase order cannot be removed");

   // Order IDs increment without colliding.
   Inventory fourth = BuildSampleInventory();
   Check(fourth.NextOrderID() == "ORD-0001", "the first order ID is ORD-0001");
   Order first(fourth.NextOrderID(), OrderType::SALE, today, "A");
   first.AddProduct("P001", "Steel Bolt", 1, 0.75);
   fourth.PlaceOrder(first, message);
   Check(fourth.NextOrderID() == "ORD-0002", "the next order ID increments");
}

void TestInventoryStockAlerts() {
   Section("Integration: low stock alerting");

   Inventory inventory = BuildSampleInventory();
   std::string message;

   Check(inventory.GetLowStockProducts().size() == 1, "one product starts below its level");

   // Dropping P001 to its reorder level should raise an alert in the message.
   Check(inventory.AdjustStock("P001", -75, message), "stock can be adjusted down");
   Check(message.find("ALERT") != std::string::npos,
         "crossing the reorder level reports an alert");
   Check(inventory.GetLowStockProducts().size() == 2, "the low-stock list grows");

   Check(!inventory.AdjustStock("P001", -1000, message), "an over-reduction is refused");
   Check(!inventory.AdjustStock("P404", -1, message), "an unknown product is refused");

   Check(inventory.AdjustStock("P002", 100, message), "stock can be adjusted up");
   Check(inventory.GetLowStockProducts().size() == 1, "restocking clears the alert");
}

void TestInventoryUsers() {
   Section("Integration: authentication");

   Inventory inventory;
   std::string message;

   inventory.EnsureDefaultAdmin();
   Check(inventory.GetUsers().size() == 1, "a fresh system gains one default admin");

   User signedIn;
   Check(inventory.Authenticate("admin", "admin123", signedIn), "the default admin can sign in");
   Check(signedIn.GetRole() == Role::ADMIN, "the default account is an administrator");
   Check(!inventory.Authenticate("admin", "wrong", signedIn), "a wrong password is refused");
   Check(!inventory.Authenticate("nobody", "admin123", signedIn), "an unknown user is refused");

   Check(inventory.AddUser(User("clerk1", "Chris Lee", "floor", Role::CLERK), message),
         "a clerk account can be created");
   Check(!inventory.AddUser(User("clerk1", "Duplicate", "x", Role::CLERK), message),
         "a duplicate username is refused");

   Check(inventory.Authenticate("clerk1", "floor", signedIn), "the clerk can sign in");
   Check(!signedIn.HasPermission(Permission::MANAGE_PRODUCTS),
         "the signed-in clerk lacks product management");

   // The last administrator must not be removable, or nobody could ever manage
   // accounts again.
   Check(!inventory.RemoveUser("admin", message), "the only administrator cannot be removed");
   Check(inventory.AddUser(User("admin2", "Second Admin", "pw", Role::ADMIN), message),
         "a second administrator can be created");
   Check(inventory.RemoveUser("admin", message), "an administrator is removable once another exists");
   Check(inventory.RemoveUser("clerk1", message), "a clerk is removable");
   Check(!inventory.RemoveUser("clerk1", message), "removing twice is refused");

   // EnsureDefaultAdmin must not add a second account over an existing one.
   inventory.EnsureDefaultAdmin();
   Check(inventory.GetUsers().size() == 1, "the default admin is not re-created");
}

void TestInventoryCSV() {
   Section("Integration: bulk import and export");

   Inventory original = BuildSampleInventory();
   std::string message;
   Date today(2026, 6, 1);

   Order sale(original.NextOrderID(), OrderType::SALE, today, "Northside Hardware");
   sale.AddProduct("P001", "Steel Bolt", 10, 0.75);
   original.PlaceOrder(sale, message);
   original.UpdateOrderStatus("ORD-0001", OrderStatus::SHIPPED, today, message);
   original.UpdateOrderStatus("ORD-0001", OrderStatus::DELIVERED, today.AddDays(7), message);

   const std::string productPath  = "test_products.csv";
   const std::string supplierPath = "test_suppliers.csv";
   const std::string orderPath    = "test_orders.csv";

   Check(original.ExportProductsCSV(productPath, message) == 3, "three products export");
   Check(original.ExportSuppliersCSV(supplierPath, message) == 2, "two suppliers export");
   Check(original.ExportOrdersCSV(orderPath, message) == 1, "one order exports");

   Inventory reloaded;
   int skipped = 0;
   Check(reloaded.ImportSuppliersCSV(supplierPath, skipped, message) == 2,
         "two suppliers import");
   Check(reloaded.ImportProductsCSV(productPath, skipped, message) == 3,
         "three products import");
   Check(reloaded.ImportOrdersCSV(orderPath, skipped, message) == 1, "one order imports");
   Check(skipped == 0, "nothing was skipped on a clean file");

   // Everything the export wrote must survive the import unchanged.
   Product beforeProduct;
   Product afterProduct;
   original.GetProduct("P003", beforeProduct);
   reloaded.GetProduct("P003", afterProduct);
   Check(afterProduct.GetName() == beforeProduct.GetName(), "the product name survives");
   Check(afterProduct.GetQuantity() == beforeProduct.GetQuantity(), "the quantity survives");
   CheckClose(afterProduct.GetPrice(), beforeProduct.GetPrice(), "the price survives");
   Check(afterProduct.GetSupplierID() == beforeProduct.GetSupplierID(),
         "the supplier link survives");

   Order afterOrder;
   Check(reloaded.GetOrder("ORD-0001", afterOrder), "the order is found after import");
   Check(afterOrder.GetStatus() == OrderStatus::DELIVERED, "the order status survives");
   CheckClose(afterOrder.GetOrderTotal(), 7.50, "the order total survives");

   // Importing orders must not deduct stock a second time, because the export
   // already reflects the stock those orders moved.
   Product reloadedBolt;
   reloaded.GetProduct("P001", reloadedBolt);
   Check(reloadedBolt.GetQuantity() == 90, "importing history does not re-deduct stock");

   // Re-importing the same products is an update, not a duplicate.
   int before = static_cast<int>(reloaded.GetProducts().size());
   reloaded.ImportProductsCSV(productPath, skipped, message);
   Check(static_cast<int>(reloaded.GetProducts().size()) == before,
         "re-importing updates rather than duplicating");

   // A file with a bad row loads the good rows and counts the bad one.
   const std::string mixedPath = "test_mixed.csv";
   {
      std::ofstream file(mixedPath);
      file << Product::CSVHeader() << "\n";
      file << "P100,Good Widget,Parts,5,1.50,S001,2\n";
      file << "P101,Broken Row,Parts,notanumber,1.50,S001,2\n";
      file << "P102,Another Good,Parts,7,2.50,S001,3\n";
   }

   Inventory partial;
   int loaded = partial.ImportProductsCSV(mixedPath, skipped, message);
   Check(loaded == 2, "the two good rows load");
   Check(skipped == 1, "the malformed row is counted as skipped");

   Check(partial.ImportProductsCSV("no_such_file_12345.csv", skipped, message) == -1,
         "a missing file reports failure rather than crashing");

   // Regression: a row the validated add path refuses used to be counted as
   // loaded, so the import reported more records than it actually stored.
   const std::string refusedPath = "test_refused.csv";
   {
      std::ofstream refused(refusedPath);
      refused << Product::CSVHeader() << "\n";
      refused << ",No ID Here,Parts,5,1.00,,1\n";
      refused << "P300,Good One,Parts,5,1.00,,1\n";
   }

   Inventory refusedInv;
   int refusedLoaded = refusedInv.ImportProductsCSV(refusedPath, skipped, message);
   Check(refusedLoaded == 1, "only the storable row is counted as loaded");
   Check(skipped == 1, "the refused row is counted as skipped");
   Check(refusedInv.GetProducts().size() == 1, "the count matches what was stored");

   // Regression: an import must not be able to create a supplier with a blank
   // ID, which every product naming no supplier would then match.
   const std::string ghostPath = "test_ghost.csv";
   {
      std::ofstream ghost(ghostPath);
      ghost << Supplier::CSVHeader() << "\n";
      ghost << ",Ghost Vendor,nobody,\n";
   }

   Inventory ghostInv;
   Check(ghostInv.ImportSuppliersCSV(ghostPath, skipped, message) == 0,
         "a supplier with a blank ID is not imported");
   Check(ghostInv.GetSuppliers().empty(), "no ghost supplier reaches the catalogue");

   // Regression: a renamed header row used to be read as a supplier record,
   // because the supplier parser has no numeric field to fail on.
   const std::string renamedPath = "test_renamed.csv";
   {
      std::ofstream renamed(renamedPath);
      renamed << "Supplier ID,Supplier Name,Contact Info,Products\n";
      renamed << "S900,Real Vendor,real@example.com,\n";
   }

   Inventory renamedInv;
   Check(renamedInv.ImportSuppliersCSV(renamedPath, skipped, message) == 1,
         "a renamed header is recognised and only the real row loads");
   Check(renamedInv.GetSuppliers().size() == 1, "no phantom supplier is created");

   // Regression: re-importing orders used to overwrite live records, leaving
   // stock committed to an order that had been replaced with a cancelled one.
   Inventory liveInv = BuildSampleInventory();
   std::string note;
   Order live(liveInv.NextOrderID(), OrderType::SALE, Date(2026, 8, 1), "Buyer");
   live.AddProduct("P001", "Steel Bolt", 40, 0.75);
   liveInv.PlaceOrder(live, note);

   const std::string clashPath = "test_clash.csv";
   {
      std::ofstream clash(clashPath);
      clash << Order::CSVHeader() << "\n";
      clash << "ORD-0001,SALE,Buyer,2026-08-01,2026-08-08,,CANCELLED,"
               "P001:Steel Bolt:40:0.75\n";
   }

   Check(liveInv.ImportOrdersCSV(clashPath, skipped, message) == 0,
         "an order already on file is not overwritten");
   Check(skipped == 1, "the clashing record is counted as skipped");

   Order stillLive;
   liveInv.GetOrder("ORD-0001", stillLive);
   Check(stillLive.GetStatus() == OrderStatus::PROCESSING,
         "the live order keeps its own status");

   Product stillCommitted;
   liveInv.GetProduct("P001", stillCommitted);
   Check(stillCommitted.GetQuantity() == 60, "its committed stock is still committed");

   // Regression: an import must not be able to demote the only administrator.
   Inventory adminInv;
   adminInv.EnsureDefaultAdmin();

   const std::string demotePath = "test_demote.csv";
   User demoted("admin", "Demoted", "x", Role::CLERK);
   {
      std::ofstream demote(demotePath);
      demote << User::CSVHeader() << "\n";
      demote << demoted.ToCSV() << "\n";
   }

   Check(adminInv.ImportUsersCSV(demotePath, skipped, message) == 0,
         "demoting the only administrator is refused on import");
   User stillAdmin;
   Check(adminInv.Authenticate("admin", "admin123", stillAdmin),
         "the original administrator account is intact");
   Check(stillAdmin.GetRole() == Role::ADMIN, "it still holds the admin role");

   // Regression: a file saved by a spreadsheet carries a byte order mark and
   // Windows line endings. Both must load without reporting a bad line.
   const std::string excelPath = "test_excel.csv";
   {
      std::ofstream excel(excelPath, std::ios::binary);
      excel << "\xEF\xBB\xBF" << Product::CSVHeader() << "\r\n";
      excel << "P200,Imported Widget,Parts,9,3.25,S001,4\r\n";
      excel << "P201,Second Widget,Parts,11,4.50,S001,5\r\n";
   }

   Inventory fromExcel;
   int excelLoaded = fromExcel.ImportProductsCSV(excelPath, skipped, message);
   Check(excelLoaded == 2, "a spreadsheet-saved file loads both rows");
   Check(skipped == 0, "the byte order mark does not look like a malformed line");

   Product imported;
   Check(fromExcel.GetProduct("P200", imported), "the first row is found by its ID");
   Check(imported.GetName() == "Imported Widget", "the carriage return is stripped from fields");
   Check(imported.GetReorderLevel() == 4, "the last field survives the carriage return");
}

void TestInventoryReports() {
   Section("Integration: reporting and analytics");

   Inventory inventory = BuildSampleInventory();
   std::string message;

   // Two delivered sales in 2026 and one purchase delivered late.
   Date march(2026, 3, 10);
   Order sale1(inventory.NextOrderID(), OrderType::SALE, march, "Northside Hardware");
   sale1.AddProduct("P001", "Steel Bolt", 20, 0.75);
   sale1.AddProduct("P003", "Alloy Bracket", 2, 14.95);
   inventory.PlaceOrder(sale1, message);
   inventory.UpdateOrderStatus("ORD-0001", OrderStatus::SHIPPED, march, message);
   inventory.UpdateOrderStatus("ORD-0001", OrderStatus::DELIVERED, march.AddDays(7), message);

   Date july(2026, 7, 2);
   Order sale2(inventory.NextOrderID(), OrderType::SALE, july, "Westgate Supply");
   sale2.AddProduct("P001", "Steel Bolt", 10, 0.75);
   inventory.PlaceOrder(sale2, message);
   inventory.UpdateOrderStatus("ORD-0002", OrderStatus::SHIPPED, july, message);
   inventory.UpdateOrderStatus("ORD-0002", OrderStatus::DELIVERED, july.AddDays(7), message);

   Order restock(inventory.NextOrderID(), OrderType::PURCHASE, july, "S001");
   restock.AddProduct("P002", "Hex Nut", 500, 0.22);
   inventory.PlaceOrder(restock, message);
   inventory.UpdateOrderStatus("ORD-0003", OrderStatus::SHIPPED, july, message);
   inventory.UpdateOrderStatus("ORD-0003", OrderStatus::DELIVERED, july.AddDays(14), message);

   // Revenue is 20*0.75 + 2*14.95 + 10*0.75 = 15.00 + 29.90 + 7.50 = 52.40
   std::string sales = inventory.ReportSalesPerformance();
   Check(sales.find("52.40") != std::string::npos, "sales revenue is computed correctly");
   Check(sales.find("Delivered sales orders: 2") != std::string::npos,
         "delivered sales are counted");
   Check(sales.find("Steel Bolt") != std::string::npos, "the product chart names a seller");

   std::string stock = inventory.ReportStockLevels();
   Check(stock.find("STOCK LEVEL REPORT") != std::string::npos, "the stock report has a title");
   Check(stock.find("Fasteners") != std::string::npos, "the category chart names a category");
   Check(stock.find("#") != std::string::npos, "the stock report draws a chart");

   std::string low = inventory.ReportLowStock();
   Check(low.find("LOW STOCK") != std::string::npos, "the low stock report has a title");

   // The purchase was promised in 7 days and arrived in 14, so S001 is 0% on
   // time with an average of 7 days late.
   std::string efficiency = inventory.ReportSupplierEfficiency();
   Check(efficiency.find("Acme Industrial") != std::string::npos,
         "the efficiency report names the supplier");
   Check(efficiency.find("7.00") != std::string::npos,
         "the average lateness is seven days");

   std::string monthly = inventory.ReportMonthlySummary(2026);
   Check(monthly.find("MONTHLY SUMMARY FOR 2026") != std::string::npos,
         "the monthly report names the year");
   Check(monthly.find("44.90") != std::string::npos, "March revenue appears");
   Check(monthly.find("Jan") != std::string::npos, "every month is listed");

   std::string yearly = inventory.ReportYearlySummary();
   Check(yearly.find("2026") != std::string::npos, "the yearly report names the year");
   Check(yearly.find("52.40") != std::string::npos, "the yearly revenue totals the sales");

   std::string dashboard = inventory.ReportDashboard();
   Check(dashboard.find("Products: 3") != std::string::npos, "the dashboard counts products");

   // The dashboard must use the same wide unit total as the detailed reports.
   Inventory large;
   large.AddProduct(Product("Large A", "A", 2000000000, 1.0, "Test"), message);
   large.AddProduct(Product("Large B", "B", 2000000000, 1.0, "Test"), message);
   Check(large.ReportDashboard().find("Units on hand: 4000000000") != std::string::npos,
         "the dashboard totals large product quantities without integer overflow");

   // A report on an empty system must still produce readable output.
   Inventory blank;
   Check(blank.ReportStockLevels().find("no products") != std::string::npos,
         "an empty stock report says so");
   Check(blank.ReportSalesPerformance().find("Revenue") != std::string::npos,
         "an empty sales report still prints its headings");
   Check(blank.ReportYearlySummary().find("no delivered sales") != std::string::npos,
         "an empty yearly report says so");
   Check(blank.ReportMonthlySummary(2026).find("TOTAL") != std::string::npos,
         "an empty monthly report still totals");
}

// Deletes the small CSV files the import tests create beside the executable,
// so a test run leaves the project folder as it found it.
void CleanUpTestFiles() {
   const char* paths[] = {
      "test_products.csv", "test_suppliers.csv", "test_orders.csv",
      "test_mixed.csv", "test_excel.csv", "test_refused.csv",
      "test_ghost.csv", "test_renamed.csv", "test_clash.csv", "test_demote.csv"
   };
   for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
      std::remove(paths[i]);
   }
}

}   // end anonymous namespace

int main() {
   std::cout << "==================================================================\n";
   std::cout << "  Inventory Management System: unit and integration test suite\n";
   std::cout << "==================================================================\n";

   TestDate();
   TestCSVUtil();
   TestProduct();
   TestSupplier();
   TestUser();
   TestOrder();
   TestChartUtil();

   TestInventoryProducts();
   TestInventorySearch();
   TestInventoryOrders();
   TestInventoryStockAlerts();
   TestInventoryUsers();
   TestInventoryCSV();
   TestInventoryReports();

   CleanUpTestFiles();

   std::cout << "\n==================================================================\n";
   std::cout << "  " << (testsRun - testsFailed) << " of " << testsRun << " checks passed";
   if (testsFailed > 0) {
      std::cout << ", " << testsFailed << " FAILED";
   }
   std::cout << "\n==================================================================\n";

   return (testsFailed == 0) ? 0 : 1;
}
