#pragma once

#include <type_traits>  //  std::true_type, std::false_type, std::remove_const, std::enable_if, std::is_base_of
#include <tuple>
#include <utility>  //  std::index_sequence

#include "functional/cxx_universal.h"
#include "functional/cxx_type_traits_polyfill.h"
#include "type_traits.h"

namespace sqlite_orm {
    namespace internal {

        template<class... DBO>
        struct storage_t;

        template<class... DBO>
        using db_objects_tuple = std::tuple<DBO...>;

        struct basic_table;
        struct index_base;
        struct base_trigger;

        template<class T>
        struct is_storage : std::false_type {};

        template<class... DBO>
        struct is_storage<storage_t<DBO...>> : std::true_type {};
        template<class... DBO>
        struct is_storage<const storage_t<DBO...>> : std::true_type {};

        template<class T>
        struct is_db_objects : std::false_type {};

        template<class... DBO>
        struct is_db_objects<db_objects_tuple<DBO...>> : std::true_type {};
        template<class... DBO>
        struct is_db_objects<const db_objects_tuple<DBO...>> : std::true_type {};

        /**
         *  A data type's label type, void otherwise.
         */
        template<typename O>
        using label_of_or_void_t = polyfill::detected_or_t<void, cte_label_type_t, O>;

        /**
         *  A data type's CTE label, otherwise T itself is used as a CTE label.
         *
         *  Note: This is useful if the cte object type `column_reuslts` gets ever looked up,
         *  and we want to ensure that the lookup happens by label instead.
         */
        template<typename T>
        using cte_label_or_nested_t = polyfill::detected_or_t<T, cte_label_type_t, T>;

        /**
         *  std::true_type if given 'table' type matches, std::false_type otherwise.
         *
         *  A 'table' type is one of: table_t<>, index_t<> [, subselect_mapper<>]
         */
        template<typename DBO, typename T>
        using dbo_type_matches = polyfill::conjunction<std::is_same<T, DBO>,
                                                       polyfill::disjunction<std::is_base_of<basic_table, T>,
                                                                             std::is_base_of<index_base, T>,
                                                                             std::is_base_of<base_trigger, T>>>;

        /**
         *  std::true_type if given object is mapped, std::false_type otherwise.
         * 
         *  Note: unlike table_t<>, index_t<>::object_type and trigger_t<>::object_type is always void.
         */
        template<typename DBO, typename Lookup>
        struct object_type_matches : polyfill::conjunction<polyfill::negation<std::is_void<object_type_t<DBO>>>,
                                                           std::is_same<Lookup, object_type_t<DBO>>> {};

        /**
         *  std::true_type if given label is mapped, std::false_type otherwise
         *
         *  Note: unlike table_t<>, index_t<> doesn't have a nested index_t::cte_label_type typename,
         *  that's why we use storage_label_of_t<> for a fallback to void.
         */
        template<typename DBO, typename Label>
        using cte_label_type_matches =
            polyfill::conjunction<polyfill::negation<std::is_void<label_of_or_void_t<DBO>>>,
                                  std::is_same<cte_label_or_nested_t<Label>, label_of_or_void_t<DBO>>>;

        /**
         *  std::true_type if given lookup type (object or label) is mapped, std::false_type otherwise.
         */
        template<typename DBO, typename Lookup>
        using lookup_type_matches = typename polyfill::disjunction<dbo_type_matches<DBO, Lookup>,
                                                                   object_type_matches<DBO, Lookup>,
                                                                   cte_label_type_matches<DBO, Lookup>>::type;
    }

    // pick/lookup metafunctions
    namespace internal {

        /**
         *   Indirect enabler for DBO, accepting an index to disambiguate non-unique DBOs
         */
        template<class Lookup, size_t Ix, class DBO>
        struct enable_found_table : std::enable_if<lookup_type_matches<DBO, Lookup>::value, DBO> {};

        /**
         *  SFINAE friendly facility to pick a table definition (`table_t`) from a tuple of database objects.
         *  
         *  Lookup - mapped data type
         *  Seq - index sequence matching the number of DBOs
         *  DBOs - db_objects_tuple type
         */
        template<class Lookup, class Seq, class DBOs>
        struct storage_pick_table;

        template<class Lookup, size_t... Ix, class... DBO>
        struct storage_pick_table<Lookup, std::index_sequence<Ix...>, db_objects_tuple<DBO...>>
            : enable_found_table<Lookup, Ix, DBO>... {};

        /**
         *  SFINAE friendly facility to pick a table definition (`table_t`) from a tuple of database objects.
         *
         *  Lookup - 'table' type, mapped data type
         *  DBOs - db_objects_tuple type, possibly const-qualified
         */
        template<class Lookup, class DBOs>
        using storage_pick_table_t = typename storage_pick_table<Lookup,
                                                                 std::make_index_sequence<std::tuple_size<DBOs>::value>,
                                                                 std::remove_const_t<DBOs>>::type;

        /**
         *  Find a table definition (`table_t`) from a tuple of database objects;
         *  `std::nonesuch` if not found.
         *
         *  DBOs - db_objects_tuple type
         *  Lookup - mapped data type
         */
        template<class Lookup, class DBOs>
        struct storage_find_table : polyfill::detected_or<polyfill::nonesuch, storage_pick_table_t, Lookup, DBOs> {};

        /**
         *  Find a table definition (`table_t`) from a tuple of database objects;
         *  `std::nonesuch` if not found.
         *
         *  DBOs - db_objects_tuple type, possibly const-qualified
         *  Lookup - mapped data type
         */
        template<class Lookup, class DBOs>
        using storage_find_table_t = typename storage_find_table<Lookup, std::remove_const_t<DBOs>>::type;

#ifndef SQLITE_ORM_BROKEN_VARIADIC_PACK_EXPANSION
        template<class DBOs, class Lookup, class SFINAE = void>
        struct is_mapped : std::false_type {};
        template<class DBOs, class Lookup>
        struct is_mapped<DBOs, Lookup, polyfill::void_t<storage_pick_table_t<Lookup, DBOs>>> : std::true_type {};
#else
        template<class DBOs, class Lookup, class SFINAE = storage_find_table_t<Lookup, DBOs>>
        struct is_mapped : std::true_type {};
        template<class DBOs, class Lookup>
        struct is_mapped<DBOs, Lookup, polyfill::nonesuch> : std::false_type {};
#endif

        template<class DBOs, class Lookup>
        SQLITE_ORM_INLINE_VAR constexpr bool is_mapped_v = is_mapped<DBOs, Lookup>::value;
    }
}

// runtime lookup functions
namespace sqlite_orm {
    namespace internal {
        /**
         *  Pick the table definition for the specified lookup type from the given tuple of schema objects.
         * 
         *  Note: This function requires Lookup to be mapped, otherwise it is removed from the overload resolution set.
         */
        template<class Lookup, class DBOs, satisfies<is_db_objects, DBOs> = true>
        auto& pick_table(DBOs& dbObjects) {
            using table_type = storage_pick_table_t<Lookup, DBOs>;
            return std::get<table_type>(dbObjects);
        }
    }
}
