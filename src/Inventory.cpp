#include "Inventory.h"
#include "ChartUtil.h"
#include "CSVUtil.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

ProductFilter::ProductFilter() {
   minPrice = -1.0;
   maxPrice = -1.0;
   lowStockOnly = false;
}

Inventory::Inventory() {
}

// ---------------------------------------------------------------------------
// private lookups
// ---------------------------------------------------------------------------

int Inventory::FindProductIndex(const std::string& productID) const {
   for (size_t i = 0; i < products.size(); ++i) {
      if (products.at(i).GetProductID() == productID) {
         return static_cast<int>(i);
      }
   }
   return -1;
}

int Inventory::FindSupplierIndex(const std::string& supplierID) const {
   for (size_t i = 0; i < suppliers.size(); ++i) {
      if (suppliers.at(i).GetSupplierID() == supplierID) {
         return static_cast<int>(i);
      }
   }
   return -1;
}

int Inventory::FindOrderIndex(const std::string& orderID) const {
   for (size_t i = 0; i < orders.size(); ++i) {
      if (orders.at(i).GetOrderID() == orderID) {
         return static_cast<int>(i);
      }
   }
   return -1;
}

int Inventory::CountAdmins() const {
   int count = 0;
   for (size_t i = 0; i < users.size(); ++i) {
      if (users.at(i).GetRole() == Role::ADMIN) {
         ++count;
      }
   }
   return count;
}

int Inventory::FindUserIndex(const std::string& username) const {
   for (size_t i = 0; i < users.size(); ++i) {
      if (users.at(i).GetUsername() == username) {
         return static_cast<int>(i);
      }
   }
   return -1;
}

// ---------------------------------------------------------------------------
// product management
// ---------------------------------------------------------------------------

bool Inventory::AddProduct(const Product& product, std::string& message) {
   if (product.GetProductID().empty()) {
      message = "Product ID cannot be empty.";
      return false;
   }
   if (FindProductIndex(product.GetProductID()) >= 0) {
      message = "A product with ID " + product.GetProductID() + " already exists.";
      return false;
   }

   products.push_back(product);

   // Keep the supplier's product list in step with the catalogue. A product
   // may legitimately have no supplier, but naming one that does not exist is
   // reported rather than passed over, since it leaves a dangling reference.
   std::string note;
   const std::string& supplierID = product.GetSupplierID();
   if (!supplierID.empty()) {
      int supplierIndex = FindSupplierIndex(supplierID);
      if (supplierIndex >= 0) {
         suppliers.at(static_cast<size_t>(supplierIndex)).AddProduct(product.GetProductID());
      }
      else {
         note = "  WARNING: supplier " + supplierID + " is not on file.";
      }
   }

   message = "Added product " + product.GetProductID() + "." + note;
   return true;
}

bool Inventory::RemoveProduct(const std::string& productID, std::string& message) {
   int index = FindProductIndex(productID);
   if (index < 0) {
      message = "No product with ID " + productID + ".";
      return false;
   }

   // Refuse while the product appears on any order at all. An open order would
   // be left unfillable, and a closed one can still be returned later, which
   // needs the product record to put the stock back into.
   for (size_t i = 0; i < orders.size(); ++i) {
      const std::vector<OrderLine>& lines = orders.at(i).GetOrderedProducts();
      for (size_t j = 0; j < lines.size(); ++j) {
         if (lines.at(j).productID == productID) {
            message = "Product " + productID + " appears on order " +
                      orders.at(i).GetOrderID() + " (" +
                      Order::StatusToString(orders.at(i).GetStatus()) +
                      "); remove the order history first.";
            return false;
         }
      }
   }

   // Detach from every supplier before erasing the catalogue record.
   for (size_t i = 0; i < suppliers.size(); ++i) {
      suppliers.at(i).RemoveProduct(productID);
   }

   products.erase(products.begin() + index);
   message = "Removed product " + productID + ".";
   return true;
}

bool Inventory::UpdateProduct(const std::string& productID, const Product& updated,
                              std::string& message) {
   int index = FindProductIndex(productID);
   if (index < 0) {
      message = "No product with ID " + productID + ".";
      return false;
   }

   std::string oldSupplier = products.at(static_cast<size_t>(index)).GetSupplierID();
   std::string newSupplier = updated.GetSupplierID();

   products.at(static_cast<size_t>(index)) = updated;
   products.at(static_cast<size_t>(index)).SetProductID(productID);   // ID is immutable

   // Move the product between supplier lists when its supplier changed.
   if (oldSupplier != newSupplier) {
      int oldIndex = FindSupplierIndex(oldSupplier);
      if (oldIndex >= 0) {
         suppliers.at(static_cast<size_t>(oldIndex)).RemoveProduct(productID);
      }
      int newIndex = FindSupplierIndex(newSupplier);
      if (newIndex >= 0) {
         suppliers.at(static_cast<size_t>(newIndex)).AddProduct(productID);
      }
   }

   message = "Updated product " + productID + ".";
   return true;
}

bool Inventory::AdjustStock(const std::string& productID, int delta,
                            std::string& message) {
   int index = FindProductIndex(productID);
   if (index < 0) {
      message = "No product with ID " + productID + ".";
      return false;
   }

   if (!products.at(static_cast<size_t>(index)).UpdateStock(delta)) {
      std::ostringstream out;
      if (delta < 0) {
         // Widen before negating: -INT_MIN is not representable as an int.
         out << "Cannot reduce " << productID << " by "
             << -static_cast<long long>(delta)
             << "; only " << products.at(static_cast<size_t>(index)).GetQuantity()
             << " on hand.";
      }
      else {
         out << "Cannot increase " << productID << " by " << delta
             << "; the resulting stock would exceed the maximum of "
             << std::numeric_limits<int>::max() << ".";
      }
      message = out.str();
      return false;
   }

   std::ostringstream out;
   out << productID << " stock is now "
       << products.at(static_cast<size_t>(index)).GetQuantity() << ".";
   if (products.at(static_cast<size_t>(index)).IsLowStock()) {
      out << "  ALERT: at or below reorder level of "
          << products.at(static_cast<size_t>(index)).GetReorderLevel() << ".";
   }
   message = out.str();
   return true;
}

