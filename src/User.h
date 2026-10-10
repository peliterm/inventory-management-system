#ifndef USER_H
#define USER_H

#include <string>

// The three staff roles the system recognises, ordered from most to least
// privileged.
enum class Role {
   ADMIN,
   MANAGER,
   CLERK
};

// The distinct capabilities a role may hold. Checking a named permission
// rather than comparing roles directly means a new role can be added by
// editing one table instead of hunting through the menu code.
enum class Permission {
   MANAGE_USERS,       // create and remove accounts
   MANAGE_PRODUCTS,    // add, edit, and remove product records
   UPDATE_STOCK,       // adjust quantities on existing products
   MANAGE_SUPPLIERS,   // add, edit, and remove supplier records
   MANAGE_ORDERS,      // place orders and advance their status
   VIEW_REPORTS,       // run the reporting and analytics screens
   IMPORT_EXPORT       // bulk load and save CSV files
};

// ---------------------------------------------------------------------------
// User
// ---------------------------------------------------------------------------
// A staff account. Passwords are never stored in the clear; the record keeps a
// salted hash and compares hashes at login.
//
// SECURITY NOTE: the digest below is FNV-1a, a fast general-purpose hash, not
// a password hash. It is appropriate for a course project because it
// demonstrates the pattern of storing a digest rather than a password, but a
// real system would use a deliberately slow algorithm such as bcrypt or
// Argon2 with a per-user random salt. FNV-1a is written out by hand rather
// than calling std::hash so that the stored digests mean the same thing under
// every compiler.
// ---------------------------------------------------------------------------
class User {
private:
   std::string username;
   std::string fullName;
   std::string passwordHash;
   Role role;

   // Combines the username with the password before hashing, so two users who
   // pick the same password do not end up with the same stored digest.
   static std::string HashPassword(const std::string& user, const std::string& password);

public:
   // Creates an empty clerk account.
   User();

   // Creates an account, hashing the supplied plain-text password immediately.
   User(const std::string& user, const std::string& name,
        const std::string& plainPassword, Role userRole);

   std::string GetUsername() const;
   std::string GetFullName() const;
   Role GetRole() const;
   std::string GetPasswordHash() const;

   void SetFullName(const std::string& name);
   void SetRole(Role userRole);

   // Replaces the stored digest with the hash of a new password.
   void SetPassword(const std::string& plainPassword);

   // Installs an already-hashed value, used when loading accounts from CSV.
   void SetPasswordHash(const std::string& hash);

   // True when the supplied password hashes to the stored digest.
   bool CheckPassword(const std::string& plainPassword) const;

   // True when this user's role grants the named capability.
   bool HasPermission(Permission permission) const;

   // Single-line summary used by the account-management screen.
   std::string GetDetails() const;

   static std::string RoleToString(Role role);
   static bool RoleFromString(const std::string& text, Role& out);

   // True when the role grants the capability. Exposed separately so the rule
   // table can be tested without constructing a user.
   static bool RoleHasPermission(Role role, Permission permission);

   std::string ToCSV() const;
   static bool ParseCSV(const std::string& line, User& out);
   static std::string CSVHeader();
};

#endif
