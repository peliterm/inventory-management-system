#ifndef DATE_H
#define DATE_H

#include <string>

// ---------------------------------------------------------------------------
// Date
// ---------------------------------------------------------------------------
// A lightweight calendar date used for order dates, delivery dates, and the
// monthly/yearly reporting periods. The class deliberately avoids <ctime>
// arithmetic because that API mutates shared state and is awkward to test.
// Instead it converts between a civil date (year/month/day) and a serial day
// number, which makes "add N days" and "days between" simple integer math.
// ---------------------------------------------------------------------------
class Date {
private:
   int year;
   int month;
   int day;

public:
   // Constructs 1970-01-01, the serial-day epoch used by this class.
   Date();

   // Constructs a specific calendar date. No validation is performed here;
   // use IsValid() to check a date built from user input.
   Date(int y, int m, int d);

   int GetYear() const;
   int GetMonth() const;
   int GetDay() const;

   // Accepted year range. Dates outside it are rejected by IsValid(), which
   // keeps every formatted date exactly ten characters wide and keeps the
   // serial-day arithmetic well inside the range of a long.
   static const int MIN_YEAR = 1900;
   static const int MAX_YEAR = 9999;

   // True when the date names a real calendar day inside the accepted year
   // range, leap years included.
   bool IsValid() const;

   // Serial day number relative to 1970-01-01. Negative for earlier dates.
   long ToSerial() const;

   // Rebuilds a Date from a serial day number produced by ToSerial().
   static Date FromSerial(long serial);

   // Returns a new Date offset by the given number of days.
   Date AddDays(int days) const;

   // Whole days from this date to other. Positive when other is later.
   long DaysUntil(const Date& other) const;

   // Formats as YYYY-MM-DD, the same form Parse() accepts.
   std::string ToString() const;

   // Parses YYYY-MM-DD. Returns false and leaves out untouched on failure.
   static bool Parse(const std::string& text, Date& out);

   // Today's date taken from the system clock.
   static Date Today();

   bool operator<(const Date& other) const;
   bool operator==(const Date& other) const;
   bool operator<=(const Date& other) const;
};

#endif