bool Inventory::GetProduct(const std::string& productID, Product& out) const {
   int index = FindProductIndex(productID);
   if (index < 0) {
      return false;
   }
   out = products.at(static_cast<size_t>(index));
   return true;
}

const std::vector<Product>& Inventory::GetProducts() const {
   return products;
}

std::vector<Product> Inventory::SearchProducts(const ProductFilter& filter) const {
   std::vector<Product> matches;

   for (size_t i = 0; i < products.size(); ++i) {
      const Product& candidate = products.at(i);

      if (!filter.nameContains.empty() &&
          !CSVUtil::ContainsIgnoreCase(candidate.GetName(), filter.nameContains) &&
          !CSVUtil::ContainsIgnoreCase(candidate.GetProductID(), filter.nameContains)) {
         continue;
      }
      if (!filter.category.empty() &&
          CSVUtil::ToLower(candidate.GetCategory()) != CSVUtil::ToLower(filter.category)) {
         continue;
      }
      if (!filter.supplierID.empty() && candidate.GetSupplierID() != filter.supplierID) {
         continue;
      }
      if (filter.minPrice >= 0.0 && candidate.GetPrice() < filter.minPrice) {
         continue;
      }
      if (filter.maxPrice >= 0.0 && candidate.GetPrice() > filter.maxPrice) {
         continue;
      }
      if (filter.lowStockOnly && !candidate.IsLowStock()) {
         continue;
      }

      matches.push_back(candidate);
   }

   return matches;
}

std::vector<Product> Inventory::GetLowStockProducts() const {
   std::vector<Product> low;
   for (size_t i = 0; i < products.size(); ++i) {
      if (products.at(i).IsLowStock()) {
         low.push_back(products.at(i));
      }
   }
   return low;
}

// ---------------------------------------------------------------------------
// supplier management
// ---------------------------------------------------------------------------

bool Inventory::AddSupplier(const Supplier& supplier, std::string& message) {
   if (supplier.GetSupplierID().empty()) {
      message = "Supplier ID cannot be empty.";
      return false;
   }
   if (FindSupplierIndex(supplier.GetSupplierID()) >= 0) {
      message = "A supplier with ID " + supplier.GetSupplierID() + " already exists.";
      return false;
   }

   suppliers.push_back(supplier);
   message = "Added supplier " + supplier.GetSupplierID() + ".";
   return true;
}

bool Inventory::RemoveSupplier(const std::string& supplierID, std::string& message) {
   int index = FindSupplierIndex(supplierID);
   if (index < 0) {
      message = "No supplier with ID " + supplierID + ".";
      return false;
   }

   for (size_t i = 0; i < products.size(); ++i) {
      if (products.at(i).GetSupplierID() == supplierID) {
         message = "Supplier " + supplierID + " still supplies product " +
                   products.at(i).GetProductID() + ".";
         return false;
      }
   }

   // A purchase order names its supplier, and the efficiency report reads that
   // name back, so removing the vendor would quietly drop those orders from
   // the report rather than showing a gap.
   for (size_t i = 0; i < orders.size(); ++i) {
      if (orders.at(i).GetOrderType() == OrderType::PURCHASE &&
          orders.at(i).GetCounterpartyID() == supplierID) {
         message = "Supplier " + supplierID + " is named on purchase order " +
                   orders.at(i).GetOrderID() + ".";
         return false;
      }
   }

   suppliers.erase(suppliers.begin() + index);
   message = "Removed supplier " + supplierID + ".";
   return true;
}

bool Inventory::UpdateSupplier(const std::string& supplierID, const std::string& name,
                               const std::string& contact, std::string& message) {
   int index = FindSupplierIndex(supplierID);
   if (index < 0) {
      message = "No supplier with ID " + supplierID + ".";
      return false;
   }

   suppliers.at(static_cast<size_t>(index)).UpdateInfo(name, contact);
   message = "Updated supplier " + supplierID + ".";
   return true;
}

bool Inventory::GetSupplier(const std::string& supplierID, Supplier& out) const {
   int index = FindSupplierIndex(supplierID);
   if (index < 0) {
      return false;
   }
   out = suppliers.at(static_cast<size_t>(index));
   return true;
}

const std::vector<Supplier>& Inventory::GetSuppliers() const {
   return suppliers;
}

bool Inventory::LinkProductToSupplier(const std::string& productID,
                                      const std::string& supplierID,
                                      std::string& message) {
   int productIndex = FindProductIndex(productID);
   int supplierIndex = FindSupplierIndex(supplierID);

   if (productIndex < 0) {
      message = "No product with ID " + productID + ".";
      return false;
   }
   if (supplierIndex < 0) {
      message = "No supplier with ID " + supplierID + ".";
      return false;
   }

   // Detach from the previous supplier so a product never has two owners.
   std::string previous = products.at(static_cast<size_t>(productIndex)).GetSupplierID();
   if (!previous.empty() && previous != supplierID) {
      int previousIndex = FindSupplierIndex(previous);
      if (previousIndex >= 0) {
         suppliers.at(static_cast<size_t>(previousIndex)).RemoveProduct(productID);
      }
   }

   products.at(static_cast<size_t>(productIndex)).SetSupplierID(supplierID);
   suppliers.at(static_cast<size_t>(supplierIndex)).AddProduct(productID);

   message = "Linked " + productID + " to " + supplierID + ".";
   return true;
}

bool Inventory::UnlinkProductFromSupplier(const std::string& productID,
                                          const std::string& supplierID,
                                          std::string& message) {
   int productIndex = FindProductIndex(productID);
   int supplierIndex = FindSupplierIndex(supplierID);

   if (productIndex < 0 || supplierIndex < 0) {
      message = "Product or supplier not found.";
      return false;
   }

   suppliers.at(static_cast<size_t>(supplierIndex)).RemoveProduct(productID);
   if (products.at(static_cast<size_t>(productIndex)).GetSupplierID() == supplierID) {
      products.at(static_cast<size_t>(productIndex)).SetSupplierID("");
   }

   message = "Unlinked " + productID + " from " + supplierID + ".";
   return true;
}

// ---------------------------------------------------------------------------
// order processing
// ---------------------------------------------------------------------------

