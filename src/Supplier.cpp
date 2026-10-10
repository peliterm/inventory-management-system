#include "Supplier.h"
#include "CSVUtil.h"

#include <iomanip>
#include <sstream>

Supplier::Supplier() {
}

Supplier::Supplier(const std::string& name, const std::string& id,
                   const std::string& contact) {
   supplierName = name;
   supplierID = id;
   contactInfo = contact;
}

std::string Supplier::GetSupplierName() const { return supplierName; }
std::string Supplier::GetSupplierID() const { return supplierID; }
std::string Supplier::GetContactInfo() const { return contactInfo; }

const std::vector<std::string>& Supplier::GetSuppliedProducts() const {
   return suppliedProducts;
}

void Supplier::SetSupplierName(const std::string& name) { supplierName = name; }
void Supplier::SetSupplierID(const std::string& id) { supplierID = id; }
void Supplier::SetContactInfo(const std::string& contact) { contactInfo = contact; }

void Supplier::UpdateInfo(const std::string& name, const std::string& contact) {
   // An empty field from the edit screen means "leave this one alone".
   if (!name.empty()) {
      supplierName = name;
   }
   if (!contact.empty()) {
      contactInfo = contact;
   }
}

bool Supplier::AddProduct(const std::string& productID) {
   if (SuppliesProduct(productID)) {
      return false;
   }
   suppliedProducts.push_back(productID);
   return true;
}

bool Supplier::RemoveProduct(const std::string& productID) {
   for (size_t i = 0; i < suppliedProducts.size(); ++i) {
      if (suppliedProducts.at(i) == productID) {
         suppliedProducts.erase(suppliedProducts.begin() + static_cast<long>(i));
         return true;
      }
   }
   return false;
}

bool Supplier::SuppliesProduct(const std::string& productID) const {
   for (size_t i = 0; i < suppliedProducts.size(); ++i) {
      if (suppliedProducts.at(i) == productID) {
         return true;
      }
   }
   return false;
}

int Supplier::GetProductCount() const {
   return static_cast<int>(suppliedProducts.size());
}

std::string Supplier::GetDetails() const {
   std::ostringstream out;
   out << std::left << std::setw(10) << supplierID
       << std::setw(28) << supplierName
       << std::setw(30) << contactInfo
       << std::right << std::setw(9) << GetProductCount();
   return out.str();
}

std::string Supplier::ToCSV() const {
   std::string packed;
   for (size_t i = 0; i < suppliedProducts.size(); ++i) {
      if (i > 0) {
         packed += ";";
      }
      // Escaped for the same reason as an order's line list: a product ID
      // holding a semicolon must not split the packed field.
      packed += CSVUtil::EscapeDelims(suppliedProducts.at(i));
   }

   std::ostringstream out;
   out << CSVUtil::Escape(supplierID) << ','
       << CSVUtil::Escape(supplierName) << ','
       << CSVUtil::Escape(contactInfo) << ','
       << CSVUtil::Escape(packed);
   return out.str();
}

bool Supplier::ParseCSV(const std::string& line, Supplier& out) {
   std::vector<std::string> fields = CSVUtil::SplitLine(line);
   if (fields.size() != 4) {
      return false;
   }

   Supplier built(fields.at(1), fields.at(0), fields.at(2));

   // Unpack the semicolon-separated product list.
   std::vector<std::string> packed = CSVUtil::SplitEscaped(fields.at(3), ';');
   for (size_t i = 0; i < packed.size(); ++i) {
      std::string id = CSVUtil::Trim(CSVUtil::UnescapeDelims(packed.at(i)));
      if (!id.empty()) {
         built.AddProduct(id);
      }
   }

   out = built;
   return true;
}

std::string Supplier::CSVHeader() {
   return "supplierID,supplierName,contactInfo,suppliedProducts";
}
