#ifndef CHARTUTIL_H
#define CHARTUTIL_H

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// ChartUtil
// ---------------------------------------------------------------------------
// Text-mode data visualisation for the reporting screens. A console program
// has no drawing surface, so charts are built from characters: each bar is a
// run of block characters scaled against the largest value in the series.
// Every chart prints the underlying number beside its bar, so the figure is
// readable even where the bars are too short to compare by eye.
// ---------------------------------------------------------------------------
namespace ChartUtil {

   // One labelled value in a chart.
   struct Bar {
      std::string label;
      double value;

      Bar();
      Bar(const std::string& barLabel, double barValue);
   };

   // Default width in characters of the longest bar in a chart.
   const int DEFAULT_WIDTH = 40;

   // Renders a horizontal bar chart. Bars are scaled so the largest value
   // fills maxWidth characters; a series of all zeros renders as empty bars
   // rather than dividing by zero. Values are printed to the given number of
   // decimal places.
   std::string HorizontalBarChart(const std::vector<Bar>& bars,
                                  int maxWidth = DEFAULT_WIDTH,
                                  int decimals = 0);

   // Renders a sparkline-style trend row: twelve short bars on one line, used
   // for the month-by-month summaries where a full chart would waste space.
   std::string TrendLine(const std::vector<Bar>& bars, int maxHeight = 8);

   // Builds a single bar string of the given length.
   std::string MakeBar(int length);
}

#endif