bool Inventory::PlaceOrder(const Order& order, std::string& message) {
   if (order.GetOrderID().empty()) {
      message = "Order ID cannot be empty.";
      return false;
   }
   if (FindOrderIndex(order.GetOrderID()) >= 0) {
      message = "An order with ID " + order.GetOrderID() + " already exists.";
      return false;
   }

   const std::vector<OrderLine>& lines = order.GetOrderedProducts();
   if (lines.empty()) {
      message = "An order must contain at least one product.";
      return false;
   }

   // Validate every line before changing any stock, so a bad line at the end
   // cannot leave the earlier lines already deducted.
   for (size_t i = 0; i < lines.size(); ++i) {
      int index = FindProductIndex(lines.at(i).productID);
      if (index < 0) {
         message = "Order names unknown product " + lines.at(i).productID + ".";
         return false;
      }
      if (order.GetOrderType() == OrderType::SALE &&
          products.at(static_cast<size_t>(index)).GetQuantity() < lines.at(i).quantity) {
         std::ostringstream out;
         out << "Only " << products.at(static_cast<size_t>(index)).GetQuantity()
             << " of " << lines.at(i).productID << " on hand; order asks for "
             << lines.at(i).quantity << ".";
         message = out.str();
         return false;
      }
   }

   // A sale claims its stock immediately so two orders cannot both take the
   // last unit. A purchase adds nothing until the goods actually arrive.
   std::ostringstream alerts;
   if (order.GetOrderType() == OrderType::SALE) {
      for (size_t i = 0; i < lines.size(); ++i) {
         int index = FindProductIndex(lines.at(i).productID);
         products.at(static_cast<size_t>(index)).UpdateStock(-lines.at(i).quantity);
         if (products.at(static_cast<size_t>(index)).IsLowStock()) {
            alerts << "  ALERT: " << lines.at(i).productID << " now at "
                   << products.at(static_cast<size_t>(index)).GetQuantity()
                   << ", at or below reorder level.";
         }
      }
   }

   orders.push_back(order);

   message = "Placed " + Order::TypeToString(order.GetOrderType()) + " order " +
             order.GetOrderID() + ", estimated delivery " +
             order.GetEstimatedDelivery().ToString() + "." + alerts.str();
   return true;
}

// Works out which way stock must move for a transition, as a signed multiplier
// applied to each line quantity. Zero means the transition moves no stock.
int Inventory::StockDirectionFor(OrderType type, OrderStatus from, OrderStatus next) {
   // A delivered purchase is the moment the goods land, so stock goes up.
   if (type == OrderType::PURCHASE && next == OrderStatus::DELIVERED) {
      return +1;
   }

   // Returning a delivered purchase sends those goods back to the vendor, so
   // the stock that arrived with it has to leave again.
   if (type == OrderType::PURCHASE && next == OrderStatus::RETURNED &&
       from == OrderStatus::DELIVERED) {
      return -1;
   }

   // A sale that is cancelled or returned releases the stock it was holding,
   // which was taken when the order was placed.
   if (type == OrderType::SALE &&
       (next == OrderStatus::CANCELLED || next == OrderStatus::RETURNED)) {
      return +1;
   }

   // Everything else, including a cancelled purchase that never added stock,
   // leaves the counts alone.
   return 0;
}

bool Inventory::ApplyStockForStatus(Order& order, OrderStatus next,
                                    std::string& message) {
   const std::vector<OrderLine>& lines = order.GetOrderedProducts();
   int direction = StockDirectionFor(order.GetOrderType(), order.GetStatus(), next);

   if (direction == 0) {
      message = "";
      return true;
   }

   // Check every line before moving anything. A transition that cannot be
   // carried out in full is refused outright, so the order status and the
   // stock counts can never end up disagreeing with each other.
   for (size_t i = 0; i < lines.size(); ++i) {
      int index = FindProductIndex(lines.at(i).productID);
      if (index < 0) {
         message = "Product " + lines.at(i).productID +
                   " on this order is no longer in the catalogue.";
         return false;
      }

      const Product& product = products.at(static_cast<size_t>(index));
      const long long delta = static_cast<long long>(direction) *
                              static_cast<long long>(lines.at(i).quantity);
      const long long result = static_cast<long long>(product.GetQuantity()) + delta;

      // Both directions are checked. A sale that asks for more than is on hand
      // and a delivery that would push a product past the largest quantity the
      // system can store are equally reasons to refuse the whole transition.
      if (result < 0) {
         std::ostringstream out;
         out << "Only " << product.GetQuantity() << " of " << lines.at(i).productID
             << " on hand; this change needs " << -delta << ".";
         message = out.str();
         return false;
      }
      if (result > static_cast<long long>(std::numeric_limits<int>::max())) {
         std::ostringstream out;
         out << "Receiving " << delta << " of " << lines.at(i).productID
             << " would take its stock past the maximum this system can hold.";
         message = out.str();
         return false;
      }
   }

   // Applied only after every line has passed. The deltas already made are
   // tracked so that an unexpected refusal here can be undone, leaving the
   // order and the stock counts exactly as they were.
   std::ostringstream alerts;
   std::vector<size_t> applied;
   for (size_t i = 0; i < lines.size(); ++i) {
      int index = FindProductIndex(lines.at(i).productID);
      const int delta = direction * lines.at(i).quantity;

      if (!products.at(static_cast<size_t>(index)).UpdateStock(delta)) {
         for (size_t undo = 0; undo < applied.size(); ++undo) {
            const int undoIndex = FindProductIndex(lines.at(applied.at(undo)).productID);
            products.at(static_cast<size_t>(undoIndex))
                    .UpdateStock(-direction * lines.at(applied.at(undo)).quantity);
         }
         message = "Stock for " + lines.at(i).productID +
                   " could not be changed, so the order was left unchanged.";
         return false;
      }

      applied.push_back(i);
      if (products.at(static_cast<size_t>(index)).IsLowStock()) {
         alerts << "  ALERT: " << lines.at(i).productID << " is at or below reorder level.";
      }
   }

   // The wording names what actually happened, since "stock went up" means
   // something different for goods arriving than for a sale being undone.
   if (order.GetOrderType() == OrderType::PURCHASE && next == OrderStatus::DELIVERED) {
      message = "  Stock received into inventory.";
   }
   else if (order.GetOrderType() == OrderType::PURCHASE && next == OrderStatus::RETURNED) {
      message = "  Stock sent back to the supplier.";
   }
   else {
      message = "  Stock returned to inventory.";
   }
   message += alerts.str();
   return true;
}

