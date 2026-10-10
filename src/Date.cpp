#include "Date.h"

#include <cstdio>
#include <ctime>

Date::Date() {
   year = 1970;
   month = 1;
   day = 1;
}

Date::Date(int y, int m, int d) {
   year = y;
   month = m;
   day = d;
}

int Date::GetYear() const { return year; }
int Date::GetMonth() const { return month; }
int Date::GetDay() const { return day; }

bool Date::IsValid() const {
   // The year is bounded so a hand-edited file cannot inject a date that
   // formats to something other than YYYY-MM-DD or overflows date arithmetic.
   if (year < MIN_YEAR || year > MAX_YEAR) {
      return false;
   }
   if (month < 1 || month > 12) {
      return false;
   }
   if (day < 1) {
      return false;
   }

   int lengths[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
   int limit = lengths[month - 1];

   // February gains a day in years divisible by 4, except centuries that are
   // not divisible by 400.
   if (month == 2) {
      bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
      if (leap) {
         limit = 29;
      }
   }

   return day <= limit;
}

// Days from 1970-01-01 using the civil-calendar algorithm: shift the year so
// that March starts the year, which makes the leap day the last day and
// removes the special case from the month-length table.
long Date::ToSerial() const {
   long y = year;
   long m = month;
   long d = day;

   y -= (m <= 2) ? 1 : 0;
   long era = (y >= 0 ? y : y - 399) / 400;
   long yoe = y - era * 400;                                   // [0, 399]
   long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;  // [0, 365]
   long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;           // [0, 146096]

   return era * 146097 + doe - 719468;
}

Date Date::FromSerial(long serial) {
   long z = serial + 719468;
   long era = (z >= 0 ? z : z - 146096) / 146097;
   long doe = z - era * 146097;
   long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
   long y = yoe + era * 400;
   long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
   long mp = (5 * doy + 2) / 153;
   long d = doy - (153 * mp + 2) / 5 + 1;
   long m = mp + (mp < 10 ? 3 : -9);

   y += (m <= 2) ? 1 : 0;

   return Date(static_cast<int>(y), static_cast<int>(m), static_cast<int>(d));
}

Date Date::AddDays(int days) const {
   return FromSerial(ToSerial() + days);
}

long Date::DaysUntil(const Date& other) const {
   return other.ToSerial() - ToSerial();
}

std::string Date::ToString() const {
   char buffer[16];
   std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", year, month, day);
   return std::string(buffer);
}

bool Date::Parse(const std::string& text, Date& out) {
   int y = 0;
   int m = 0;
   int d = 0;

   if (text.size() != 10 || text.at(4) != '-' || text.at(7) != '-') {
      return false;
   }
   // Visual Studio treats sscanf as unsafe and offers sscanf_s, which for
   // these numeric conversions takes the same arguments.
#if defined(_MSC_VER)
   if (sscanf_s(text.c_str(), "%4d-%2d-%2d", &y, &m, &d) != 3) {
      return false;
   }
#else
   if (std::sscanf(text.c_str(), "%4d-%2d-%2d", &y, &m, &d) != 3) {
      return false;
   }
#endif

   Date candidate(y, m, d);
   if (!candidate.IsValid()) {
      return false;
   }

   out = candidate;
   return true;
}

Date Date::Today() {
   std::time_t now = std::time(nullptr);
   std::tm local;

   // localtime() returns a pointer into a shared buffer, which Visual Studio
   // rejects as unsafe. Each toolchain has its own re-entrant replacement and
   // they take their arguments in the opposite order, so both are spelled out
   // rather than silencing the warning.
#if defined(_MSC_VER)
   if (localtime_s(&local, &now) != 0) {
      return Date(1970, 1, 1);
   }
#else
   if (localtime_r(&now, &local) == nullptr) {
      return Date(1970, 1, 1);
   }
#endif

   return Date(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
}

bool Date::operator<(const Date& other) const {
   return ToSerial() < other.ToSerial();
}

bool Date::operator==(const Date& other) const {
   return year == other.year && month == other.month && day == other.day;
}

bool Date::operator<=(const Date& other) const {
   return ToSerial() <= other.ToSerial();
}
