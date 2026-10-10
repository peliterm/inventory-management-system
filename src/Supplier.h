#ifndef SUPPLIER_H
#define SUPPLIER_H

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Supplier
// ---------------------------------------------------------------------------
// A vendor the business buys stock from. The supplied-products list holds
// product identifiers rather than Product objects, so a product can be edited
// or removed in one place without leaving stale copies inside every supplier
// that carries it.
// ---------------------------------------------------------------------------
class Supplier {
private:
   std::string supplierName;
   std::string supplierID;
   std::string contactInfo;
   std::vector<std::string> suppliedProducts;   // product IDs

public:
   // Creates an empty supplier with no products attached.
   Supplier();

   // Creates a supplier record with no products attached yet.
   Supplier(const std::string& name, const std::string& id,
            const std::string& contact);

   std::string GetSupplierName() const;
   std::string GetSupplierID() const;
   std::string GetContactInfo() const;

   // Read-only view of the supplied product IDs.
   const std::vector<std::string>& GetSuppliedProducts() const;

   void SetSupplierName(const std::string& name);
   void SetSupplierID(const std::string& id);
   void SetContactInfo(const std::string& contact);

   // Updates name and contact in one call, leaving either unchanged when the
   // corresponding argument is empty. Used by the CLI edit screen.
   void UpdateInfo(const std::string& name, const std::string& contact);

   // Attaches a product ID. Returns false when the supplier already carries
   // it, which keeps the list free of duplicates.
   bool AddProduct(const std::string& productID);

   // Detaches a product ID. Returns false when the supplier does not carry it.
   bool RemoveProduct(const std::string& productID);

   // True when the supplier carries the given product ID.
   bool SuppliesProduct(const std::string& productID) const;

   // Number of distinct products attached to this supplier.
   int GetProductCount() const;

   // Single-line summary used by the CLI listings.
   std::string GetDetails() const;

   // Serializes to one CSV record, with the product list packed into a single
   // semicolon-separated field so the record stays one line.
   std::string ToCSV() const;

   // Builds a Supplier from one CSV record.
   static bool ParseCSV(const std::string& line, Supplier& out);

   static std::string CSVHeader();
};

#endif