bool Inventory::UpdateOrderStatus(const std::string& orderID, OrderStatus next,
                                  const Date& when, std::string& message) {
   int index = FindOrderIndex(orderID);
   if (index < 0) {
      message = "No order with ID " + orderID + ".";
      return false;
   }

   Order& order = orders.at(static_cast<size_t>(index));
   OrderStatus previous = order.GetStatus();

   // Check the lifecycle rule first without committing to it.
   if (!Order::IsLegalTransition(previous, next)) {
      message = "Cannot move order " + orderID + " from " +
                Order::StatusToString(previous) + " to " +
                Order::StatusToString(next) + ".";
      return false;
   }

   // Move the stock before the status, so a stock movement that cannot be
   // carried out leaves the order exactly as it was rather than recording a
   // status the inventory does not reflect.
   std::string stockNote;
   if (!ApplyStockForStatus(order, next, stockNote)) {
      message = "Order " + orderID + " was not changed. " + stockNote;
      return false;
   }

   order.UpdateStatus(next, when);

   message = "Order " + orderID + " is now " + Order::StatusToString(next) +
             "." + stockNote;
   return true;
}

bool Inventory::SetDeliveryEstimate(const std::string& orderID, const Date& estimate,
                                    std::string& message) {
   int index = FindOrderIndex(orderID);
   if (index < 0) {
      message = "No order with ID " + orderID + ".";
      return false;
   }

   OrderStatus status = orders.at(static_cast<size_t>(index)).GetStatus();
   if (status == OrderStatus::DELIVERED || status == OrderStatus::RETURNED ||
       status == OrderStatus::CANCELLED) {
      message = "Order " + orderID + " is closed; its estimate cannot change.";
      return false;
   }

   orders.at(static_cast<size_t>(index)).SetEstimatedDelivery(estimate);
   message = "Order " + orderID + " is now estimated for " + estimate.ToString() + ".";
   return true;
}

bool Inventory::GetOrder(const std::string& orderID, Order& out) const {
   int index = FindOrderIndex(orderID);
   if (index < 0) {
      return false;
   }
   out = orders.at(static_cast<size_t>(index));
   return true;
}

const std::vector<Order>& Inventory::GetOrders() const {
   return orders;
}

std::string Inventory::NextOrderID() const {
   int highest = 0;
   for (size_t i = 0; i < orders.size(); ++i) {
      std::string id = orders.at(i).GetOrderID();
      // Pull the numeric tail off IDs shaped like ORD-0007.
      size_t dash = id.find('-');
      if (dash != std::string::npos) {
         int value = 0;
         if (CSVUtil::ToInt(id.substr(dash + 1), value) && value > highest) {
            highest = value;
         }
      }
   }

   std::ostringstream out;
   out << "ORD-" << std::setw(4) << std::setfill('0') << (highest + 1);
   return out.str();
}

// ---------------------------------------------------------------------------
// user accounts
// ---------------------------------------------------------------------------

bool Inventory::AddUser(const User& user, std::string& message) {
   if (user.GetUsername().empty()) {
      message = "Username cannot be empty.";
      return false;
   }
   if (FindUserIndex(user.GetUsername()) >= 0) {
      message = "User " + user.GetUsername() + " already exists.";
      return false;
   }

   users.push_back(user);
   message = "Created user " + user.GetUsername() + ".";
   return true;
}

bool Inventory::RemoveUser(const std::string& username, std::string& message) {
   int index = FindUserIndex(username);
   if (index < 0) {
      message = "No user named " + username + ".";
      return false;
   }

   // Refuse to delete the last administrator, which would lock everyone out of
   // account management permanently.
   if (users.at(static_cast<size_t>(index)).GetRole() == Role::ADMIN &&
       CountAdmins() <= 1) {
      message = "Cannot remove the only administrator account.";
      return false;
   }

   users.erase(users.begin() + index);
   message = "Removed user " + username + ".";
   return true;
}

bool Inventory::Authenticate(const std::string& username, const std::string& password,
                             User& out) const {
   int index = FindUserIndex(username);
   if (index < 0) {
      return false;
   }
   if (!users.at(static_cast<size_t>(index)).CheckPassword(password)) {
      return false;
   }

   out = users.at(static_cast<size_t>(index));
   return true;
}

const std::vector<User>& Inventory::GetUsers() const {
   return users;
}

void Inventory::EnsureDefaultAdmin() {
   if (!users.empty()) {
      return;
   }
   // A fresh installation needs one account to log in with. The password is
   // printed at the login screen so the grader can get in.
   users.push_back(User("admin", "Default Administrator", "admin123", Role::ADMIN));
}

// ---------------------------------------------------------------------------
// bulk import and export
// ---------------------------------------------------------------------------
// Every importer follows the same shape: open the file, skip a header row if
// one is present, parse each remaining line, and count the lines that would
// not parse rather than abandoning the file.
// ---------------------------------------------------------------------------

// True when a line looks like a column-header row rather than a record. The
// first field is compared after lowercasing and dropping anything that is not
// a letter, so "productID", "Product ID", and "product_id" are all recognised.
// Without this a renamed header is read as a record, and a parser with no
// numeric field to fail on accepts it.
static bool LooksLikeHeader(const std::string& line, const std::string& expectedKey) {
   std::vector<std::string> fields = CSVUtil::SplitLine(line);
   if (fields.empty()) {
      return false;
   }

   std::string key;
   const std::string& first = fields.at(0);
   for (size_t i = 0; i < first.size(); ++i) {
      unsigned char c = static_cast<unsigned char>(first.at(i));
      if (std::isalpha(c)) {
         key += static_cast<char>(std::tolower(c));
      }
   }

   return key == expectedKey;
}

