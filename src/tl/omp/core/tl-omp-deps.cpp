/*--------------------------------------------------------------------
  (C) Copyright 2006-2013 Barcelona Supercomputing Center
                          Centro Nacional de Supercomputacion
  
  This file is part of Mercurium C/C++ source-to-source compiler.
  
  See AUTHORS file in the top level directory for information
  regarding developers and contributors.
  
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 3 of the License, or (at your option) any later version.
  
  Mercurium C/C++ source-to-source compiler is distributed in the hope
  that it will be useful, but WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the GNU Lesser General Public License for more
  details.
  
  You should have received a copy of the GNU Lesser General Public
  License along with Mercurium C/C++ source-to-source compiler; if
  not, write to the Free Software Foundation, Inc., 675 Mass Ave,
  Cambridge, MA 02139, USA.
--------------------------------------------------------------------*/




#include "tl-omp-core.hpp"
#include "tl-omp-deps.hpp"
#include "cxx-diagnostic.h"
#include "cxx-exprtype.h"
#include "fortran03-exprtype.h"

// Needed for parsing OpenMP standard clauses
#include <sys/types.h>
#include <regex.h>


namespace TL { namespace OpenMP {

    struct DataRefVisitorDep : public Nodecl::ExhaustiveVisitor<void>
    {
        struct ExtraDataSharing : public Nodecl::ExhaustiveVisitor<void>
        {
            DataSharingEnvironment& _data_sharing;
            std::string _clause_name;

            ExtraDataSharing(DataSharingEnvironment& ds, const std::string& clause_name)
                :_data_sharing(ds), _clause_name(clause_name) { }

            void visit(const Nodecl::Symbol& node)
            {
                TL::Symbol sym = node.get_symbol();
                if (!sym.is_valid()
                        || !sym.is_variable()
                        || sym.is_fortran_parameter())
                    return;

                if ((_data_sharing.get_data_sharing(sym, /* check_enclosing */ false) & ~DS_IMPLICIT)
                        == DS_UNDEFINED)
                {
                    std::stringstream reason;
                    reason << "variable is used in the expression of a '" << _clause_name << "' clause";

                    // Mark this as an implicit firstprivate
                    _data_sharing.set_data_sharing(sym,
                           TL::OpenMP::DataSharingAttribute( DS_FIRSTPRIVATE | DS_IMPLICIT),
                           reason.str());

                    // Do not warn saved expressions, it confuses users
                    if (!sym.is_saved_expression())
                    {
                        std::cerr << node.get_locus_str() << ": warning: assuming '"
                            << sym.get_qualified_name() << "' as firstprivate" << std::endl;
                    }
                }
            }

            void visit(const Nodecl::ClassMemberAccess& node)
            {
                walk(node.get_lhs());
                // Do not walk the rhs
            }
        };

        ExtraDataSharing _extra_data_sharing;

        DataRefVisitorDep(DataSharingEnvironment& ds, const std::string& clause_name)
            : _extra_data_sharing(ds, clause_name) { }

        void visit_pre(const Nodecl::Shaping &node)
        {
            _extra_data_sharing.walk(node.get_shape());
        }

        void visit_pre(const Nodecl::ArraySubscript &node)
        {
            _extra_data_sharing.walk(node.get_subscripts());
        }

        void visit_pre(const Nodecl::ClassMemberAccess &node)
        {
            _extra_data_sharing.walk(node.get_lhs());
        }
    };

    void add_extra_data_sharings(Nodecl::NodeclBase data_ref,
            DataSharingEnvironment& ds,
            const std::string& clause_name)
    {
        DataRefVisitorDep data_ref_visitor_dep(ds, clause_name);
        data_ref_visitor_dep.walk(data_ref);
    }

