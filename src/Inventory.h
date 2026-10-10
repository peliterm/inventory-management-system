#ifndef INVENTORY_H
#define INVENTORY_H

#include "Date.h"
#include "Order.h"
#include "Product.h"
#include "Supplier.h"
#include "User.h"

#include <string>
#include <vector>

// Criteria for the product search and filter screen. An empty string or a
// negative number means "do not filter on this field", so one struct covers
// every combination the menu offers.
struct ProductFilter {
   std::string nameContains;
   std::string category;
   std::string supplierID;
   double minPrice;
   double maxPrice;
   bool lowStockOnly;

   ProductFilter();
};

// ---------------------------------------------------------------------------
// Inventory
// ---------------------------------------------------------------------------
// The system's central store and the only class that changes stock. Products,
// suppliers, orders, and user accounts all live here, and every operation that
// touches more than one of them, such as placing an order or deleting a
// product that suppliers still reference, is implemented as a method on this
// class rather than being left to the caller to coordinate.
// ---------------------------------------------------------------------------
class Inventory {
private:
   std::vector<Product> products;
   std::vector<Supplier> suppliers;
   std::vector<Order> orders;
   std::vector<User> users;

   // Returns the index of a record, or -1 when no record carries that ID.
   int FindProductIndex(const std::string& productID) const;
   int FindSupplierIndex(const std::string& supplierID) const;
   int FindOrderIndex(const std::string& orderID) const;
   int FindUserIndex(const std::string& username) const;

   // Number of accounts holding the ADMIN role, used by the guards that stop
   // the system being left with nobody who can manage accounts.
   int CountAdmins() const;

   // Which way stock moves for a transition: +1 adds the line quantities,
   // -1 removes them, 0 leaves stock alone.
   static int StockDirectionFor(OrderType type, OrderStatus from, OrderStatus next);

   // Applies the stock movement an order implies when it reaches a new status.
   // Every line is checked before any stock moves, so the method either
   // carries out the whole movement or changes nothing and returns false.
   bool ApplyStockForStatus(Order& order, OrderStatus next, std::string& message);

public:
   Inventory();

   // --- product management -------------------------------------------------

   // Adds a product. Fails when the ID is blank or already in use. On success
   // the product is also attached to its supplier when one is named.
   bool AddProduct(const Product& product, std::string& message);

   // Removes a product and detaches it from every supplier that carried it.
   // Fails when the product appears on an order that is still open.
   bool RemoveProduct(const std::string& productID, std::string& message);

   // Replaces an existing product record, keeping its ID.
   bool UpdateProduct(const std::string& productID, const Product& updated,
                      std::string& message);

   // Adjusts stock by delta, rejecting a change that would go below zero.
   bool AdjustStock(const std::string& productID, int delta, std::string& message);

   // Copies a product out by ID. Returns false when no such product exists.
   bool GetProduct(const std::string& productID, Product& out) const;

   const std::vector<Product>& GetProducts() const;

   // Returns every product matching the filter, in catalogue order.
   std::vector<Product> SearchProducts(const ProductFilter& filter) const;

   // Returns every product at or below its reorder level.
   std::vector<Product> GetLowStockProducts() const;

   // --- supplier management ------------------------------------------------

   bool AddSupplier(const Supplier& supplier, std::string& message);

   // Removes a supplier. Fails while any product still names it, so the
   // catalogue cannot end up pointing at a vendor that no longer exists.
   bool RemoveSupplier(const std::string& supplierID, std::string& message);

   bool UpdateSupplier(const std::string& supplierID, const std::string& name,
                       const std::string& contact, std::string& message);

   bool GetSupplier(const std::string& supplierID, Supplier& out) const;
   const std::vector<Supplier>& GetSuppliers() const;

   // Links or unlinks a product and a supplier, updating both records.
   bool LinkProductToSupplier(const std::string& productID,
                              const std::string& supplierID, std::string& message);
   bool UnlinkProductFromSupplier(const std::string& productID,
                                  const std::string& supplierID, std::string& message);

   // --- order processing ---------------------------------------------------

   // Places an order. A SALE checks availability and removes the stock at once
   // so two sales cannot both claim the last unit; a PURCHASE adds stock only
   // when it is delivered. Fails when a line names an unknown product, asks for
   // more than is on hand, or the order has no lines.
   bool PlaceOrder(const Order& order, std::string& message);

   // Advances an order and applies the matching stock movement. A delivered
   // purchase adds stock, a returned or cancelled sale puts stock back.
   bool UpdateOrderStatus(const std::string& orderID, OrderStatus next,
                          const Date& when, std::string& message);

   // Sets the promised delivery date on an open order.
   bool SetDeliveryEstimate(const std::string& orderID, const Date& estimate,
                            std::string& message);

   bool GetOrder(const std::string& orderID, Order& out) const;
   const std::vector<Order>& GetOrders() const;

   // Generates the next unused order ID in the form ORD-0001.
   std::string NextOrderID() const;

   // --- user accounts ------------------------------------------------------

   bool AddUser(const User& user, std::string& message);
   bool RemoveUser(const std::string& username, std::string& message);

   // Checks a username and password. On success copies the account into out.
   bool Authenticate(const std::string& username, const std::string& password,
                     User& out) const;

   const std::vector<User>& GetUsers() const;

   // Creates the default administrator account when no users exist, so a fresh
   // installation can be logged into.
   void EnsureDefaultAdmin();

   // --- bulk import and export ---------------------------------------------
   // Each returns the number of records actually stored, or -1 when the file
   // could not be opened. A line that will not parse, that the validated add
   // path refuses, or that would overwrite a record whose effects cannot be
   // undone is counted in skipped rather than aborting the import, so one bad
   // row does not discard a good file and the returned count never overstates
   // what was loaded.

   int ImportProductsCSV(const std::string& path, int& skipped, std::string& message);
   int ExportProductsCSV(const std::string& path, std::string& message) const;
   int ImportSuppliersCSV(const std::string& path, int& skipped, std::string& message);
   int ExportSuppliersCSV(const std::string& path, std::string& message) const;
   int ImportOrdersCSV(const std::string& path, int& skipped, std::string& message);
   int ExportOrdersCSV(const std::string& path, std::string& message) const;
   int ImportUsersCSV(const std::string& path, int& skipped, std::string& message);
   int ExportUsersCSV(const std::string& path, std::string& message) const;

   // --- reporting and analytics --------------------------------------------

   // Full stock listing with totals and a chart of value held per category.
   std::string ReportStockLevels() const;

   // Products at or below their reorder level, most urgent first.
   std::string ReportLowStock() const;

   // Revenue and units from delivered sales, with a chart of the top sellers.
   std::string ReportSalesPerformance() const;

   // On-time delivery rate and average lateness for each supplier.
   std::string ReportSupplierEfficiency() const;

   // Month-by-month revenue for one calendar year, with a trend chart.
   std::string ReportMonthlySummary(int year) const;

   // Year-by-year revenue and order counts across all recorded sales.
   std::string ReportYearlySummary() const;

   // Counts and headline totals for the dashboard shown after login.
   std::string ReportDashboard() const;
};

#endif
