/** @file Mainly existing to disentangle implementation details from circular and cross dependencies
 *  (e.g. column_t -> default_value_extractor -> serializer_context -> storage_impl -> table_t -> column_t)
 *  this file is also used to provide definitions of interface methods 'hitting the database'.
 */
#pragma once
#include <sqlite3.h>
#include <algorithm>  //  std::find_if
#include <sstream>  //  std::stringstream
#include <cstdlib>  //  std::atoi

#include "../error_code.h"
#include "../storage_impl.h"
#include "../util.h"

namespace sqlite_orm {
    namespace internal {

        inline bool storage_impl_base::table_exists(const std::string& tableName, sqlite3* db) const {
            using namespace std::string_literals;

            bool result = false;
            std::stringstream ss;
            ss << "SELECT COUNT(*) FROM sqlite_master WHERE type = " << quote_string_literal("table"s)
               << " AND name = " << quote_string_literal(tableName);
            auto query = ss.str();
            auto rc = sqlite3_exec(
                db,
                query.c_str(),
                [](void* data, int argc, char** argv, char** /*azColName*/) -> int {
                    auto& res = *(bool*)data;
                    if(argc) {
                        res = !!std::atoi(argv[0]);
                    }
                    return 0;
                },
                &result,
                nullptr);
            if(rc != SQLITE_OK) {
                throw_translated_sqlite_error(db);
            }
            return result;
        }

        inline void
        storage_impl_base::rename_table(sqlite3* db, const std::string& oldName, const std::string& newName) const {
            std::stringstream ss;
            ss << "ALTER TABLE " << quote_identifier(oldName) << " RENAME TO " << quote_identifier(newName);
            perform_void_exec(db, ss.str());
        }

        inline bool storage_impl_base::calculate_remove_add_columns(std::vector<table_info*>& columnsToAdd,
                                                                    std::vector<table_info>& storageTableInfo,
                                                                    std::vector<table_info>& dbTableInfo) {
            bool notEqual = false;

            //  iterate through storage columns
            for(size_t storageColumnInfoIndex = 0; storageColumnInfoIndex < storageTableInfo.size();
                ++storageColumnInfoIndex) {

                //  get storage's column info
                auto& storageColumnInfo = storageTableInfo[storageColumnInfoIndex];
                auto& columnName = storageColumnInfo.name;

                //  search for a column in db eith the same name
                auto dbColumnInfoIt = std::find_if(dbTableInfo.begin(), dbTableInfo.end(), [&columnName](auto& ti) {
                    return ti.name == columnName;
                });
                if(dbColumnInfoIt != dbTableInfo.end()) {
                    auto& dbColumnInfo = *dbColumnInfoIt;
                    auto columnsAreEqual =
                        dbColumnInfo.name == storageColumnInfo.name &&
                        dbColumnInfo.notnull == storageColumnInfo.notnull &&
                        (!dbColumnInfo.dflt_value.empty()) == (!storageColumnInfo.dflt_value.empty()) &&
                        dbColumnInfo.pk == storageColumnInfo.pk;
                    if(!columnsAreEqual) {
                        notEqual = true;
                        break;
                    }
                    dbTableInfo.erase(dbColumnInfoIt);
                    storageTableInfo.erase(storageTableInfo.begin() + static_cast<ptrdiff_t>(storageColumnInfoIndex));
                    --storageColumnInfoIndex;
                } else {
                    columnsToAdd.push_back(&storageColumnInfo);
                }
            }
            return notEqual;
        }

        template<class H, class... Ts>
        void storage_impl<H, Ts...>::copy_table(sqlite3* db,
                                                const std::string& tableName,
                                                const std::vector<table_info*>& columnsToIgnore) const {
            std::stringstream ss;
            std::vector<std::string> columnNames;
            this->table.for_each_column([&columnNames, &columnsToIgnore](auto& c) {
                auto& columnName = c.name;
                auto columnToIgnoreIt =
                    std::find_if(columnsToIgnore.begin(), columnsToIgnore.end(), [&columnName](auto tableInfoPointer) {
                        return columnName == tableInfoPointer->name;
                    });
                if(columnToIgnoreIt == columnsToIgnore.end()) {
                    columnNames.emplace_back(columnName);
                }
            });
            auto columnNamesCount = columnNames.size();
            ss << "INSERT INTO " << quote_identifier(tableName) << " (";
            for(size_t i = 0; i < columnNamesCount; ++i) {
                ss << columnNames[i];
                if(i < columnNamesCount - 1) {
                    ss << ",";
                }
                ss << " ";
            }
            ss << ") ";
            ss << "SELECT ";
            for(size_t i = 0; i < columnNamesCount; ++i) {
                ss << columnNames[i];
                if(i < columnNamesCount - 1) {
                    ss << ", ";
                }
            }
            ss << " FROM " << quote_identifier(this->table.name);
            perform_void_exec(db, ss.str());
        }

    }
}