    static void add_data_sharings(ObjectList<Nodecl::NodeclBase> &expression_list,
            DataSharingEnvironment& data_sharing,
            DependencyDirection dep_attr,
            DataSharingAttribute default_data_attr,
            bool in_ompss_mode,
            const std::string &clause_name)
    {
        DataRefVisitorDep data_ref_visitor_dep(data_sharing, clause_name);
        for (ObjectList<Nodecl::NodeclBase>::iterator it = expression_list.begin();
                it != expression_list.end();
                it++)
        {
            DataReference expr(*it);
            if (!expr.is_valid())
            {
                std::cerr << expr.get_error_log();
                std::cerr << expr.get_locus_str()
                    << ": error: skipping invalid dependency expression '" << expr.prettyprint() << "'" << std::endl;
                continue;
            }

            DependencyItem dep_item(*it, dep_attr);

            Symbol sym = expr.get_base_symbol();

            if((default_data_attr & DS_AUTO) == DS_AUTO)
            {
                data_sharing.set_data_sharing(sym, (DataSharingAttribute)(DS_AUTO),
                        "'default(auto)'");
            }
            else if (in_ompss_mode)
            {
                // In OmpSs, the storage of a dependence is always SHARED. Note that with this
                // definition we aren't defining the data-sharings of the variables involved
                // in that expression.
                //
                // About the data-sharings of the variables involved in the dependence expression:
                // - Fortran: the base symbol of the dependence expression is always SHARED
                // - C/C++:
                //  * Trivial dependences must always be SHARED:
                //          int x, a[10];
                //          inout(x) -> shared(x)
                //          inout(a) -> shared(a)
                //  * Arrays and references to arrays must be SHARED too:
                //          int a[10];
                //          inout(a[4])   -> shared(a)
                //          inout(a[1:2]) -> shared(a)
                //  * Otherwise, the data-sharing of the base symbol is FIRSTPRIVATE:
                //          int* p;
                //          inout(*p)     -> firstprivate(p)
                //          inout(p[10])  -> firstprivate(p)
                //          inout(p[1:2]) -> firstprivate(p)
                //          inout([10][20] p) -> firstprivate(p)
                if (IS_FORTRAN_LANGUAGE)
                {
                    data_sharing.set_data_sharing(sym, (DataSharingAttribute)(DS_SHARED | DS_IMPLICIT),
                            "the variable is mentioned in a dependence and it did not have an explicit data-sharing");
                }
                else if (expr.is<Nodecl::Symbol>())
                {
                    data_sharing.set_data_sharing(sym, (DataSharingAttribute)(DS_SHARED | DS_IMPLICIT),
                            "the variable is mentioned in a dependence and it did not have an explicit data-sharing");
                }
                else if (sym.get_type().is_array()
                        || (sym.get_type().is_any_reference()
                            && sym.get_type().references_to().is_array()))
                {
                    data_sharing.set_data_sharing(sym, (DataSharingAttribute)(DS_SHARED | DS_IMPLICIT),
                            "the variable is an array mentioned in a non-trivial dependence "
                            "and it did not have an explicit data-sharing");
                }
                else
                {
                    data_sharing.set_data_sharing(sym, (DataSharingAttribute)(DS_FIRSTPRIVATE | DS_IMPLICIT),
                            "the variable is a non-array mentioned in a non-trivial dependence "
                            "and it did not have an explicit data-sharing");
                }
            }

            data_sharing.add_dependence(dep_item);

            data_ref_visitor_dep.walk(expr);
        }
    }

    void Core::get_dependences_info(TL::PragmaCustomLine construct, DataSharingEnvironment& data_sharing, 
        DataSharingAttribute default_data_attr)
    {
        PragmaCustomClause input_clause = construct.get_clause("in",
                /* deprecated */ "input");
        get_dependences_info_clause(input_clause, data_sharing, DEP_DIR_IN, default_data_attr, "in");

        TL::ObjectList<std::string> input_private_names;
        PragmaCustomClause input_private_clause = construct.get_clause("inprivate");
        get_dependences_info_clause(input_private_clause, data_sharing, DEP_DIR_IN_PRIVATE, default_data_attr, "inprivate");

        PragmaCustomClause output_clause = construct.get_clause("out",
                /* deprecated */ "output");
        get_dependences_info_clause(output_clause, data_sharing, DEP_DIR_OUT, default_data_attr, "out");
        
        PragmaCustomClause inout_clause = construct.get_clause("inout");
        get_dependences_info_clause(inout_clause, data_sharing, DEP_DIR_INOUT, default_data_attr, "inout");

        PragmaCustomClause concurrent_clause = construct.get_clause("concurrent");
        get_dependences_info_clause(concurrent_clause, data_sharing,
                DEP_CONCURRENT, default_data_attr, "concurrent");

        PragmaCustomClause commutative_clause = construct.get_clause("commutative");
        get_dependences_info_clause(commutative_clause, data_sharing,
                DEP_COMMUTATIVE, default_data_attr, "commutative");

        // OpenMP standard proposal
        PragmaCustomClause depends = construct.get_clause("depend");
        get_dependences_info_std_clause(construct, depends, data_sharing, default_data_attr);
    }

    static decl_context_t decl_context_map_id(decl_context_t d)
    {
        return d;
    }

