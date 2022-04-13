#pragma once

#include <type_traits>
#include <tuple>

#include "cxx_polyfill.h"
#include "start_macros.h"
#include "type_traits.h"

namespace sqlite_orm {

    namespace internal {

        /**
         *  Aliased column expression mapped into a CTE, stored as a field in a table column.
         */
        template<class A, class F>
        struct aliased_field {
            ~aliased_field() = delete;
            aliased_field(const aliased_field&) = delete;
            void operator=(const aliased_field&) = delete;

            F field;
        };

        /**
         *  This class captures various properties and aspects of a subselect's column expression,
         *  and is used as a proxy in table_t<>.
         */
        template<typename Label,
                 typename ExplicitColRefs,
                 typename Expression,
                 typename SubselectColRefs,
                 typename FinalColRefs,
                 typename... Fs>
        class subselect_mapper {
          public:
            subselect_mapper() = delete;

            using index_sequence = std::index_sequence_for<Fs...>;
            // this type name is used to detect the mapping from label to object
            using cte_label_type = Label;
            using fields_type = fields_t<Fs...>;
            // this type name is used to detect the mapping from label to object;
            // it only exists to satisfy a table_t<>'s requirement to have a object_type typename
            using cte_object_type = fields_t<Fs...>;
            // this type captures the expressions forming the columns in a subselect;
            // it is currently unused, however proves to be useful in compilation errors,
            // as it simplifies recognizing errors in column expressions
            using expressions_tuple = tuplify_t<Expression>;
            // this type captures column reference expressions specified at CTE construction;
            // those are: member pointers, alias holders
            using explicit_colrefs_tuple = ExplicitColRefs;
            // this type captures column reference expressions from the subselect;
            // those are: member pointers, alias holders
            using subselect_colrefs_tuple = SubselectColRefs;
            // this type captures column reference expressions merged from SubselectColRefs and ExplicitColRefs
            using final_colrefs_tuple = FinalColRefs;
        };
    }
}
