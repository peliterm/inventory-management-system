#include "User.h"
#include "CSVUtil.h"

#include <iomanip>
#include <sstream>

User::User() {
   role = Role::CLERK;
}

User::User(const std::string& user, const std::string& name,
           const std::string& plainPassword, Role userRole) {
   username = user;
   fullName = name;
   passwordHash = HashPassword(user, plainPassword);
   role = userRole;
}

std::string User::HashPassword(const std::string& user, const std::string& password) {
   // The username acts as the salt. See the security note in the header.
   //
   // The digest is computed here with FNV-1a rather than by calling
   // std::hash, because std::hash is implementation-defined: the same string
   // produces a different value under g++ than it does under MSVC. An
   // accounts file written by one compiler would then reject every login
   // under the other. This loop produces identical digests on any conforming
   // compiler, which is what makes data/users.csv portable.
   const std::string salted = user + "::" + password;

   unsigned long long digest = 14695981039346656037ULL;   // FNV-1a 64-bit offset basis
   for (size_t i = 0; i < salted.size(); ++i) {
      digest ^= static_cast<unsigned long long>(
                   static_cast<unsigned char>(salted.at(i)));
      digest *= 1099511628211ULL;                         // FNV-1a 64-bit prime
   }

   std::ostringstream out;
   out << std::hex << std::setw(16) << std::setfill('0') << digest;
   return out.str();
}

std::string User::GetUsername() const { return username; }
std::string User::GetFullName() const { return fullName; }
Role User::GetRole() const { return role; }
std::string User::GetPasswordHash() const { return passwordHash; }

void User::SetFullName(const std::string& name) { fullName = name; }
void User::SetRole(Role userRole) { role = userRole; }

void User::SetPassword(const std::string& plainPassword) {
   passwordHash = HashPassword(username, plainPassword);
}

void User::SetPasswordHash(const std::string& hash) {
   passwordHash = hash;
}

bool User::CheckPassword(const std::string& plainPassword) const {
   return passwordHash == HashPassword(username, plainPassword);
}

bool User::HasPermission(Permission permission) const {
   return RoleHasPermission(role, permission);
}

bool User::RoleHasPermission(Role role, Permission permission) {
   switch (role) {
      case Role::ADMIN:
         // An administrator holds every capability.
         return true;

      case Role::MANAGER:
         // A manager runs the warehouse but cannot create or delete accounts.
         return permission != Permission::MANAGE_USERS;

      case Role::CLERK:
         // A stock clerk works the floor: adjust counts, move orders along,
         // and read reports. No structural changes to the catalogue.
         return permission == Permission::UPDATE_STOCK ||
                permission == Permission::MANAGE_ORDERS ||
                permission == Permission::VIEW_REPORTS;
   }
   return false;
}

std::string User::GetDetails() const {
   std::ostringstream out;
   out << std::left << std::setw(16) << username
       << std::setw(26) << fullName
       << std::setw(10) << RoleToString(role);
   return out.str();
}

std::string User::RoleToString(Role role) {
   switch (role) {
      case Role::ADMIN:   return "ADMIN";
      case Role::MANAGER: return "MANAGER";
      case Role::CLERK:   return "CLERK";
   }
   return "UNKNOWN";
}

bool User::RoleFromString(const std::string& text, Role& out) {
   std::string upper = CSVUtil::Trim(text);
   for (size_t i = 0; i < upper.size(); ++i) {
      upper.at(i) = static_cast<char>(std::toupper(static_cast<unsigned char>(upper.at(i))));
   }

   if (upper == "ADMIN")   { out = Role::ADMIN;   return true; }
   if (upper == "MANAGER") { out = Role::MANAGER; return true; }
   if (upper == "CLERK")   { out = Role::CLERK;   return true; }
   return false;
}

std::string User::ToCSV() const {
   std::ostringstream out;
   out << CSVUtil::Escape(username) << ','
       << CSVUtil::Escape(fullName) << ','
       << RoleToString(role) << ','
       << passwordHash;
   return out.str();
}

bool User::ParseCSV(const std::string& line, User& out) {
   std::vector<std::string> fields = CSVUtil::SplitLine(line);
   if (fields.size() != 4) {
      return false;
   }

   Role parsed = Role::CLERK;
   if (!RoleFromString(fields.at(2), parsed)) {
      return false;
   }

   User built;
   built.username = fields.at(0);
   built.fullName = fields.at(1);
   built.role = parsed;
   built.passwordHash = fields.at(3);   // already hashed on disk

   out = built;
   return true;
}

std::string User::CSVHeader() {
   return "username,fullName,role,passwordHash";
}