    void Core::parse_dependences_info_std_clause(
            TL::ReferenceScope parsing_scope,
            TL::PragmaCustomClause clause,
            TL::ObjectList<Nodecl::NodeclBase> &in,
            TL::ObjectList<Nodecl::NodeclBase> &out,
            TL::ObjectList<Nodecl::NodeclBase> &inout,
            const locus_t* locus
            )
    {
        if (!clause.is_defined())
            return;

        ObjectList<std::string> arguments = clause.get_tokenized_arguments();

        // Since we coalesce all the arguments of a clauses with the same name
        // in a case like depend(in : a, b) depend(out : c, d) will be a list
        // containing "in:a", "b", "out:c", "d"

        int cflags = REG_EXTENDED;
        if (IS_FORTRAN_LANGUAGE)
        {
            cflags |= REG_ICASE;
        }

        regex_t preg;
        if (regcomp(&preg, "^[[:blank:]]*((in)|(out)|(inout))[[:blank:]]*:(.*)$", cflags) != 0)
        {
            internal_error("Invalid regular expression", 0);
        }
        const int num_matches = 6;
        regmatch_t pmatch[num_matches] = { };

        TL::ObjectList<Nodecl::NodeclBase> *dep_set = NULL;
        for (ObjectList<std::string>::iterator it = arguments.begin();
                it != arguments.end();
                it++)
        {
            std::string clause_name;
            int match = regexec(&preg, it->c_str(), num_matches, pmatch, 0);

            std::string current_dep_expr = *it;

            if (match == 0)
            {
                // Zero-th match is the whole regular expression
                ERROR_CONDITION(pmatch[1].rm_so == -1, "Invalid match", 0);
                std::string dependency_type;
                for (int i = pmatch[1].rm_so; i < pmatch[1].rm_eo; i++)
                {
                    dependency_type += tolower((*it)[i]);
                }

                if (dependency_type == "in")
                {
                    dep_set = &in;
                }
                else if (dependency_type == "out")
                {
                    dep_set = &out;
                }
                else if (dependency_type == "inout")
                {
                    dep_set = &inout;
                }
                else
                {
                    internal_error("Code unreachable", 0);
                }

                // Now compute the proper dependence expression
                current_dep_expr.clear();
                ERROR_CONDITION(pmatch[5].rm_so == -1, "Invalid match", 0);
                for (int i = pmatch[5].rm_so; i < pmatch[5].rm_eo; i++)
                {
                    current_dep_expr += (*it)[i];
                }
            }
            else if (match == REG_NOMATCH)
            {
                if (dep_set == NULL)
                {
                    error_printf("%s: error: skipping item '%s' in 'depend' clause because it lacks dependence-type\n",
                            locus_to_str(locus),
                            current_dep_expr.c_str());
                    continue;
                }
            }
            else
            {
                internal_error("Unexpected result %d from regexec\n", match);
            }

            Source src;
            src << current_dep_expr;

            // Now, parse a single OpenMP list item and hand it to the usual dependency routines
            Nodecl::NodeclBase expr;
            if (IS_C_LANGUAGE || IS_CXX_LANGUAGE)
            {
                expr = src.parse_generic(parsing_scope,
                        /* ParseFlags */ Source::DEFAULT,
                        "@OMP-DEPEND-ITEM@",
                        Source::c_cxx_check_expression_adapter,
                        decl_context_map_id);
            }
            else if (IS_FORTRAN_LANGUAGE)
            {
                expr = src.parse_generic(parsing_scope,
                        /* ParseFlags */ Source::DEFAULT,
                        "@OMP-DEPEND-ITEM@",
                        Source::fortran_check_expression_adapter,
                        decl_context_map_id);
            }

            // Singleton
            dep_set->append(expr);
        }

        regfree(&preg);
    }

    void Core::get_dependences_info_std_clause(
            TL::PragmaCustomLine construct,
            TL::PragmaCustomClause clause,
            DataSharingEnvironment& data_sharing, 
            DataSharingAttribute default_data_attr)
    {
            TL::ObjectList<Nodecl::NodeclBase> in, out, inout;
            parse_dependences_info_std_clause(
                    construct,
                    clause,
                    in,
                    out,
                    inout,
                    construct.get_locus());

            add_data_sharings(in, data_sharing,
                    DEP_DIR_IN, default_data_attr, this->in_ompss_mode(), "depend(in:)");
            add_data_sharings(out, data_sharing,
                    DEP_DIR_OUT, default_data_attr, this->in_ompss_mode(), "depend(out:)");
            add_data_sharings(inout, data_sharing,
                    DEP_DIR_INOUT, default_data_attr, this->in_ompss_mode(), "depend(inout:)");
    }

    void Core::get_dependences_info_clause(PragmaCustomClause clause,
           DataSharingEnvironment& data_sharing,
           DependencyDirection dep_attr, 
           DataSharingAttribute default_data_attr,
           const std::string& clause_name)
    {
        if (clause.is_defined())
        {
            ObjectList<Nodecl::NodeclBase> expr_list = clause.get_arguments_as_expressions();
            add_data_sharings(expr_list, data_sharing,
                    dep_attr, default_data_attr, this->in_ompss_mode(), clause_name);
        }
    }

    std::string get_dependency_direction_name(DependencyDirection d)
    {
        switch (d)
        {
            case DEP_DIR_UNDEFINED:
                return "<<undefined-dependence>>";
            case DEP_DIR_IN:
            case DEP_DIR_IN_VALUE:
                return "in";
            case DEP_DIR_OUT:
                return "out";
            case DEP_DIR_INOUT:
                return "inout";
            case DEP_CONCURRENT:
                return "concurrent";
            case DEP_COMMUTATIVE:
                return "commutative";
            default:
                return "<<unknown-dependence-kind?>>";
        }
    }
} }
