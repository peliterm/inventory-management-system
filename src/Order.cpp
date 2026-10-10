#include "Order.h"
#include "CSVUtil.h"

#include <cctype>
#include <iomanip>
#include <limits>
#include <sstream>

OrderLine::OrderLine() {
   quantity = 0;
   unitPrice = 0.0;
}

OrderLine::OrderLine(const std::string& id, const std::string& name, int qty,
                     double price) {
   productID = id;
   productName = name;
   quantity = qty;
   unitPrice = price;
}

double OrderLine::GetLineTotal() const {
   return quantity * unitPrice;
}

Order::Order() {
   delivered = false;
   orderStatus = OrderStatus::PROCESSING;
   orderType = OrderType::SALE;
}

Order::Order(const std::string& id, OrderType type, const Date& placed,
             const std::string& counterparty) {
   orderID = id;
   orderType = type;
   orderDate = placed;
   estimatedDelivery = placed.AddDays(DEFAULT_LEAD_DAYS);
   delivered = false;
   orderStatus = OrderStatus::PROCESSING;
   counterpartyID = counterparty;
}

std::string Order::GetOrderID() const { return orderID; }
OrderType Order::GetOrderType() const { return orderType; }
OrderStatus Order::GetStatus() const { return orderStatus; }
Date Order::GetOrderDate() const { return orderDate; }
Date Order::GetEstimatedDelivery() const { return estimatedDelivery; }
std::string Order::GetCounterpartyID() const { return counterpartyID; }

const std::vector<OrderLine>& Order::GetOrderedProducts() const {
   return orderedProducts;
}

bool Order::GetDeliveryDate(Date& out) const {
   if (!delivered) {
      return false;
   }
   out = deliveryDate;
   return true;
}

void Order::SetOrderID(const std::string& id) { orderID = id; }
void Order::SetOrderDate(const Date& placed) { orderDate = placed; }
void Order::SetEstimatedDelivery(const Date& estimate) { estimatedDelivery = estimate; }
void Order::SetCounterpartyID(const std::string& counterparty) { counterpartyID = counterparty; }

bool Order::AddProduct(const std::string& productID, const std::string& productName,
                       int quantity, double unitPrice) {
   if (quantity <= 0) {
      return false;
   }
   // Contents are frozen once the order leaves the processing stage.
   if (orderStatus != OrderStatus::PROCESSING) {
      return false;
   }

   for (size_t i = 0; i < orderedProducts.size(); ++i) {
      if (orderedProducts.at(i).productID == productID) {
         // Refuse a merge that would overflow the line quantity. Without this
         // the combined value wraps negative, and a negative quantity passes
         // every availability check and then adds stock instead of removing it.
         if (orderedProducts.at(i).quantity >
             std::numeric_limits<int>::max() - quantity) {
            return false;
         }
         orderedProducts.at(i).quantity += quantity;
         return true;
      }
   }

   orderedProducts.push_back(OrderLine(productID, productName, quantity, unitPrice));
   return true;
}

bool Order::RemoveProduct(const std::string& productID) {
   if (orderStatus != OrderStatus::PROCESSING) {
      return false;
   }

   for (size_t i = 0; i < orderedProducts.size(); ++i) {
      if (orderedProducts.at(i).productID == productID) {
         orderedProducts.erase(orderedProducts.begin() + static_cast<long>(i));
         return true;
      }
   }
   return false;
}

bool Order::IsLegalTransition(OrderStatus from, OrderStatus next) {
   switch (from) {
      case OrderStatus::PROCESSING:
         return next == OrderStatus::SHIPPED || next == OrderStatus::CANCELLED;
      case OrderStatus::SHIPPED:
         return next == OrderStatus::DELIVERED || next == OrderStatus::CANCELLED;
      case OrderStatus::DELIVERED:
         return next == OrderStatus::RETURNED;
      case OrderStatus::RETURNED:
      case OrderStatus::CANCELLED:
         return false;   // both are terminal
   }
   return false;
}