int Inventory::ImportProductsCSV(const std::string& path, int& skipped,
                                 std::string& message) {
   std::ifstream file(path);
   skipped = 0;
   if (!file.is_open()) {
      message = "Could not open " + path + " for reading.";
      return -1;
   }

   int loaded = 0;
   std::string line;
   bool firstLine = true;

   while (std::getline(file, line)) {
      // A spreadsheet writes a byte order mark ahead of the first line, so
      // it is stripped before anything else looks at the text.
      if (firstLine) {
         line = CSVUtil::StripBOM(line);
      }
      line = CSVUtil::Trim(line);
      if (line.empty()) {
         continue;
      }
      if (firstLine && LooksLikeHeader(line, "productid")) {
         firstLine = false;
         continue;   // header row
      }
      firstLine = false;

      Product parsed;
      if (!Product::ParseCSV(line, parsed)) {
         ++skipped;
         continue;
      }

      // Every record goes through the same validated path the menus use, and
      // a record that path refuses is counted as skipped rather than reported
      // as loaded.
      std::string note;
      bool stored = false;
      if (FindProductIndex(parsed.GetProductID()) >= 0) {
         stored = UpdateProduct(parsed.GetProductID(), parsed, note);
      }
      else {
         stored = AddProduct(parsed, note);
      }

      if (stored) {
         ++loaded;
      }
      else {
         ++skipped;
      }
   }

   std::ostringstream out;
   out << "Imported " << loaded << " product record(s) from " << path;
   if (skipped > 0) {
      out << "; skipped " << skipped << " malformed line(s)";
   }
   out << ".";
   message = out.str();
   return loaded;
}

int Inventory::ExportProductsCSV(const std::string& path, std::string& message) const {
   std::ofstream file(path);
   if (!file.is_open()) {
      message = "Could not open " + path + " for writing.";
      return -1;
   }

   file << Product::CSVHeader() << "\n";
   for (size_t i = 0; i < products.size(); ++i) {
      file << products.at(i).ToCSV() << "\n";
   }

   message = "Exported " + std::to_string(products.size()) + " product record(s) to " + path + ".";
   return static_cast<int>(products.size());
}

int Inventory::ImportSuppliersCSV(const std::string& path, int& skipped,
                                  std::string& message) {
   std::ifstream file(path);
   skipped = 0;
   if (!file.is_open()) {
      message = "Could not open " + path + " for reading.";
      return -1;
   }

   int loaded = 0;
   std::string line;
   bool firstLine = true;

   while (std::getline(file, line)) {
      // A spreadsheet writes a byte order mark ahead of the first line, so
      // it is stripped before anything else looks at the text.
      if (firstLine) {
         line = CSVUtil::StripBOM(line);
      }
      line = CSVUtil::Trim(line);
      if (line.empty()) {
         continue;
      }
      if (firstLine && LooksLikeHeader(line, "supplierid")) {
         firstLine = false;
         continue;
      }
      firstLine = false;

      Supplier parsed;
      if (!Supplier::ParseCSV(line, parsed)) {
         ++skipped;
         continue;
      }

      // A blank supplier ID would be matched by every product that names no
      // supplier, so the same check the menu applies is applied here.
      if (parsed.GetSupplierID().empty()) {
         ++skipped;
         continue;
      }

      int existing = FindSupplierIndex(parsed.GetSupplierID());
      if (existing >= 0) {
         suppliers.at(static_cast<size_t>(existing)) = parsed;
      }
      else {
         suppliers.push_back(parsed);
      }
      ++loaded;
   }

   std::ostringstream out;
   out << "Imported " << loaded << " supplier record(s) from " << path;
   if (skipped > 0) {
      out << "; skipped " << skipped << " malformed line(s)";
   }
   out << ".";
   message = out.str();
   return loaded;
}

int Inventory::ExportSuppliersCSV(const std::string& path, std::string& message) const {
   std::ofstream file(path);
   if (!file.is_open()) {
      message = "Could not open " + path + " for writing.";
      return -1;
   }

   file << Supplier::CSVHeader() << "\n";
   for (size_t i = 0; i < suppliers.size(); ++i) {
      file << suppliers.at(i).ToCSV() << "\n";
   }

   message = "Exported " + std::to_string(suppliers.size()) + " supplier record(s) to " + path + ".";
   return static_cast<int>(suppliers.size());
}

int Inventory::ImportOrdersCSV(const std::string& path, int& skipped,
                               std::string& message) {
   std::ifstream file(path);
   skipped = 0;
   if (!file.is_open()) {
      message = "Could not open " + path + " for reading.";
      return -1;
   }

   int loaded = 0;
   std::string line;
   bool firstLine = true;

   while (std::getline(file, line)) {
      // A spreadsheet writes a byte order mark ahead of the first line, so
      // it is stripped before anything else looks at the text.
      if (firstLine) {
         line = CSVUtil::StripBOM(line);
      }
      line = CSVUtil::Trim(line);
      if (line.empty()) {
         continue;
      }
      if (firstLine && LooksLikeHeader(line, "orderid")) {
         firstLine = false;
         continue;
      }
      firstLine = false;

      Order parsed;
      if (!Order::ParseCSV(line, parsed)) {
         ++skipped;
         continue;
      }

      if (parsed.GetOrderID().empty()) {
         ++skipped;
         continue;
      }

      // An order already on file is left alone. Overwriting one would replace
      // a record whose stock movements have already been applied, and nothing
      // would reverse them, so the counts and the order would disagree with
      // no way back. Re-importing the same file is therefore safe.
      if (FindOrderIndex(parsed.GetOrderID()) >= 0) {
         ++skipped;
         continue;
      }

      // Imported orders are historical records, so they are stored as-is
      // rather than being replayed through PlaceOrder, which would deduct
      // stock a second time for sales that already shipped.
      orders.push_back(parsed);
      ++loaded;
   }

   std::ostringstream out;
   out << "Imported " << loaded << " order record(s) from " << path;
   if (skipped > 0) {
      out << "; skipped " << skipped << " malformed line(s)";
   }
   out << ".";
   message = out.str();
   return loaded;
}

