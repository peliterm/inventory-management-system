#include "Product.h"
#include "CSVUtil.h"

#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>

Product::Product() {
   quantity = 0;
   price = 0.0;
   reorderLevel = 0;
}

Product::Product(const std::string& productName, const std::string& id, int qty,
                 double unitPrice, const std::string& cat,
                 const std::string& supplier, int reorder) {
   name = productName;
   productID = id;
   quantity = (qty < 0) ? 0 : qty;
   price = (std::isfinite(unitPrice) && unitPrice > 0.0) ? unitPrice : 0.0;
   category = cat;
   supplierID = supplier;
   reorderLevel = (reorder < 0) ? 0 : reorder;
}

std::string Product::GetName() const { return name; }
std::string Product::GetProductID() const { return productID; }
int Product::GetQuantity() const { return quantity; }
double Product::GetPrice() const { return price; }
std::string Product::GetCategory() const { return category; }
std::string Product::GetSupplierID() const { return supplierID; }
int Product::GetReorderLevel() const { return reorderLevel; }

void Product::SetName(const std::string& productName) { name = productName; }
void Product::SetProductID(const std::string& id) { productID = id; }

void Product::SetQuantity(int qty) {
   quantity = (qty < 0) ? 0 : qty;
}

void Product::SetPrice(double unitPrice) {
   // A non-finite price would turn every total that touches it into nan, so
   // it is refused the same way a negative price is.
   if (!std::isfinite(unitPrice) || unitPrice < 0.0) {
      price = 0.0;
      return;
   }
   price = unitPrice;
}

void Product::SetCategory(const std::string& cat) { category = cat; }
void Product::SetSupplierID(const std::string& supplier) { supplierID = supplier; }

void Product::SetReorderLevel(int reorder) {
   reorderLevel = (reorder < 0) ? 0 : reorder;
}

bool Product::UpdateStock(int delta) {
   // The arithmetic is carried out in a wider type and the result is range
   // checked before anything is stored. Doing it in int would be undefined
   // behaviour twice over: quantity + delta can overflow, and negating delta
   // to compare against it overflows when delta is the most negative int.
   static_assert(sizeof(long long) > sizeof(int),
                 "UpdateStock needs a wider type than int to range check in");

   const long long result = static_cast<long long>(quantity) +
                            static_cast<long long>(delta);

   // Reject rather than clamp, so the caller can tell the difference between
   // "shipped what was asked" and "shipped what was left".
   if (result < 0) {
      return false;
   }
   if (result > static_cast<long long>(std::numeric_limits<int>::max())) {
      return false;
   }

   quantity = static_cast<int>(result);
   return true;
}

bool Product::IsLowStock() const {
   return quantity <= reorderLevel;
}

double Product::GetStockValue() const {
   return quantity * price;
}

std::string Product::GetDetails() const {
   std::ostringstream out;
   out << std::left << std::setw(10) << productID
       << std::setw(26) << name
       << std::setw(14) << category
       << std::right << std::setw(7) << quantity
       << std::setw(11) << std::fixed << std::setprecision(2) << price
       << std::setw(13) << GetStockValue();
   if (IsLowStock()) {
      out << "  LOW";
   }
   return out.str();
}

std::string Product::ToCSV() const {
   std::ostringstream out;
   out << CSVUtil::Escape(productID) << ','
       << CSVUtil::Escape(name) << ','
       << CSVUtil::Escape(category) << ','
       << quantity << ','
       << std::fixed << std::setprecision(4) << price << ','
       << CSVUtil::Escape(supplierID) << ','
       << reorderLevel;
   return out.str();
}

bool Product::ParseCSV(const std::string& line, Product& out) {
   std::vector<std::string> fields = CSVUtil::SplitLine(line);
   if (fields.size() != 7) {
      return false;
   }

   int qty = 0;
   double unitPrice = 0.0;
   int reorder = 0;

   if (!CSVUtil::ToInt(fields.at(3), qty)) {
      return false;
   }
   if (!CSVUtil::ToDouble(fields.at(4), unitPrice)) {
      return false;
   }
   if (!CSVUtil::ToInt(fields.at(6), reorder)) {
      return false;
   }

   out = Product(fields.at(1), fields.at(0), qty, unitPrice, fields.at(2),
                 fields.at(5), reorder);
   return true;
}

std::string Product::CSVHeader() {
   return "productID,name,category,quantity,price,supplierID,reorderLevel";
}
