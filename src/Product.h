#ifndef PRODUCT_H
#define PRODUCT_H

#include <string>

// ---------------------------------------------------------------------------
// Product
// ---------------------------------------------------------------------------
// A single stock-keeping item held in the inventory. Beyond the five core
// attributes the project requires, each product carries the identifier of the
// supplier it is purchased from and a reorder level, which together support
// the low-stock alerting and supplier-relations features.
// ---------------------------------------------------------------------------
class Product {
private:
   std::string name;
   std::string productID;
   int quantity;
   double price;
   std::string category;
   std::string supplierID;   // links the product to a Supplier record
   int reorderLevel;         // quantity at or below which the item is "low"

public:
   // Creates an empty product with zero quantity, price, and reorder level.
   Product();

   // Creates a fully populated product record.
   Product(const std::string& productName, const std::string& id, int qty,
           double unitPrice, const std::string& cat,
           const std::string& supplier = "", int reorder = 0);

   std::string GetName() const;
   std::string GetProductID() const;
   int GetQuantity() const;
   double GetPrice() const;
   std::string GetCategory() const;
   std::string GetSupplierID() const;
   int GetReorderLevel() const;

   void SetName(const std::string& productName);
   void SetProductID(const std::string& id);

   // Replaces the stock count outright. Negative values are clamped to zero
   // so the inventory can never report impossible stock.
   void SetQuantity(int qty);

   void SetPrice(double unitPrice);
   void SetCategory(const std::string& cat);
   void SetSupplierID(const std::string& supplier);
   void SetReorderLevel(int reorder);

   // Adds delta to the stock count, where delta may be negative. Returns false
   // and leaves the quantity unchanged when the change would drive stock below
   // zero, which is how the order system detects an unfillable sale, or when
   // it would overflow the counter. Callers must check the return value; a
   // refused change is not a no-op to be ignored.
   bool UpdateStock(int delta);

   // True when stock has fallen to or below the reorder level.
   bool IsLowStock() const;

   // Quantity multiplied by unit price, the item's contribution to the value
   // of inventory on hand.
   double GetStockValue() const;

   // A single-line human-readable summary used by the CLI listings.
   std::string GetDetails() const;

   // Serializes to one CSV record. Field order matches ParseCSV(). The price
   // is written to four decimal places rather than two so a sub-cent unit
   // price survives the save-and-reload cycle; the display columns still show
   // two places.
   std::string ToCSV() const;

   // Builds a Product from one CSV record. Returns false when the line has the
   // wrong number of fields or a non-numeric quantity, price, or reorder level.
   static bool ParseCSV(const std::string& line, Product& out);

   // Column header written at the top of an exported product file.
   static std::string CSVHeader();
};

#endif