int Inventory::ExportOrdersCSV(const std::string& path, std::string& message) const {
   std::ofstream file(path);
   if (!file.is_open()) {
      message = "Could not open " + path + " for writing.";
      return -1;
   }

   file << Order::CSVHeader() << "\n";
   for (size_t i = 0; i < orders.size(); ++i) {
      file << orders.at(i).ToCSV() << "\n";
   }

   message = "Exported " + std::to_string(orders.size()) + " order record(s) to " + path + ".";
   return static_cast<int>(orders.size());
}

int Inventory::ImportUsersCSV(const std::string& path, int& skipped,
                              std::string& message) {
   std::ifstream file(path);
   skipped = 0;
   if (!file.is_open()) {
      message = "Could not open " + path + " for reading.";
      return -1;
   }

   int loaded = 0;
   std::string line;
   bool firstLine = true;

   while (std::getline(file, line)) {
      // A spreadsheet writes a byte order mark ahead of the first line, so
      // it is stripped before anything else looks at the text.
      if (firstLine) {
         line = CSVUtil::StripBOM(line);
      }
      line = CSVUtil::Trim(line);
      if (line.empty()) {
         continue;
      }
      if (firstLine && LooksLikeHeader(line, "username")) {
         firstLine = false;
         continue;
      }
      firstLine = false;

      User parsed;
      if (!User::ParseCSV(line, parsed)) {
         ++skipped;
         continue;
      }

      if (parsed.GetUsername().empty()) {
         ++skipped;
         continue;
      }

      int existing = FindUserIndex(parsed.GetUsername());
      if (existing >= 0) {
         // Replacing the only administrator with a lesser role would leave a
         // system nobody can administer, which is the same outcome
         // RemoveUser() refuses, so it is refused here too.
         if (users.at(static_cast<size_t>(existing)).GetRole() == Role::ADMIN &&
             parsed.GetRole() != Role::ADMIN && CountAdmins() <= 1) {
            ++skipped;
            continue;
         }
         users.at(static_cast<size_t>(existing)) = parsed;
      }
      else {
         users.push_back(parsed);
      }
      ++loaded;
   }

   std::ostringstream out;
   out << "Imported " << loaded << " user record(s) from " << path;
   if (skipped > 0) {
      out << "; skipped " << skipped << " malformed line(s)";
   }
   out << ".";
   message = out.str();
   return loaded;
}

int Inventory::ExportUsersCSV(const std::string& path, std::string& message) const {
   std::ofstream file(path);
   if (!file.is_open()) {
      message = "Could not open " + path + " for writing.";
      return -1;
   }

   file << User::CSVHeader() << "\n";
   for (size_t i = 0; i < users.size(); ++i) {
      file << users.at(i).ToCSV() << "\n";
   }

   message = "Exported " + std::to_string(users.size()) + " user record(s) to " + path + ".";
   return static_cast<int>(users.size());
}

// ---------------------------------------------------------------------------
// reporting and analytics
// ---------------------------------------------------------------------------

std::string Inventory::ReportStockLevels() const {
   std::ostringstream out;
   out << "STOCK LEVEL REPORT\n";
   out << "==================================================================================\n";
   out << std::left << std::setw(10) << "ID" << std::setw(26) << "NAME"
       << std::setw(14) << "CATEGORY" << std::right << std::setw(7) << "QTY"
       << std::setw(11) << "PRICE" << std::setw(13) << "VALUE" << "\n";
   out << "----------------------------------------------------------------------------------\n";

   double totalValue = 0.0;
   long long totalUnits = 0;
   for (size_t i = 0; i < products.size(); ++i) {
      out << products.at(i).GetDetails() << "\n";
      totalValue += products.at(i).GetStockValue();
      totalUnits += static_cast<long long>(products.at(i).GetQuantity());
   }

   if (products.empty()) {
      out << "   (no products on file)\n";
   }

   out << "----------------------------------------------------------------------------------\n";
   out << "   Distinct products: " << products.size()
       << "   Units on hand: " << totalUnits
       << "   Inventory value: " << std::fixed << std::setprecision(2) << totalValue << "\n\n";

   // Chart the value held in each category, which is where the money sits.
   std::vector<std::string> categoryNames;
   std::vector<double> categoryValues;
   for (size_t i = 0; i < products.size(); ++i) {
      std::string category = products.at(i).GetCategory();
      if (category.empty()) {
         category = "(none)";
      }
      bool found = false;
      for (size_t j = 0; j < categoryNames.size(); ++j) {
         if (categoryNames.at(j) == category) {
            categoryValues.at(j) += products.at(i).GetStockValue();
            found = true;
            break;
         }
      }
      if (!found) {
         categoryNames.push_back(category);
         categoryValues.push_back(products.at(i).GetStockValue());
      }
   }

   std::vector<ChartUtil::Bar> bars;
   for (size_t i = 0; i < categoryNames.size(); ++i) {
      bars.push_back(ChartUtil::Bar(categoryNames.at(i), categoryValues.at(i)));
   }

   out << "   Inventory value by category\n";
   out << ChartUtil::HorizontalBarChart(bars, 40, 2);
   return out.str();
}

std::string Inventory::ReportLowStock() const {
   std::vector<Product> low = GetLowStockProducts();

   // Most urgent first: the furthest below its reorder level.
   std::sort(low.begin(), low.end(), [](const Product& a, const Product& b) {
      return (a.GetQuantity() - a.GetReorderLevel()) <
             (b.GetQuantity() - b.GetReorderLevel());
   });

   std::ostringstream out;
   out << "LOW STOCK ALERTS\n";
   out << "==================================================================================\n";

   if (low.empty()) {
      out << "   No products are at or below their reorder level.\n";
      return out.str();
   }

   out << std::left << std::setw(10) << "ID" << std::setw(26) << "NAME"
       << std::right << std::setw(8) << "QTY" << std::setw(10) << "REORDER"
       << std::setw(10) << "SHORT" << "   SUPPLIER\n";
   out << "----------------------------------------------------------------------------------\n";

   for (size_t i = 0; i < low.size(); ++i) {
      int shortfall = low.at(i).GetReorderLevel() - low.at(i).GetQuantity();
      out << std::left << std::setw(10) << low.at(i).GetProductID()
          << std::setw(26) << low.at(i).GetName()
          << std::right << std::setw(8) << low.at(i).GetQuantity()
          << std::setw(10) << low.at(i).GetReorderLevel()
          << std::setw(10) << shortfall
          << "   " << low.at(i).GetSupplierID() << "\n";
   }

   out << "----------------------------------------------------------------------------------\n";
   out << "   " << low.size() << " product(s) need reordering.\n";
   return out.str();
}

