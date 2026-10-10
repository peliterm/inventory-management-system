// ---------------------------------------------------------------------------
// main.cpp
// ---------------------------------------------------------------------------
// Command-line front end for the inventory management system.
//
// The program loads its data files on start-up, requires a login, and then
// presents a menu whose options are filtered by the signed-in user's role. All
// state lives in a single Inventory object; this file is only presentation and
// input handling, so the business rules can be tested without a console.
//
// Build:  make
// Run:    ./inventory
// ---------------------------------------------------------------------------

#include "Inventory.h"
#include "CSVUtil.h"

#include <iomanip>
#include <iostream>
#include <string>

namespace {

// Paths of the data files loaded at start-up and written on exit.
const std::string PRODUCT_FILE  = "data/products.csv";
const std::string SUPPLIER_FILE = "data/suppliers.csv";
const std::string ORDER_FILE    = "data/orders.csv";
const std::string USER_FILE     = "data/users.csv";

// --- input helpers ---------------------------------------------------------

// Reads a whole line, so values containing spaces survive intact.
std::string ReadLine(const std::string& prompt) {
   std::cout << prompt;
   std::string line;
   std::getline(std::cin, line);
   return CSVUtil::Trim(line);
}

// Reads an integer, re-prompting until the input parses. An empty line returns
// the supplied default, which lets a menu offer a sensible fallback.
int ReadInt(const std::string& prompt, int fallback) {
   while (true) {
      std::string line = ReadLine(prompt);
      if (line.empty()) {
         return fallback;
      }
      int value = 0;
      if (CSVUtil::ToInt(line, value)) {
         return value;
      }
      std::cout << "   Please enter a whole number.\n";
   }
}

double ReadDouble(const std::string& prompt, double fallback) {
   while (true) {
      std::string line = ReadLine(prompt);
      if (line.empty()) {
         return fallback;
      }
      double value = 0.0;
      if (CSVUtil::ToDouble(line, value)) {
         return value;
      }
      std::cout << "   Please enter a number.\n";
   }
}

// Reads a YYYY-MM-DD date, defaulting to today when the line is left blank.
Date ReadDate(const std::string& prompt) {
   while (true) {
      std::string line = ReadLine(prompt);
      if (line.empty()) {
         return Date::Today();
      }
      Date value;
      if (Date::Parse(line, value)) {
         return value;
      }
      std::cout << "   Please enter a date as YYYY-MM-DD.\n";
   }
}

bool Confirm(const std::string& prompt) {
   std::string answer = CSVUtil::ToLower(ReadLine(prompt + " (y/n): "));
   return answer == "y" || answer == "yes";
}

void Pause() {
   ReadLine("\n   Press Enter to continue...");
}

void Header(const std::string& title) {
   std::cout << "\n==================================================================================\n";
   std::cout << "  " << title << "\n";
   std::cout << "==================================================================================\n";
}

// Prints the message produced by an Inventory call, prefixed by whether the
// operation succeeded, so every screen reports results the same way.
void Report(bool ok, const std::string& message) {
   std::cout << (ok ? "   OK   " : "   FAIL ") << message << "\n";
}

// --- product screens -------------------------------------------------------

void ListProducts(const std::vector<Product>& list) {
   std::cout << std::left << std::setw(10) << "ID" << std::setw(26) << "NAME"
             << std::setw(14) << "CATEGORY" << std::right << std::setw(7) << "QTY"
             << std::setw(11) << "PRICE" << std::setw(13) << "VALUE" << "\n";
   std::cout << "----------------------------------------------------------------------------------\n";
   for (size_t i = 0; i < list.size(); ++i) {
      std::cout << list.at(i).GetDetails() << "\n";
   }
   if (list.empty()) {
      std::cout << "   (no matching products)\n";
   }
   std::cout << "----------------------------------------------------------------------------------\n";
   std::cout << "   " << list.size() << " product(s).\n";
}

void SearchProductScreen(const Inventory& inventory) {
   Header("Search and Filter Products");
   std::cout << "   Leave a field blank to skip it.\n\n";

   ProductFilter filter;
   filter.nameContains = ReadLine("   Name or ID contains: ");
   filter.category     = ReadLine("   Category exactly:    ");
   filter.supplierID   = ReadLine("   Supplier ID:         ");
   filter.minPrice     = ReadDouble("   Minimum price:       ", -1.0);
   filter.maxPrice     = ReadDouble("   Maximum price:       ", -1.0);
   filter.lowStockOnly = Confirm("   Low stock only?");

   std::cout << "\n";
   ListProducts(inventory.SearchProducts(filter));
}

void AddProductScreen(Inventory& inventory) {
   Header("Add Product");

   std::string id = ReadLine("   Product ID:    ");
   if (id.empty()) {
      std::cout << "   Cancelled.\n";
      return;
   }

   std::string name     = ReadLine("   Name:          ");
   std::string category = ReadLine("   Category:      ");
   int quantity         = ReadInt("   Quantity:      ", 0);
   double price         = ReadDouble("   Unit price:    ", 0.0);
   std::string supplier = ReadLine("   Supplier ID:   ");
   int reorder          = ReadInt("   Reorder level: ", 0);

   std::string message;
   bool ok = inventory.AddProduct(
         Product(name, id, quantity, price, category, supplier, reorder), message);
   Report(ok, message);
}

void EditProductScreen(Inventory& inventory) {
   Header("Edit Product");

   std::string id = ReadLine("   Product ID to edit: ");
   Product existing;
   if (!inventory.GetProduct(id, existing)) {
      Report(false, "No product with ID " + id + ".");
      return;
   }

   std::cout << "\n   Current: " << existing.GetDetails() << "\n";
   std::cout << "   Press Enter to keep the current value.\n\n";

   std::string name = ReadLine("   Name [" + existing.GetName() + "]: ");
   if (!name.empty()) {
      existing.SetName(name);
   }
   std::string category = ReadLine("   Category [" + existing.GetCategory() + "]: ");
   if (!category.empty()) {
      existing.SetCategory(category);
   }
   existing.SetPrice(ReadDouble("   Price [" + std::to_string(existing.GetPrice()) + "]: ",
                                existing.GetPrice()));
   existing.SetReorderLevel(ReadInt("   Reorder level [" +
                                    std::to_string(existing.GetReorderLevel()) + "]: ",
                                    existing.GetReorderLevel()));
   std::string supplier = ReadLine("   Supplier ID [" + existing.GetSupplierID() + "]: ");
   if (!supplier.empty()) {
      existing.SetSupplierID(supplier);
   }

   std::string message;
   Report(inventory.UpdateProduct(id, existing, message), message);
}

void AdjustStockScreen(Inventory& inventory) {
   Header("Adjust Stock");

   std::string id = ReadLine("   Product ID: ");
   Product existing;
   if (!inventory.GetProduct(id, existing)) {
      Report(false, "No product with ID " + id + ".");
      return;
   }

   std::cout << "   On hand: " << existing.GetQuantity() << "\n";
   int delta = ReadInt("   Change by (negative to remove): ", 0);

   std::string message;
   Report(inventory.AdjustStock(id, delta, message), message);
}

void ProductMenu(Inventory& inventory, const User& user) {
   while (true) {
      Header("Products");
      std::cout << "   1) List all products\n";
      std::cout << "   2) Search and filter\n";
      std::cout << "   3) Low stock alerts\n";
      if (user.HasPermission(Permission::UPDATE_STOCK)) {
         std::cout << "   4) Adjust stock quantity\n";
      }
      if (user.HasPermission(Permission::MANAGE_PRODUCTS)) {
         std::cout << "   5) Add product\n";
         std::cout << "   6) Edit product\n";
         std::cout << "   7) Remove product\n";
      }
      std::cout << "   0) Back\n";

      int choice = ReadInt("\n   Choice: ", 0);

      if (choice == 0) {
         return;
      }
      else if (choice == 1) {
         Header("All Products");
         ListProducts(inventory.GetProducts());
         Pause();
      }
      else if (choice == 2) {
         SearchProductScreen(inventory);
         Pause();
      }
      else if (choice == 3) {
         Header("Low Stock");
         std::cout << inventory.ReportLowStock();
         Pause();
      }
      else if (choice == 4 && user.HasPermission(Permission::UPDATE_STOCK)) {
         AdjustStockScreen(inventory);
         Pause();
      }
      else if (choice == 5 && user.HasPermission(Permission::MANAGE_PRODUCTS)) {
         AddProductScreen(inventory);
         Pause();
      }
      else if (choice == 6 && user.HasPermission(Permission::MANAGE_PRODUCTS)) {
         EditProductScreen(inventory);
         Pause();
      }
      else if (choice == 7 && user.HasPermission(Permission::MANAGE_PRODUCTS)) {
         Header("Remove Product");
         std::string id = ReadLine("   Product ID to remove: ");
         if (Confirm("   Remove " + id + "?")) {
            std::string message;
            Report(inventory.RemoveProduct(id, message), message);
         }
         Pause();
      }
      else {
         std::cout << "   Not a valid choice for your role.\n";
         Pause();
      }
   }
}

// --- supplier screens ------------------------------------------------------

void ListSuppliers(const std::vector<Supplier>& list) {
   std::cout << std::left << std::setw(10) << "ID" << std::setw(28) << "NAME"
             << std::setw(30) << "CONTACT" << std::right << std::setw(9) << "PRODUCTS" << "\n";
   std::cout << "----------------------------------------------------------------------------------\n";
   for (size_t i = 0; i < list.size(); ++i) {
      std::cout << list.at(i).GetDetails() << "\n";
   }
   if (list.empty()) {
      std::cout << "   (no suppliers on file)\n";
   }
   std::cout << "----------------------------------------------------------------------------------\n";
}

void SupplierMenu(Inventory& inventory, const User& user) {
   while (true) {
      Header("Suppliers");
      std::cout << "   1) List suppliers\n";
      std::cout << "   2) View supplied products\n";
      if (user.HasPermission(Permission::MANAGE_SUPPLIERS)) {
         std::cout << "   3) Add supplier\n";
         std::cout << "   4) Update supplier contact\n";
         std::cout << "   5) Remove supplier\n";
         std::cout << "   6) Link product to supplier\n";
         std::cout << "   7) Unlink product from supplier\n";
      }
      std::cout << "   0) Back\n";

      int choice = ReadInt("\n   Choice: ", 0);
      std::string message;

      if (choice == 0) {
         return;
      }
      else if (choice == 1) {
         Header("All Suppliers");
         ListSuppliers(inventory.GetSuppliers());
         Pause();
      }
      else if (choice == 2) {
         Header("Supplied Products");
         std::string id = ReadLine("   Supplier ID: ");
         Supplier found;
         if (!inventory.GetSupplier(id, found)) {
            Report(false, "No supplier with ID " + id + ".");
         }
         else {
            std::cout << "\n   " << found.GetSupplierName() << " supplies "
                      << found.GetProductCount() << " product(s):\n\n";
            const std::vector<std::string>& ids = found.GetSuppliedProducts();
            for (size_t i = 0; i < ids.size(); ++i) {
               Product product;
               if (inventory.GetProduct(ids.at(i), product)) {
                  std::cout << "   " << product.GetDetails() << "\n";
               }
               else {
                  std::cout << "   " << ids.at(i) << "   (no catalogue record)\n";
               }
            }
         }
         Pause();
      }
      else if (choice == 3 && user.HasPermission(Permission::MANAGE_SUPPLIERS)) {
         Header("Add Supplier");
         std::string id      = ReadLine("   Supplier ID: ");
         std::string name    = ReadLine("   Name:        ");
         std::string contact = ReadLine("   Contact:     ");
         Report(inventory.AddSupplier(Supplier(name, id, contact), message), message);
         Pause();
      }
      else if (choice == 4 && user.HasPermission(Permission::MANAGE_SUPPLIERS)) {
         Header("Update Supplier");
         std::string id      = ReadLine("   Supplier ID: ");
         std::string name    = ReadLine("   New name (blank to keep):    ");
         std::string contact = ReadLine("   New contact (blank to keep): ");
         Report(inventory.UpdateSupplier(id, name, contact, message), message);
         Pause();
      }
      else if (choice == 5 && user.HasPermission(Permission::MANAGE_SUPPLIERS)) {
         Header("Remove Supplier");
         std::string id = ReadLine("   Supplier ID: ");
         if (Confirm("   Remove " + id + "?")) {
            Report(inventory.RemoveSupplier(id, message), message);
         }
         Pause();
      }
      else if (choice == 6 && user.HasPermission(Permission::MANAGE_SUPPLIERS)) {
         Header("Link Product to Supplier");
         std::string productID  = ReadLine("   Product ID:  ");
         std::string supplierID = ReadLine("   Supplier ID: ");
         Report(inventory.LinkProductToSupplier(productID, supplierID, message), message);
         Pause();
      }
      else if (choice == 7 && user.HasPermission(Permission::MANAGE_SUPPLIERS)) {
         Header("Unlink Product from Supplier");
         std::string productID  = ReadLine("   Product ID:  ");
         std::string supplierID = ReadLine("   Supplier ID: ");
         Report(inventory.UnlinkProductFromSupplier(productID, supplierID, message), message);
         Pause();
      }
      else {
         std::cout << "   Not a valid choice for your role.\n";
         Pause();
      }
   }
}

// --- order screens ---------------------------------------------------------

void ListOrders(const std::vector<Order>& list) {
   std::cout << std::left << std::setw(10) << "ID" << std::setw(10) << "TYPE"
             << std::setw(20) << "COUNTERPARTY" << std::setw(12) << "ORDERED"
             << std::setw(12) << "STATUS" << std::right << std::setw(6) << "UNITS"
             << std::setw(12) << "TOTAL" << "\n";
   std::cout << "----------------------------------------------------------------------------------\n";
   for (size_t i = 0; i < list.size(); ++i) {
      std::cout << list.at(i).GetDetails() << "\n";
   }
   if (list.empty()) {
      std::cout << "   (no orders on file)\n";
   }
   std::cout << "----------------------------------------------------------------------------------\n";
}

// Collects product lines from the operator until a blank ID is entered.
void BuildOrderLines(Inventory& inventory, Order& order) {
   std::cout << "\n   Add products. Leave the ID blank to finish.\n";
   while (true) {
      std::string productID = ReadLine("\n   Product ID: ");
      if (productID.empty()) {
         return;
      }

      Product product;
      if (!inventory.GetProduct(productID, product)) {
         std::cout << "   No product with ID " << productID << ".\n";
         continue;
      }

      std::cout << "   " << product.GetName() << ", " << product.GetQuantity()
                << " on hand at " << std::fixed << std::setprecision(2)
                << product.GetPrice() << " each.\n";

      int quantity = ReadInt("   Quantity: ", 0);
      if (quantity <= 0) {
         std::cout << "   Quantity must be positive.\n";
         continue;
      }

      if (order.AddProduct(productID, product.GetName(), quantity, product.GetPrice())) {
         std::cout << "   Added " << quantity << " x " << product.GetName() << ".\n";
      }
      else {
         std::cout << "   Could not add that line.\n";
      }
   }
}

void PlaceOrderScreen(Inventory& inventory, OrderType type) {
   Header(type == OrderType::SALE ? "Place Sales Order" : "Place Purchase Order");

   std::string id = inventory.NextOrderID();
   std::cout << "   New order ID: " << id << "\n";

   std::string counterparty = ReadLine(
         type == OrderType::SALE ? "   Customer name: " : "   Supplier ID:   ");

   Date placed = ReadDate("   Order date [today]: ");
   Order order(id, type, placed, counterparty);

   int leadDays = ReadInt("   Lead time in days [" +
                          std::to_string(Order::DEFAULT_LEAD_DAYS) + "]: ",
                          Order::DEFAULT_LEAD_DAYS);
   order.SetEstimatedDelivery(placed.AddDays(leadDays));

   BuildOrderLines(inventory, order);

   std::cout << "\n   Order total: " << std::fixed << std::setprecision(2)
             << order.GetOrderTotal() << " across " << order.GetTotalUnits() << " unit(s).\n";

   if (!Confirm("   Place this order?")) {
      std::cout << "   Cancelled.\n";
      return;
   }

   std::string message;
   Report(inventory.PlaceOrder(order, message), message);
}

void AdvanceOrderScreen(Inventory& inventory) {
   Header("Update Order Status");

   std::string id = ReadLine("   Order ID: ");
   Order order;
   if (!inventory.GetOrder(id, order)) {
      Report(false, "No order with ID " + id + ".");
      return;
   }

   std::cout << "\n   " << order.GetDetails() << "\n";
   std::cout << "   Current status: " << Order::StatusToString(order.GetStatus()) << "\n\n";
   std::cout << "   1) SHIPPED\n   2) DELIVERED\n   3) RETURNED\n   4) CANCELLED\n   0) Cancel\n";

   int choice = ReadInt("\n   New status: ", 0);
   OrderStatus next = OrderStatus::SHIPPED;

   if (choice == 1)      { next = OrderStatus::SHIPPED; }
   else if (choice == 2) { next = OrderStatus::DELIVERED; }
   else if (choice == 3) { next = OrderStatus::RETURNED; }
   else if (choice == 4) { next = OrderStatus::CANCELLED; }
   else                  { std::cout << "   Cancelled.\n"; return; }

   Date when = ReadDate("   Effective date [today]: ");
   std::string message;
   Report(inventory.UpdateOrderStatus(id, next, when, message), message);
}

void ViewOrderScreen(const Inventory& inventory) {
   Header("Order Detail");

   std::string id = ReadLine("   Order ID: ");
   Order order;
   if (!inventory.GetOrder(id, order)) {
      Report(false, "No order with ID " + id + ".");
      return;
   }

   std::cout << "\n   Order:        " << order.GetOrderID() << "\n";
   std::cout << "   Type:         " << Order::TypeToString(order.GetOrderType()) << "\n";
   std::cout << "   Counterparty: " << order.GetCounterpartyID() << "\n";
   std::cout << "   Ordered:      " << order.GetOrderDate().ToString() << "\n";
   std::cout << "   Estimated:    " << order.GetEstimatedDelivery().ToString() << "\n";

   Date actual;
   if (order.GetDeliveryDate(actual)) {
      std::cout << "   Delivered:    " << actual.ToString();
      long variance = order.GetDeliveryVariance();
      if (variance > 0) {
         std::cout << "   (" << variance << " day(s) late)";
      }
      else if (variance < 0) {
         std::cout << "   (" << -variance << " day(s) early)";
      }
      else {
         std::cout << "   (on time)";
      }
      std::cout << "\n";
   }
   std::cout << "   Status:       " << Order::StatusToString(order.GetStatus()) << "\n\n";

   std::cout << std::left << std::setw(12) << "PRODUCT" << std::setw(26) << "NAME"
             << std::right << std::setw(8) << "QTY" << std::setw(12) << "UNIT"
             << std::setw(14) << "LINE TOTAL" << "\n";
   std::cout << "----------------------------------------------------------------------------------\n";

   const std::vector<OrderLine>& lines = order.GetOrderedProducts();
   for (size_t i = 0; i < lines.size(); ++i) {
      std::cout << std::left << std::setw(12) << lines.at(i).productID
                << std::setw(26) << lines.at(i).productName
                << std::right << std::setw(8) << lines.at(i).quantity
                << std::setw(12) << std::fixed << std::setprecision(2) << lines.at(i).unitPrice
                << std::setw(14) << lines.at(i).GetLineTotal() << "\n";
   }

   std::cout << "----------------------------------------------------------------------------------\n";
   std::cout << "   Total: " << std::fixed << std::setprecision(2) << order.GetOrderTotal() << "\n";
}

void OrderMenu(Inventory& inventory, const User& user) {
   while (true) {
      Header("Orders");
      std::cout << "   1) List orders\n";
      std::cout << "   2) View order detail\n";
      if (user.HasPermission(Permission::MANAGE_ORDERS)) {
         std::cout << "   3) Place sales order\n";
         std::cout << "   4) Place purchase order\n";
         std::cout << "   5) Update order status\n";
         std::cout << "   6) Change delivery estimate\n";
      }
      std::cout << "   0) Back\n";

      int choice = ReadInt("\n   Choice: ", 0);
      std::string message;

      if (choice == 0) {
         return;
      }
      else if (choice == 1) {
         Header("All Orders");
         ListOrders(inventory.GetOrders());
         Pause();
      }
      else if (choice == 2) {
         ViewOrderScreen(inventory);
         Pause();
      }
      else if (choice == 3 && user.HasPermission(Permission::MANAGE_ORDERS)) {
         PlaceOrderScreen(inventory, OrderType::SALE);
         Pause();
      }
      else if (choice == 4 && user.HasPermission(Permission::MANAGE_ORDERS)) {
         PlaceOrderScreen(inventory, OrderType::PURCHASE);
         Pause();
      }
      else if (choice == 5 && user.HasPermission(Permission::MANAGE_ORDERS)) {
         AdvanceOrderScreen(inventory);
         Pause();
      }
      else if (choice == 6 && user.HasPermission(Permission::MANAGE_ORDERS)) {
         Header("Change Delivery Estimate");
         std::string id = ReadLine("   Order ID: ");
         Date estimate = ReadDate("   New estimated delivery [today]: ");
         Report(inventory.SetDeliveryEstimate(id, estimate, message), message);
         Pause();
      }
      else {
         std::cout << "   Not a valid choice for your role.\n";
         Pause();
      }
   }
}

// --- reporting, data, and account screens ----------------------------------

void ReportMenu(const Inventory& inventory) {
   while (true) {
      Header("Reports and Analytics");
      std::cout << "   1) Stock levels\n";
      std::cout << "   2) Low stock alerts\n";
      std::cout << "   3) Sales performance\n";
      std::cout << "   4) Supplier efficiency\n";
      std::cout << "   5) Monthly summary\n";
      std::cout << "   6) Yearly summary\n";
      std::cout << "   0) Back\n";

      int choice = ReadInt("\n   Choice: ", 0);

      if (choice == 0) {
         return;
      }
      else if (choice == 1) {
         std::cout << "\n" << inventory.ReportStockLevels();
      }
      else if (choice == 2) {
         std::cout << "\n" << inventory.ReportLowStock();
      }
      else if (choice == 3) {
         std::cout << "\n" << inventory.ReportSalesPerformance();
      }
      else if (choice == 4) {
         std::cout << "\n" << inventory.ReportSupplierEfficiency();
      }
      else if (choice == 5) {
         int year = ReadInt("   Year [" + std::to_string(Date::Today().GetYear()) + "]: ",
                            Date::Today().GetYear());
         std::cout << "\n" << inventory.ReportMonthlySummary(year);
      }
      else if (choice == 6) {
         std::cout << "\n" << inventory.ReportYearlySummary();
      }
      else {
         std::cout << "   Not a valid choice.\n";
      }
      Pause();
   }
}

void DataMenu(Inventory& inventory) {
   while (true) {
      Header("Import and Export");
      std::cout << "   1) Import products from CSV\n";
      std::cout << "   2) Export products to CSV\n";
      std::cout << "   3) Import suppliers from CSV\n";
      std::cout << "   4) Export suppliers to CSV\n";
      std::cout << "   5) Import orders from CSV\n";
      std::cout << "   6) Export orders to CSV\n";
      std::cout << "   7) Save everything to the default files\n";
      std::cout << "   0) Back\n";

      int choice = ReadInt("\n   Choice: ", 0);
      std::string message;
      int skipped = 0;

      if (choice == 0) {
         return;
      }
      else if (choice == 1) {
         std::string path = ReadLine("   Path [" + PRODUCT_FILE + "]: ");
         if (path.empty()) { path = PRODUCT_FILE; }
         Report(inventory.ImportProductsCSV(path, skipped, message) >= 0, message);
      }
      else if (choice == 2) {
         std::string path = ReadLine("   Path [" + PRODUCT_FILE + "]: ");
         if (path.empty()) { path = PRODUCT_FILE; }
         Report(inventory.ExportProductsCSV(path, message) >= 0, message);
      }
      else if (choice == 3) {
         std::string path = ReadLine("   Path [" + SUPPLIER_FILE + "]: ");
         if (path.empty()) { path = SUPPLIER_FILE; }
         Report(inventory.ImportSuppliersCSV(path, skipped, message) >= 0, message);
      }
      else if (choice == 4) {
         std::string path = ReadLine("   Path [" + SUPPLIER_FILE + "]: ");
         if (path.empty()) { path = SUPPLIER_FILE; }
         Report(inventory.ExportSuppliersCSV(path, message) >= 0, message);
      }
      else if (choice == 5) {
         std::string path = ReadLine("   Path [" + ORDER_FILE + "]: ");
         if (path.empty()) { path = ORDER_FILE; }
         Report(inventory.ImportOrdersCSV(path, skipped, message) >= 0, message);
      }
      else if (choice == 6) {
         std::string path = ReadLine("   Path [" + ORDER_FILE + "]: ");
         if (path.empty()) { path = ORDER_FILE; }
         Report(inventory.ExportOrdersCSV(path, message) >= 0, message);
      }
      else if (choice == 7) {
         Report(inventory.ExportProductsCSV(PRODUCT_FILE, message) >= 0, message);
         Report(inventory.ExportSuppliersCSV(SUPPLIER_FILE, message) >= 0, message);
         Report(inventory.ExportOrdersCSV(ORDER_FILE, message) >= 0, message);
         Report(inventory.ExportUsersCSV(USER_FILE, message) >= 0, message);
      }
      else {
         std::cout << "   Not a valid choice.\n";
      }
      Pause();
   }
}

void UserMenu(Inventory& inventory) {
   while (true) {
      Header("User Accounts");
      std::cout << "   1) List accounts\n";
      std::cout << "   2) Create account\n";
      std::cout << "   3) Remove account\n";
      std::cout << "   0) Back\n";

      int choice = ReadInt("\n   Choice: ", 0);
      std::string message;

      if (choice == 0) {
         return;
      }
      else if (choice == 1) {
         Header("All Accounts");
         std::cout << std::left << std::setw(16) << "USERNAME" << std::setw(26) << "FULL NAME"
                   << std::setw(10) << "ROLE" << "\n";
         std::cout << "----------------------------------------------------------------------------------\n";
         const std::vector<User>& list = inventory.GetUsers();
         for (size_t i = 0; i < list.size(); ++i) {
            std::cout << list.at(i).GetDetails() << "\n";
         }
         std::cout << "----------------------------------------------------------------------------------\n";
      }
      else if (choice == 2) {
         Header("Create Account");
         std::string username = ReadLine("   Username:  ");
         std::string fullName = ReadLine("   Full name: ");
         std::string password = ReadLine("   Password:  ");
         std::cout << "   1) ADMIN   2) MANAGER   3) CLERK\n";
         int roleChoice = ReadInt("   Role [3]: ", 3);

         Role role = Role::CLERK;
         if (roleChoice == 1)      { role = Role::ADMIN; }
         else if (roleChoice == 2) { role = Role::MANAGER; }

         Report(inventory.AddUser(User(username, fullName, password, role), message), message);
      }
      else if (choice == 3) {
         Header("Remove Account");
         std::string username = ReadLine("   Username: ");
         if (Confirm("   Remove " + username + "?")) {
            Report(inventory.RemoveUser(username, message), message);
         }
      }
      else {
         std::cout << "   Not a valid choice.\n";
      }
      Pause();
   }
}

// --- start-up and main loop ------------------------------------------------

// Loads the four data files, reporting what was found. Missing files are not
// an error on a first run; the system simply starts empty.
void LoadAll(Inventory& inventory) {
   std::string message;
   int skipped = 0;

   std::cout << "Loading data files...\n";
   if (inventory.ImportUsersCSV(USER_FILE, skipped, message) >= 0)      { std::cout << "   " << message << "\n"; }
   if (inventory.ImportSuppliersCSV(SUPPLIER_FILE, skipped, message) >= 0) { std::cout << "   " << message << "\n"; }
   if (inventory.ImportProductsCSV(PRODUCT_FILE, skipped, message) >= 0)   { std::cout << "   " << message << "\n"; }
   if (inventory.ImportOrdersCSV(ORDER_FILE, skipped, message) >= 0)       { std::cout << "   " << message << "\n"; }

   inventory.EnsureDefaultAdmin();
}

// Prompts for credentials, allowing three attempts before giving up.
bool LoginScreen(const Inventory& inventory, User& signedIn) {
   Header("Inventory Management System");
   std::cout << "   Sign in to continue. Default account: admin / admin123\n\n";

   for (int attempt = 1; attempt <= 3; ++attempt) {
      std::string username = ReadLine("   Username: ");
      std::string password = ReadLine("   Password: ");

      if (inventory.Authenticate(username, password, signedIn)) {
         std::cout << "\n   Welcome, " << signedIn.GetFullName() << " ("
                   << User::RoleToString(signedIn.GetRole()) << ").\n";
         return true;
      }

      std::cout << "   Incorrect username or password. "
                << (3 - attempt) << " attempt(s) left.\n\n";
   }

   return false;
}

}   // end anonymous namespace

