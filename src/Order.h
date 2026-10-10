#ifndef ORDER_H
#define ORDER_H

#include "Date.h"

#include <string>
#include <vector>

// Where an order sits in its lifecycle. Returns are tracked as a terminal
// state of their own rather than by deleting the order, so a returned sale
// still appears in the history with its original dates intact.
enum class OrderStatus {
   PROCESSING,
   SHIPPED,
   DELIVERED,
   RETURNED,
   CANCELLED
};

// Which direction stock moves. A SALE ships product out to a customer and is
// what the sales-performance report measures. A PURCHASE brings product in
// from a supplier and is what the supplier-efficiency report measures.
enum class OrderType {
   SALE,
   PURCHASE
};

// ---------------------------------------------------------------------------
// OrderLine
// ---------------------------------------------------------------------------
// One product on an order. The name and unit price are copied in at the time
// the line is created rather than looked up later, so a price change next
// month does not rewrite the value of an order placed today.
// ---------------------------------------------------------------------------
struct OrderLine {
   std::string productID;
   std::string productName;
   int quantity;
   double unitPrice;

   OrderLine();
   OrderLine(const std::string& id, const std::string& name, int qty, double price);

   // Quantity multiplied by the unit price recorded on this line.
   double GetLineTotal() const;
};

// ---------------------------------------------------------------------------
// Order
// ---------------------------------------------------------------------------
// A customer sale or a supplier purchase, holding the products involved, the
// dates that bound it, and its position in the lifecycle. The class owns its
// own state transition rules so an invalid move, such as shipping an order
// that has already been delivered, is refused in one place.
// ---------------------------------------------------------------------------
class Order {
private:
   std::string orderID;
   std::vector<OrderLine> orderedProducts;
   Date orderDate;
   Date estimatedDelivery;   // promised date, set when the order is placed
   Date deliveryDate;        // actual date, set when the status becomes DELIVERED
   bool delivered;           // guards deliveryDate, which is meaningless until then
   OrderStatus orderStatus;
   OrderType orderType;
   std::string counterpartyID;   // customer name for a sale, supplier ID for a purchase

public:
   // Creates an empty PROCESSING sale dated to the epoch.
   Order();

   // Creates an order with no lines yet. The estimated delivery date defaults
   // to the standard lead time measured from the order date.
   Order(const std::string& id, OrderType type, const Date& placed,
         const std::string& counterparty);

   // Default lead time in days used when no estimate is supplied.
   static const int DEFAULT_LEAD_DAYS = 7;

   std::string GetOrderID() const;
   OrderType GetOrderType() const;
   OrderStatus GetStatus() const;
   Date GetOrderDate() const;
   Date GetEstimatedDelivery() const;
   std::string GetCounterpartyID() const;

   // Read-only view of the lines on this order.
   const std::vector<OrderLine>& GetOrderedProducts() const;

   // Writes the actual delivery date into out and returns true only once the
   // order has actually been delivered.
   bool GetDeliveryDate(Date& out) const;

   void SetOrderID(const std::string& id);
   void SetOrderDate(const Date& placed);
   void SetEstimatedDelivery(const Date& estimate);
   void SetCounterpartyID(const std::string& counterparty);

   // Adds a product line. When the product is already on the order the
   // quantities are combined instead of creating a duplicate line, and the
   // unit price already recorded is kept. Returns false for a non-positive
   // quantity, once the order has left PROCESSING (a shipped order must not
   // change contents), or when combining would overflow the line quantity.
   bool AddProduct(const std::string& productID, const std::string& productName,
                   int quantity, double unitPrice);

   // Removes a product line entirely. Returns false when the product is not on
   // the order or the order has left PROCESSING.
   bool RemoveProduct(const std::string& productID);

   // Moves the order to a new status, enforcing the lifecycle rules:
   // PROCESSING may ship or cancel, SHIPPED may deliver or cancel, DELIVERED
   // may be returned, and RETURNED and CANCELLED are terminal. Returns false
   // on an illegal move. Supplying the date records it as the actual delivery
   // date when the new status is DELIVERED.
   bool UpdateStatus(OrderStatus next, const Date& when);

   // True when the lifecycle permits the given move. Exposed so a caller can
   // test a transition before committing to it, which lets the inventory
   // check and move stock first and leave the order untouched on failure.
   static bool IsLegalTransition(OrderStatus from, OrderStatus next);

   // Total value of every line on the order.
   double GetOrderTotal() const;

   // Total number of units across every line.
   long long GetTotalUnits() const;

   // True when the order was delivered later than the date promised.
   bool WasLate() const;

   // Days between the promised and actual delivery dates. Negative when early,
   // zero when the order has not been delivered.
   long GetDeliveryVariance() const;

   // Single-line summary used by the CLI listings.
   std::string GetDetails() const;

   // Human-readable status and type names, and their inverses for CSV import.
   static std::string StatusToString(OrderStatus status);
   static bool StatusFromString(const std::string& text, OrderStatus& out);
   static std::string TypeToString(OrderType type);
   static bool TypeFromString(const std::string& text, OrderType& out);

   // Serializes the order header and its lines to one CSV record.
   std::string ToCSV() const;
   static bool ParseCSV(const std::string& line, Order& out);
   static std::string CSVHeader();
};

#endif