std::string Inventory::ReportSalesPerformance() const {
   std::ostringstream out;
   out << "SALES PERFORMANCE REPORT\n";
   out << "==================================================================================\n";

   // Only delivered sales count as revenue. Processing and shipped orders are
   // not money yet, and returned orders gave the money back.
   double revenue = 0.0;
   long long unitsSold = 0;
   int deliveredCount = 0;
   int openCount = 0;
   int returnedCount = 0;
   int cancelledCount = 0;

   std::vector<std::string> productIDs;
   std::vector<std::string> productNames;
   std::vector<double> productRevenue;

   for (size_t i = 0; i < orders.size(); ++i) {
      if (orders.at(i).GetOrderType() != OrderType::SALE) {
         continue;
      }

      OrderStatus status = orders.at(i).GetStatus();
      if (status == OrderStatus::PROCESSING || status == OrderStatus::SHIPPED) {
         ++openCount;
         continue;
      }
      if (status == OrderStatus::RETURNED) {
         ++returnedCount;
         continue;
      }
      if (status == OrderStatus::CANCELLED) {
         ++cancelledCount;
         continue;
      }

      ++deliveredCount;
      revenue += orders.at(i).GetOrderTotal();
      unitsSold += orders.at(i).GetTotalUnits();

      // Grouped by product ID rather than by name, so two different products
      // that happen to share a name are not reported as one seller.
      const std::vector<OrderLine>& lines = orders.at(i).GetOrderedProducts();
      for (size_t j = 0; j < lines.size(); ++j) {
         bool found = false;
         for (size_t k = 0; k < productIDs.size(); ++k) {
            if (productIDs.at(k) == lines.at(j).productID) {
               productRevenue.at(k) += lines.at(j).GetLineTotal();
               found = true;
               break;
            }
         }
         if (!found) {
            productIDs.push_back(lines.at(j).productID);
            productNames.push_back(lines.at(j).productName.empty()
                                   ? lines.at(j).productID
                                   : lines.at(j).productName);
            productRevenue.push_back(lines.at(j).GetLineTotal());
         }
      }
   }

   out << "   Delivered sales orders: " << deliveredCount << "\n";
   out << "   Open sales orders:      " << openCount << "\n";
   out << "   Returned orders:        " << returnedCount << "\n";
   out << "   Cancelled orders:       " << cancelledCount << "\n";
   out << "   Units sold:             " << unitsSold << "\n";
   out << "   Revenue:                " << std::fixed << std::setprecision(2) << revenue << "\n";
   if (deliveredCount > 0) {
      out << "   Average order value:    " << (revenue / deliveredCount) << "\n";
   }
   out << "\n";

   // Rank products by revenue and chart the strongest performers.
   for (size_t i = 0; i < productRevenue.size(); ++i) {
      for (size_t j = i + 1; j < productRevenue.size(); ++j) {
         if (productRevenue.at(j) > productRevenue.at(i)) {
            std::swap(productRevenue.at(i), productRevenue.at(j));
            std::swap(productNames.at(i), productNames.at(j));
            std::swap(productIDs.at(i), productIDs.at(j));
         }
      }
   }

   std::vector<ChartUtil::Bar> bars;
   for (size_t i = 0; i < productNames.size() && i < 8; ++i) {
      bars.push_back(ChartUtil::Bar(productNames.at(i), productRevenue.at(i)));
   }

   out << "   Revenue by product (top " << bars.size() << ")\n";
   out << ChartUtil::HorizontalBarChart(bars, 40, 2);
   return out.str();
}

std::string Inventory::ReportSupplierEfficiency() const {
   std::ostringstream out;
   out << "SUPPLIER EFFICIENCY REPORT\n";
   out << "==================================================================================\n";
   out << std::left << std::setw(10) << "ID" << std::setw(26) << "SUPPLIER"
       << std::right << std::setw(9) << "ORDERS" << std::setw(10) << "ON TIME"
       << std::setw(11) << "ON-TIME %" << std::setw(14) << "AVG DAYS LATE" << "\n";
   out << "----------------------------------------------------------------------------------\n";

   std::vector<ChartUtil::Bar> bars;

   for (size_t s = 0; s < suppliers.size(); ++s) {
      std::string id = suppliers.at(s).GetSupplierID();

      int delivered = 0;
      int onTime = 0;
      long totalVariance = 0;

      for (size_t i = 0; i < orders.size(); ++i) {
         if (orders.at(i).GetOrderType() != OrderType::PURCHASE) {
            continue;
         }
         if (orders.at(i).GetCounterpartyID() != id) {
            continue;
         }
         if (orders.at(i).GetStatus() != OrderStatus::DELIVERED) {
            continue;
         }

         ++delivered;
         if (!orders.at(i).WasLate()) {
            ++onTime;
         }
         // Only lateness is averaged; arriving early is recorded as zero so a
         // very early delivery cannot mask a very late one.
         long variance = orders.at(i).GetDeliveryVariance();
         if (variance > 0) {
            totalVariance += variance;
         }
      }

      double onTimeRate = (delivered > 0) ? (100.0 * onTime / delivered) : 0.0;
      double averageLate = (delivered > 0) ? (static_cast<double>(totalVariance) / delivered) : 0.0;

      out << std::left << std::setw(10) << id
          << std::setw(26) << suppliers.at(s).GetSupplierName()
          << std::right << std::setw(9) << delivered
          << std::setw(10) << onTime
          << std::setw(11) << std::fixed << std::setprecision(1) << onTimeRate
          << std::setw(14) << std::setprecision(2) << averageLate << "\n";

      if (delivered > 0) {
         bars.push_back(ChartUtil::Bar(suppliers.at(s).GetSupplierName(), onTimeRate));
      }
   }

   if (suppliers.empty()) {
      out << "   (no suppliers on file)\n";
   }

   out << "----------------------------------------------------------------------------------\n";
   out << "   On-time delivery rate by supplier (percent)\n";
   out << ChartUtil::HorizontalBarChart(bars, 40, 1);
   out << "\n   Note. Only delivered purchase orders are counted. A supplier with no\n";
   out << "   delivered orders is listed but not charted.\n";
   return out.str();
}

