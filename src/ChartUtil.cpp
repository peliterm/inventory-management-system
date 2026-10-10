#include "ChartUtil.h"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace ChartUtil {

Bar::Bar() {
   value = 0.0;
}

Bar::Bar(const std::string& barLabel, double barValue) {
   label = barLabel;
   value = barValue;
}

std::string MakeBar(int length) {
   std::string bar;
   for (int i = 0; i < length; ++i) {
      bar += '#';
   }
   return bar;
}

std::string HorizontalBarChart(const std::vector<Bar>& bars, int maxWidth,
                               int decimals) {
   if (bars.empty()) {
      return "   (no data to chart)\n";
   }

   // Find the largest magnitude so every bar can be scaled against it, and the
   // longest label so the bars all start in the same column.
   double largest = 0.0;
   size_t labelWidth = 0;
   for (size_t i = 0; i < bars.size(); ++i) {
      double magnitude = std::fabs(bars.at(i).value);
      if (magnitude > largest) {
         largest = magnitude;
      }
      if (bars.at(i).label.size() > labelWidth) {
         labelWidth = bars.at(i).label.size();
      }
   }

   std::ostringstream out;
   for (size_t i = 0; i < bars.size(); ++i) {
      int length = 0;
      // A series of all zeros has no scale, so every bar is drawn empty
      // instead of dividing by zero.
      if (largest > 0.0) {
         length = static_cast<int>((std::fabs(bars.at(i).value) / largest) * maxWidth + 0.5);
      }

      out << "   " << std::left << std::setw(static_cast<int>(labelWidth) + 2)
          << bars.at(i).label
          << std::setw(maxWidth + 2) << MakeBar(length)
          << std::right << std::fixed << std::setprecision(decimals)
          << bars.at(i).value << "\n";
   }

   return out.str();
}

std::string TrendLine(const std::vector<Bar>& bars, int maxHeight) {
   if (bars.empty()) {
      return "   (no data to chart)\n";
   }

   double largest = 0.0;
   for (size_t i = 0; i < bars.size(); ++i) {
      if (std::fabs(bars.at(i).value) > largest) {
         largest = std::fabs(bars.at(i).value);
      }
   }

   // Draw from the top row down, printing a block wherever the column is at
   // least as tall as the current row.
   std::ostringstream out;
   for (int row = maxHeight; row >= 1; --row) {
      out << "   ";
      for (size_t i = 0; i < bars.size(); ++i) {
         int height = 0;
         if (largest > 0.0) {
            height = static_cast<int>((std::fabs(bars.at(i).value) / largest) * maxHeight + 0.5);
         }
         out << (height >= row ? " # " : "   ");
      }
      out << "\n";
   }

   out << "   ";
   for (size_t i = 0; i < bars.size(); ++i) {
      std::string label = bars.at(i).label;
      if (label.size() > 3) {
         label = label.substr(0, 3);
      }
      out << std::left << std::setw(3) << label;
   }
   out << "\n";

   return out.str();
}

}
