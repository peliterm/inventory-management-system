#include "CSVUtil.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace CSVUtil {

std::vector<std::string> SplitLine(const std::string& line) {
   std::vector<std::string> fields;
   std::string current;
   bool inQuotes = false;
   size_t i = 0;

   while (i < line.size()) {
      char c = line.at(i);

      if (inQuotes) {
         if (c == '"') {
            // A doubled quote inside a quoted field is one literal quote.
            if (i + 1 < line.size() && line.at(i + 1) == '"') {
               current += '"';
               ++i;
            }
            else {
               inQuotes = false;
            }
         }
         else {
            current += c;
         }
      }
      else {
         if (c == '"') {
            inQuotes = true;
         }
         else if (c == ',') {
            fields.push_back(Trim(current));
            current.clear();
         }
         else {
            current += c;
         }
      }
      ++i;
   }

   fields.push_back(Trim(current));
   return fields;
}

std::string Escape(const std::string& field) {
   // A line break cannot survive a reader that works one line at a time, so
   // it is folded to a space rather than quoted. Quoting it would produce a
   // file this program could not read back.
   std::string flattened;
   for (size_t i = 0; i < field.size(); ++i) {
      char c = field.at(i);
      flattened += (c == '\n' || c == '\r') ? ' ' : c;
   }

   bool needsQuotes = false;
   for (size_t i = 0; i < flattened.size(); ++i) {
      char c = flattened.at(i);
      if (c == ',' || c == '"') {
         needsQuotes = true;
         break;
      }
   }

   if (!needsQuotes) {
      return flattened;
   }

   std::string out = "\"";
   for (size_t i = 0; i < flattened.size(); ++i) {
      char c = flattened.at(i);
      if (c == '"') {
         out += "\"\"";
      }
      else {
         out += c;
      }
   }
   out += "\"";
   return out;
}

bool ToInt(const std::string& text, int& out) {
   std::string trimmed = Trim(text);
   if (trimmed.empty()) {
      return false;
   }

   errno = 0;
   char* end = nullptr;
   long value = std::strtol(trimmed.c_str(), &end, 10);

   // strtol stops at the first character it cannot use; if that is not the
   // terminator then the field held something other than a plain integer.
   if (end == nullptr || *end != '\0') {
      return false;
   }

   // A value too large for long sets ERANGE, and a value that fits in long but
   // not in int would be silently truncated by the cast, so both are rejected
   // rather than quietly becoming a different number.
   if (errno == ERANGE) {
      return false;
   }
   if (value > std::numeric_limits<int>::max() ||
       value < std::numeric_limits<int>::min()) {
      return false;
   }

   out = static_cast<int>(value);
   return true;
}

bool ToDouble(const std::string& text, double& out) {
   std::string trimmed = Trim(text);
   if (trimmed.empty()) {
      return false;
   }

   errno = 0;
   char* end = nullptr;
   double value = std::strtod(trimmed.c_str(), &end);

   if (end == nullptr || *end != '\0') {
      return false;
   }

   // Reject overflow, and reject infinity and not-a-number outright. strtod
   // accepts the literal text "inf" and "nan", and a single such value would
   // turn every total that touches it into nan.
   if (errno == ERANGE) {
      return false;
   }
   if (!std::isfinite(value)) {
      return false;
   }

   out = value;
   return true;
}

std::string Trim(const std::string& text) {
   size_t first = 0;
   size_t last = text.size();

   while (first < last && std::isspace(static_cast<unsigned char>(text.at(first)))) {
      ++first;
   }
   while (last > first && std::isspace(static_cast<unsigned char>(text.at(last - 1)))) {
      --last;
   }

   return text.substr(first, last - first);
}

std::string ToLower(const std::string& text) {
   std::string out = text;
   for (size_t i = 0; i < out.size(); ++i) {
      out.at(i) = static_cast<char>(std::tolower(static_cast<unsigned char>(out.at(i))));
   }
   return out;
}

bool ContainsIgnoreCase(const std::string& haystack, const std::string& needle) {
   if (needle.empty()) {
      return true;
   }
   return ToLower(haystack).find(ToLower(needle)) != std::string::npos;
}

std::string StripBOM(const std::string& line) {
   // The UTF-8 BOM is the three bytes EF BB BF.
   if (line.size() >= 3 &&
       static_cast<unsigned char>(line.at(0)) == 0xEF &&
       static_cast<unsigned char>(line.at(1)) == 0xBB &&
       static_cast<unsigned char>(line.at(2)) == 0xBF) {
      return line.substr(3);
   }
   return line;
}

std::string EscapeDelims(const std::string& text) {
   std::string out;
   for (size_t i = 0; i < text.size(); ++i) {
      char c = text.at(i);
      // The backslash is escaped first so the mapping can be reversed.
      if (c == '\\')      { out += "\\\\"; }
      else if (c == ':') { out += "\\c"; }
      else if (c == ';') { out += "\\s"; }
      else               { out += c; }
   }
   return out;
}

std::string UnescapeDelims(const std::string& text) {
   std::string out;
   size_t i = 0;
   while (i < text.size()) {
      if (text.at(i) == '\\' && i + 1 < text.size()) {
         char next = text.at(i + 1);
         if (next == '\\')      { out += '\\'; i += 2; continue; }
         if (next == 'c')      { out += ':';  i += 2; continue; }
         if (next == 's')      { out += ';';  i += 2; continue; }
      }
      out += text.at(i);
      ++i;
   }
   return out;
}

std::vector<std::string> SplitEscaped(const std::string& text, char delimiter) {
   std::vector<std::string> pieces;
   std::string current;
   size_t i = 0;

   while (i < text.size()) {
      // A backslash always protects the character after it, so a delimiter
      // that was escaped on the way in is not treated as a separator here.
      if (text.at(i) == '\\' && i + 1 < text.size()) {
         current += text.at(i);
         current += text.at(i + 1);
         i += 2;
         continue;
      }
      if (text.at(i) == delimiter) {
         pieces.push_back(current);
         current.clear();
         ++i;
         continue;
      }
      current += text.at(i);
      ++i;
   }

   pieces.push_back(current);
   return pieces;
}

}