std::string Inventory::ReportMonthlySummary(int year) const {
   std::ostringstream out;
   out << "MONTHLY SUMMARY FOR " << year << "\n";
   out << "==================================================================================\n";

   double monthlyRevenue[12] = { 0 };
   int monthlyOrders[12] = { 0 };
   long long monthlyUnits[12] = { 0 };

   for (size_t i = 0; i < orders.size(); ++i) {
      if (orders.at(i).GetOrderType() != OrderType::SALE) {
         continue;
      }
      if (orders.at(i).GetStatus() != OrderStatus::DELIVERED) {
         continue;
      }
      if (orders.at(i).GetOrderDate().GetYear() != year) {
         continue;
      }

      int month = orders.at(i).GetOrderDate().GetMonth() - 1;
      if (month < 0 || month > 11) {
         continue;
      }

      monthlyRevenue[month] += orders.at(i).GetOrderTotal();
      monthlyOrders[month] += 1;
      monthlyUnits[month] += orders.at(i).GetTotalUnits();
   }

   const char* names[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                             "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

   out << std::left << std::setw(8) << "MONTH" << std::right << std::setw(10) << "ORDERS"
       << std::setw(10) << "UNITS" << std::setw(14) << "REVENUE" << "\n";
   out << "----------------------------------------------------------------------------------\n";

   double yearRevenue = 0.0;
   int yearOrders = 0;
   long long yearUnits = 0;
   std::vector<ChartUtil::Bar> bars;

   for (int m = 0; m < 12; ++m) {
      out << std::left << std::setw(8) << names[m]
          << std::right << std::setw(10) << monthlyOrders[m]
          << std::setw(10) << monthlyUnits[m]
          << std::setw(14) << std::fixed << std::setprecision(2) << monthlyRevenue[m] << "\n";
      yearRevenue += monthlyRevenue[m];
      yearOrders += monthlyOrders[m];
      yearUnits += monthlyUnits[m];
      bars.push_back(ChartUtil::Bar(names[m], monthlyRevenue[m]));
   }

   out << "----------------------------------------------------------------------------------\n";
   out << std::left << std::setw(8) << "TOTAL" << std::right << std::setw(10) << yearOrders
       << std::setw(10) << yearUnits
       << std::setw(14) << std::fixed << std::setprecision(2) << yearRevenue << "\n\n";

   out << "   Revenue by month\n";
   out << ChartUtil::TrendLine(bars, 8);
   return out.str();
}

std::string Inventory::ReportYearlySummary() const {
   std::ostringstream out;
   out << "YEARLY SUMMARY\n";
   out << "==================================================================================\n";

   std::vector<int> years;
   std::vector<double> revenue;
   std::vector<int> counts;

   for (size_t i = 0; i < orders.size(); ++i) {
      if (orders.at(i).GetOrderType() != OrderType::SALE) {
         continue;
      }
      if (orders.at(i).GetStatus() != OrderStatus::DELIVERED) {
         continue;
      }

      int year = orders.at(i).GetOrderDate().GetYear();
      bool found = false;
      for (size_t j = 0; j < years.size(); ++j) {
         if (years.at(j) == year) {
            revenue.at(j) += orders.at(i).GetOrderTotal();
            counts.at(j) += 1;
            found = true;
            break;
         }
      }
      if (!found) {
         years.push_back(year);
         revenue.push_back(orders.at(i).GetOrderTotal());
         counts.push_back(1);
      }
   }

   // Oldest year first so the table reads as a timeline.
   for (size_t i = 0; i < years.size(); ++i) {
      for (size_t j = i + 1; j < years.size(); ++j) {
         if (years.at(j) < years.at(i)) {
            std::swap(years.at(i), years.at(j));
            std::swap(revenue.at(i), revenue.at(j));
            std::swap(counts.at(i), counts.at(j));
         }
      }
   }

   out << std::left << std::setw(10) << "YEAR" << std::right << std::setw(10) << "ORDERS"
       << std::setw(14) << "REVENUE" << std::setw(16) << "AVG ORDER" << "\n";
   out << "----------------------------------------------------------------------------------\n";

   std::vector<ChartUtil::Bar> bars;
   for (size_t i = 0; i < years.size(); ++i) {
      double average = (counts.at(i) > 0) ? revenue.at(i) / counts.at(i) : 0.0;
      out << std::left << std::setw(10) << years.at(i)
          << std::right << std::setw(10) << counts.at(i)
          << std::setw(14) << std::fixed << std::setprecision(2) << revenue.at(i)
          << std::setw(16) << average << "\n";
      bars.push_back(ChartUtil::Bar(std::to_string(years.at(i)), revenue.at(i)));
   }

   if (years.empty()) {
      out << "   (no delivered sales on file)\n";
   }

   out << "----------------------------------------------------------------------------------\n";
   out << "   Revenue by year\n";
   out << ChartUtil::HorizontalBarChart(bars, 40, 2);
   return out.str();
}

std::string Inventory::ReportDashboard() const {
   double stockValue = 0.0;
   long long units = 0;
   for (size_t i = 0; i < products.size(); ++i) {
      stockValue += products.at(i).GetStockValue();
      units += static_cast<long long>(products.at(i).GetQuantity());
   }

   int openOrders = 0;
   for (size_t i = 0; i < orders.size(); ++i) {
      OrderStatus status = orders.at(i).GetStatus();
      if (status == OrderStatus::PROCESSING || status == OrderStatus::SHIPPED) {
         ++openOrders;
      }
   }

   std::vector<Product> low = GetLowStockProducts();

   std::ostringstream out;
   out << "   Products: " << products.size()
       << "   Suppliers: " << suppliers.size()
       << "   Orders: " << orders.size()
       << "   Open: " << openOrders << "\n";
   out << "   Units on hand: " << units
       << "   Inventory value: " << std::fixed << std::setprecision(2) << stockValue << "\n";
   if (!low.empty()) {
      out << "   ALERT: " << low.size() << " product(s) at or below reorder level.\n";
   }
   return out.str();
}