bool Order::UpdateStatus(OrderStatus next, const Date& when) {
   if (!IsLegalTransition(orderStatus, next)) {
      return false;
   }

   if (next == OrderStatus::DELIVERED) {
      deliveryDate = when;
      delivered = true;
   }

   orderStatus = next;
   return true;
}

double Order::GetOrderTotal() const {
   double total = 0.0;
   for (size_t i = 0; i < orderedProducts.size(); ++i) {
      total += orderedProducts.at(i).GetLineTotal();
   }
   return total;
}

long long Order::GetTotalUnits() const {
   // Accumulated in a wider type than the line quantities. Each quantity fits
   // in an int, but a handful of large lines added together does not, and the
   // sum feeds the units-sold figures on the reports.
   long long units = 0;
   for (size_t i = 0; i < orderedProducts.size(); ++i) {
      units += static_cast<long long>(orderedProducts.at(i).quantity);
   }
   return units;
}

bool Order::WasLate() const {
   if (!delivered) {
      return false;
   }
   return estimatedDelivery < deliveryDate;
}

long Order::GetDeliveryVariance() const {
   if (!delivered) {
      return 0;
   }
   return estimatedDelivery.DaysUntil(deliveryDate);
}

std::string Order::GetDetails() const {
   std::ostringstream out;
   out << std::left << std::setw(10) << orderID
       << std::setw(10) << TypeToString(orderType)
       << std::setw(20) << counterpartyID
       << std::setw(12) << orderDate.ToString()
       << std::setw(12) << StatusToString(orderStatus)
       << std::right << std::setw(6) << GetTotalUnits()
       << std::setw(12) << std::fixed << std::setprecision(2) << GetOrderTotal();
   return out.str();
}

std::string Order::StatusToString(OrderStatus status) {
   switch (status) {
      case OrderStatus::PROCESSING: return "PROCESSING";
      case OrderStatus::SHIPPED:    return "SHIPPED";
      case OrderStatus::DELIVERED:  return "DELIVERED";
      case OrderStatus::RETURNED:   return "RETURNED";
      case OrderStatus::CANCELLED:  return "CANCELLED";
   }
   return "UNKNOWN";
}

bool Order::StatusFromString(const std::string& text, OrderStatus& out) {
   std::string upper = CSVUtil::Trim(text);
   for (size_t i = 0; i < upper.size(); ++i) {
      upper.at(i) = static_cast<char>(std::toupper(static_cast<unsigned char>(upper.at(i))));
   }

   if (upper == "PROCESSING") { out = OrderStatus::PROCESSING; return true; }
   if (upper == "SHIPPED")    { out = OrderStatus::SHIPPED;    return true; }
   if (upper == "DELIVERED")  { out = OrderStatus::DELIVERED;  return true; }
   if (upper == "RETURNED")   { out = OrderStatus::RETURNED;   return true; }
   if (upper == "CANCELLED")  { out = OrderStatus::CANCELLED;  return true; }
   return false;
}

std::string Order::TypeToString(OrderType type) {
   return (type == OrderType::SALE) ? "SALE" : "PURCHASE";
}

bool Order::TypeFromString(const std::string& text, OrderType& out) {
   std::string upper = CSVUtil::Trim(text);
   for (size_t i = 0; i < upper.size(); ++i) {
      upper.at(i) = static_cast<char>(std::toupper(static_cast<unsigned char>(upper.at(i))));
   }

   if (upper == "SALE")     { out = OrderType::SALE;     return true; }
   if (upper == "PURCHASE") { out = OrderType::PURCHASE; return true; }
   return false;
}

