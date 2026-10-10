#ifndef CSVUTIL_H
#define CSVUTIL_H

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// CSVUtil
// ---------------------------------------------------------------------------
// Shared helpers for the bulk import and export feature. Splitting a CSV line
// is not as simple as splitting on commas, because a quoted field may contain
// a comma of its own, so the splitter is written as a small state machine.
// Every class that reads or writes CSV uses these functions, which keeps the
// escaping rules in one place.
// ---------------------------------------------------------------------------
namespace CSVUtil {

   // Splits one CSV record into fields. Handles double-quoted fields and the
   // doubled-quote escape ("" inside a quoted field means one literal quote).
   std::vector<std::string> SplitLine(const std::string& line);

   // Wraps a field in quotes when it contains a comma or a quote, and doubles
   // any quote inside it. A line break is folded to a space first, because a
   // reader that works one line at a time cannot reassemble a field split
   // across physical lines. Returns the field unchanged otherwise.
   std::string Escape(const std::string& field);

   // Strict numeric conversions. Each returns false when the text is empty,
   // holds anything other than a well-formed number, or names a value the
   // target type cannot hold. ToDouble also rejects infinity and not-a-number,
   // since one such value would poison every total it reaches. A malformed
   // import field is therefore reported rather than read as a wrong number.
   bool ToInt(const std::string& text, int& out);
   bool ToDouble(const std::string& text, double& out);

   // Removes leading and trailing whitespace, including carriage returns left
   // behind by files saved on Windows.
   std::string Trim(const std::string& text);

   // Lowercases a copy of the text, used for case-insensitive searching.
   std::string ToLower(const std::string& text);

   // True when needle appears anywhere in haystack, ignoring case.
   bool ContainsIgnoreCase(const std::string& haystack, const std::string& needle);

   // --- sub-field packing --------------------------------------------------
   // Some records pack a list inside a single CSV field: an order packs its
   // lines, and a supplier packs its product IDs. Those packed lists use ';'
   // between items and ':' between the parts of an item, so any of those
   // characters appearing in the data itself would split the field in the
   // wrong place. The two functions below escape and restore them, with the
   // backslash escaped first so the mapping is reversible.
   //
   //    backslash -> \\      colon -> \c      semicolon -> \s

   // Removes a UTF-8 byte order mark from the front of a line. Spreadsheet
   // programs write one at the start of a saved CSV, and without this the
   // first field of the first line carries three invisible bytes that stop a
   // header from being recognised and corrupt an ID that follows it.
   std::string StripBOM(const std::string& line);

   std::string EscapeDelims(const std::string& text);
   std::string UnescapeDelims(const std::string& text);

   // Splits on an unescaped delimiter, leaving escaped ones inside the pieces.
   // The caller passes each piece through UnescapeDelims() to recover it.
   std::vector<std::string> SplitEscaped(const std::string& text, char delimiter);
}

#endif
