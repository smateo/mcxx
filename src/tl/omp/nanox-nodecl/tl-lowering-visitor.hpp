#include "tl-nodecl-visitor.hpp"
#include "tl-outline-info.hpp"

namespace TL { namespace Nanox {

class LoweringVisitor : public Nodecl::ExhaustiveVisitor<void>
{
    public:
        LoweringVisitor();
        void visit(const Nodecl::Parallel::Async& construct);
        void visit(const Nodecl::Parallel::WaitAsyncsShallow& construct);

    private:
        std::string declare_argument_structure(OutlineInfo& outline_info, Nodecl::NodeclBase construct);
        bool type_needs_vla_handling(TL::Type t);
        bool requires_vla_handling(OutlineDataItem& outline_data_item);

        void emit_outline(OutlineInfo& outline_info,
                Nodecl::NodeclBase body,
                const std::string& outline_name,
                const std::string& structure_name);

        TL::Type handle_vla_type_rec(
                OutlineDataItem& outline_data_item,
                TL::Type type, 
                TL::Scope class_scope, 
                TL::Symbol new_class_symbol,
                TL::Type new_class_type,
                TL::ObjectList<TL::Symbol>& new_symbols,
                const std::string& filename, 
                int line);
        void handle_vla_type(
                OutlineDataItem& outline_data_item,
                TL::Scope class_scope, 
                TL::Symbol new_class_symbol,
                TL::Type new_class_type,
                TL::ObjectList<TL::Symbol>& new_symbols,
                const std::string& filename, 
                int line);
};

} }