std::string Order::ToCSV() const {
   // Lines are packed into a single field as id:qty:price pairs separated by
   // semicolons, which keeps one order on one record.
   std::string packed;
   for (size_t i = 0; i < orderedProducts.size(); ++i) {
      if (i > 0) {
         packed += ";";
      }
      // Each part is escaped so a product name containing a colon or a
      // semicolon cannot split the record in the wrong place.
      std::ostringstream line;
      line << CSVUtil::EscapeDelims(orderedProducts.at(i).productID) << ':'
           << CSVUtil::EscapeDelims(orderedProducts.at(i).productName) << ':'
           << orderedProducts.at(i).quantity << ':'
           << std::fixed << std::setprecision(4) << orderedProducts.at(i).unitPrice;
      packed += line.str();
   }

   std::ostringstream out;
   out << CSVUtil::Escape(orderID) << ','
       << TypeToString(orderType) << ','
       << CSVUtil::Escape(counterpartyID) << ','
       << orderDate.ToString() << ','
       << estimatedDelivery.ToString() << ','
       << (delivered ? deliveryDate.ToString() : "") << ','
       << StatusToString(orderStatus) << ','
       << CSVUtil::Escape(packed);
   return out.str();
}

bool Order::ParseCSV(const std::string& line, Order& out) {
   std::vector<std::string> fields = CSVUtil::SplitLine(line);
   if (fields.size() != 8) {
      return false;
   }

   OrderType type = OrderType::SALE;
   OrderStatus status = OrderStatus::PROCESSING;
   Date placed;
   Date estimate;

   if (!TypeFromString(fields.at(1), type)) {
      return false;
   }
   if (!Date::Parse(fields.at(3), placed)) {
      return false;
   }
   if (!Date::Parse(fields.at(4), estimate)) {
      return false;
   }
   if (!StatusFromString(fields.at(6), status)) {
      return false;
   }

   Order built(fields.at(0), type, placed, fields.at(2));
   built.SetEstimatedDelivery(estimate);

   // Unpack the product lines before applying the status, because AddProduct
   // refuses to modify an order that has already shipped. A segment that does
   // not hold exactly four usable parts fails the whole record rather than
   // being dropped, so a corrupted file is reported instead of loading an
   // order whose contents have quietly gone missing.
   std::vector<std::string> segments = CSVUtil::SplitEscaped(fields.at(7), ';');
   for (size_t i = 0; i < segments.size(); ++i) {
      if (segments.at(i).empty()) {
         continue;
      }

      std::vector<std::string> parts = CSVUtil::SplitEscaped(segments.at(i), ':');
      if (parts.size() != 4) {
         return false;
      }

      int qty = 0;
      double price = 0.0;
      if (!CSVUtil::ToInt(parts.at(2), qty)) {
         return false;
      }
      if (!CSVUtil::ToDouble(parts.at(3), price)) {
         return false;
      }
      if (!built.AddProduct(CSVUtil::UnescapeDelims(parts.at(0)),
                            CSVUtil::UnescapeDelims(parts.at(1)), qty, price)) {
         return false;
      }
   }

   // Replay the lifecycle so the stored status is reached through legal moves.
   Date actual;
   bool haveActual = Date::Parse(fields.at(5), actual);

   if (status == OrderStatus::SHIPPED || status == OrderStatus::DELIVERED ||
       status == OrderStatus::RETURNED) {
      built.UpdateStatus(OrderStatus::SHIPPED, placed);
   }
   if (status == OrderStatus::DELIVERED || status == OrderStatus::RETURNED) {
      built.UpdateStatus(OrderStatus::DELIVERED, haveActual ? actual : estimate);
   }
   if (status == OrderStatus::RETURNED) {
      built.UpdateStatus(OrderStatus::RETURNED, haveActual ? actual : estimate);
   }
   if (status == OrderStatus::CANCELLED) {
      built.UpdateStatus(OrderStatus::CANCELLED, placed);
   }

   out = built;
   return true;
}

std::string Order::CSVHeader() {
   return "orderID,type,counterparty,orderDate,estimatedDelivery,deliveryDate,status,lines";
}
