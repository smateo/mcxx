/*--------------------------------------------------------------------
  (C) Copyright 2006-2012 Barcelona Supercomputing Center
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

#ifndef TL_IV_ANALYSIS_HPP
#define TL_IV_ANALYSIS_HPP

#include "tl-extensible-graph.hpp"
#include "tl-nodecl.hpp"
#include "tl-nodecl-utils.hpp"
#include "tl-nodecl-visitor.hpp"
#include "tl-symbol.hpp"

namespace TL {
namespace Analysis {
    
    class Node;
    
    // ********************************************************************************************* //
    // ************************* Class representing and induction variable ************************* //
    // FIXME Clean this class!!! Represents the same as 'InductionVariableData'
    struct InductionVarInfo {
        Symbol _s;
        Nodecl::NodeclBase _lb;
        Nodecl::NodeclBase _ub;     // value included in the range
        Nodecl::NodeclBase _stride;
        bool _stride_is_one;
        int _stride_is_positive;
        
        InductionVarInfo(Symbol s, Nodecl::NodeclBase lb);
        
        // *** Getters and setters *** //
        Symbol get_symbol() const;
        Type get_type() const;
        Nodecl::NodeclBase get_lb() const;
        void set_lb(Nodecl::NodeclBase lb);
        Nodecl::NodeclBase get_ub() const;
        void set_ub(Nodecl::NodeclBase ub);
        Nodecl::NodeclBase get_stride() const;
        void set_stride(Nodecl::NodeclBase stride);
        /*!\return 0 if negative, 1 if positive, 2 if we cannot compute it
         */        
        int stride_is_positive() const;
        void set_stride_is_positive(int stride_is_positive);
        
        bool operator==(const InductionVarInfo &v) const;
        bool operator<(const InductionVarInfo &v) const;
    };

    struct Node_hash {
        size_t operator() (const int& n) const;
    };
    
    struct Node_comp {
        bool operator() (const int& n1, const int& n2) const;
    };
    
    typedef std::tr1::unordered_multimap<int, InductionVarInfo*, Node_hash, Node_comp> induc_vars_map;
    
    // *********************** END class representing and induction variable *********************** //
    // ********************************************************************************************* //
    
    
    
    // ********************************************************************************************* //
    // ************************* Class representing and induction variable ************************* //
    
    enum InductionVarType {
        basic_iv,
        derived_iv
    };
    
    class LIBTL_CLASS InductionVariableData {
    private:
        Nodecl::NodeclBase _lb;         /*!< Lower bound within a loop >*/
        Nodecl::NodeclBase _ub;         /*!< Upper bound within a loop (included) >*/
        Nodecl::NodeclBase _stride;     /*!< Stride within a loop >*/
        InductionVarType _type;         /*!< Type of iv: '1' = basic, '2' = derived >*/
        Nodecl::NodeclBase _family;     /*!< Family of the IV. For basic IVs, the family is the IV itself >*/
            
    public: 
        InductionVariableData( InductionVarType type, Nodecl::NodeclBase family );
        
        bool is_basic( );
    };

    // This type definition is redefined in tl-node.hpp
    typedef std::tr1::unordered_map<Nodecl::NodeclBase, InductionVariableData, Nodecl::Utils::Nodecl_hash, 
                                    Nodecl::Utils::Nodecl_comp> IV_map;
 
    // *********************** END class representing and induction variable *********************** //
    // ********************************************************************************************* //
    

    
    // ********************************************************************************************* //
    // ************************** Class for induction variables analysis *************************** //
    
    class LIBTL_CLASS InductionVariableAnalysis : public Nodecl::ExhaustiveVisitor<bool> {
    private:
        
        induc_vars_map _induction_vars;             // DEPRECATED
        
        void detect_basic_induction_variables(Node* node, Node* loop);
        void detect_derived_induction_variables( Node* node, Node* loop );
        
        bool is_there_unique_definition_in_loop( Nodecl::NodeclBase iv_st, Node* iv_node, Node* loop );
        bool is_there_definition_in_loop_(Nodecl::NodeclBase iv_st, Node* iv_node, Node* current, Node* loop );
        
        //! This method returns true when \iv is defined more than once in the loop
        bool is_false_induction_variable( Nodecl::NodeclBase iv, Nodecl::NodeclBase stmt, Node* node, int id_end );
        
        //! This method is overloaded to deal with graph visits
        bool is_false_induction_variable_( Nodecl::NodeclBase iv, Nodecl::NodeclBase stmt, Node* node, int id_end );
        
        bool only_definition_is_in_loop(Nodecl::NodeclBase family, Nodecl::NodeclBase iv_st, Node* iv_node, Node* loop);
        bool only_definition_is_in_loop( Nodecl::NodeclBase iv_st, Node* iv_node, Node* loop );
        
        /*!
         * Deletes those induction variables included in the list during a previous traverse through the loop control 
         * that are redefined within the loop
         * \param node Node in the graph we are analysing
         * \param loop_node Outer loop node where is contained the node we are checking
         */
        void delete_false_induction_vars(Node* node, Node* loop_node);
        
        bool is_loop_invariant_(Node* node, int id_end);
        
        
        // Private members used in modified symbol visitor //
        
        Nodecl::NodeclBase _constant;           /*!< Nodecl to be checked of being constant */
        bool _defining;                         /*!< Boolean used during the visit indicating whether we are in a defining situation */
        
        // * Private methods for modified symbols visitor //
        
        //! Visiting method for any kind of assignment
        bool visit_assignment(Nodecl::NodeclBase lhs, Nodecl::NodeclBase rhs);
        //! Visiting method for any kind of function call
        bool visit_function(Symbol func_sym, ObjectList<Type> param_types, Nodecl::List arguments);
        
        //! Specialization of join_list Visitor method for lists of booleans
        virtual bool join_list(TL::ObjectList<bool>& list);
        
        
    public: 
        
        // **** Constructor **** //
        
        InductionVariableAnalysis();
        
        // **** Induction Variables analysis methods **** //
        
        void induction_variable_detection( Node* current );
        
        Nodecl::NodeclBase is_basic_induction_variable( Nodecl::NodeclBase st, Node* loop );
        
        Nodecl::NodeclBase is_derived_induction_variable(Nodecl::NodeclBase st, Node* current, 
                                                         Node* loop, Nodecl::NodeclBase& family);
        
        bool is_loop_invariant(Node* node, int id_end);
        
        InductionVarInfo* induction_vars_l_contains_symbol(Node*, Symbol s) const;
        
        std::map<Symbol, Nodecl::NodeclBase> get_induction_vars_mapping(Node* loop_node) const;
        
        std::map<Symbol, int> get_induction_vars_direction(Node* loop_node) const;
        
        
        // ******************* Utils ******************* //
        
        void print_induction_variables( Node* node );
        
        
        // ********** Modified Symbol Visitor ********** //
        
        /*!This part of the LoopAnalysis class implements a Visitor that checks whether a symbol is modified in a given Nodecl
         * This is used during the induction variable analysis
         * The Visitor returns true in case the symbol is modified, and false otherwise
         */
        Ret visit(const Nodecl::AddAssignment& n);
        Ret visit(const Nodecl::ArithmeticShrAssignment& n);
        Ret visit(const Nodecl::ArraySubscript& n);
        Ret visit(const Nodecl::Assignment& n);
        Ret visit(const Nodecl::BitwiseAndAssignment& n);
        Ret visit(const Nodecl::BitwiseOrAssignment& n);
        Ret visit(const Nodecl::BitwiseShlAssignment& n);
        Ret visit(const Nodecl::BitwiseShrAssignment& n);
        Ret visit(const Nodecl::BitwiseXorAssignment& n);
        Ret visit(const Nodecl::ClassMemberAccess& n);
        Ret visit(const Nodecl::Dereference& n);
        Ret visit(const Nodecl::DivAssignment& n);
        Ret visit(const Nodecl::FunctionCall& n);
        Ret visit(const Nodecl::MinusAssignment& n);
        Ret visit(const Nodecl::ModAssignment& n);
        Ret visit(const Nodecl::MulAssignment& n);
        Ret visit(const Nodecl::Symbol& n);
        Ret visit(const Nodecl::VirtualFunctionCall& n);
    };
    
    // ************************ END class for induction variables analysis ************************* //
    // ********************************************************************************************* //
    
    
    
    // ********************************************************************************************* //
    // **************** Visitor matching nodecls for induction variables analysis ****************** //
    //! Returns true when the Nodecl being visited contains or is equal to \_node_to_find
    
    class LIBTL_CLASS MatchingVisitor : public Nodecl::ExhaustiveVisitor<bool>
    {
    private:
        Nodecl::NodeclBase _node_to_find;
        
        //! Specialization of join_list Visitor method for lists of booleans
        virtual bool join_list( TL::ObjectList<bool>& list );
        
    public:
        MatchingVisitor( Nodecl::NodeclBase nodecl );
        Ret visit( const Nodecl::Symbol& n );
        Ret visit( const Nodecl::ArraySubscript& n );
        Ret visit( const Nodecl::ClassMemberAccess& n );
    };
    
    // ************** END visitor matching nodecls for induction variables analysis **************** //
    // ********************************************************************************************* //

}
}

#endif          // TL_IV_ANALYSIS_HPP