int main() {
   Inventory inventory;
   LoadAll(inventory);

   User signedIn;
   if (!LoginScreen(inventory, signedIn)) {
      std::cout << "\nToo many failed attempts. Exiting.\n";
      return 1;
   }

   while (true) {
      Header("Main Menu");
      std::cout << inventory.ReportDashboard() << "\n";
      std::cout << "   Signed in as " << signedIn.GetUsername() << " ("
                << User::RoleToString(signedIn.GetRole()) << ")\n\n";
      std::cout << "   1) Products\n";
      std::cout << "   2) Suppliers\n";
      std::cout << "   3) Orders\n";
      if (signedIn.HasPermission(Permission::VIEW_REPORTS)) {
         std::cout << "   4) Reports and analytics\n";
      }
      if (signedIn.HasPermission(Permission::IMPORT_EXPORT)) {
         std::cout << "   5) Import and export\n";
      }
      if (signedIn.HasPermission(Permission::MANAGE_USERS)) {
         std::cout << "   6) User accounts\n";
      }
      std::cout << "   0) Save and exit\n";

      int choice = ReadInt("\n   Choice: ", 0);

      if (choice == 0) {
         std::string message;
         std::cout << "\nSaving...\n";
         inventory.ExportProductsCSV(PRODUCT_FILE, message);   std::cout << "   " << message << "\n";
         inventory.ExportSuppliersCSV(SUPPLIER_FILE, message); std::cout << "   " << message << "\n";
         inventory.ExportOrdersCSV(ORDER_FILE, message);       std::cout << "   " << message << "\n";
         inventory.ExportUsersCSV(USER_FILE, message);         std::cout << "   " << message << "\n";
         std::cout << "\nGoodbye.\n";
         return 0;
      }
      else if (choice == 1) {
         ProductMenu(inventory, signedIn);
      }
      else if (choice == 2) {
         SupplierMenu(inventory, signedIn);
      }
      else if (choice == 3) {
         OrderMenu(inventory, signedIn);
      }
      else if (choice == 4 && signedIn.HasPermission(Permission::VIEW_REPORTS)) {
         ReportMenu(inventory);
      }
      else if (choice == 5 && signedIn.HasPermission(Permission::IMPORT_EXPORT)) {
         DataMenu(inventory);
      }
      else if (choice == 6 && signedIn.HasPermission(Permission::MANAGE_USERS)) {
         UserMenu(inventory);
      }
      else {
         std::cout << "   Not a valid choice for your role.\n";
         Pause();
      }
   }
}
