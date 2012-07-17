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



#ifndef TL_NODE_HPP
#define TL_NODE_HPP

#include <map>

#include "tl-builtin.hpp"
#include "tl-extended-symbol.hpp"
#include "tl-nodecl.hpp"
#include "tl-nodecl-utils.hpp"
#include "tl-objectlist.hpp"
#include "tl-structures.hpp"

namespace TL  {
namespace Analysis {
    
    class Edge;
    class LatticeCellValue;
    class InductionVariableData;
    
    typedef std::tr1::unordered_map<Nodecl::NodeclBase, Nodecl::NodeclBase, Nodecl::Utils::Nodecl_hash, 
                                    Nodecl::Utils::Nodecl_comp> nodecl_map;
    // This type definition is redefined in tl-iv-analysis.hpp
    typedef std::tr1::unordered_map<Nodecl::NodeclBase, InductionVariableData, Nodecl::Utils::Nodecl_hash, 
                                    Nodecl::Utils::Nodecl_comp> IV_map;
    
    //! Class representing a Node in the Extensible Graph
    class LIBTL_CLASS Node : public LinkData {
        
        private:
            // *** Class attributes *** //
        
            int _id;
            ObjectList<Edge*> _entry_edges;
            ObjectList<Edge*> _exit_edges;
            bool _visited;
            bool _visited_aux;
        
            bool _has_deps_computed;    // This boolean only makes sense for Task nodes
                                        // It is true when the auto-dependencies for the node has been computed
            
            // *** Not allowed construction methods *** //
            Node(const Node& n);
            Node& operator=(const Node&);
            
            // *** Analysis *** //
            
            //! Traverses forward the nodes that do not contain Statements inside them.
            /*!
            The method stops when the processed node has a number of children different from 1 or
            does not contain statements.
            \return Pointer to the last node processed.
            */
            Node* advance_over_non_statement_nodes();
            
            //! Traverses backward the nodes that do not contain Statements inside them.
            /*!
            The method stops when the processed node has a number of parents different from 1 or
            does not contain statements.
            \return Pointer to the last node processed.
            */
            Node* back_over_non_statement_nodes();
            
            //! Returns a list with two elements. The firs is the list of upper exposed variables of the graph node;
            //! The second is the list of killed variables of the graph node (Used in composite nodes)
            ObjectList<Utils::ext_sym_set> get_use_def_over_nodes();
            
            //! Returns the list of live in variables in the node (Used in composite nodes)
            Utils::ext_sym_set get_live_in_over_nodes();
            
            //! Returns the list of live out variables in the node (Used in composite nodes)
            Utils::ext_sym_set get_live_out_over_nodes();
        
            //! Sets the variable represented by a symbol as a killed or an upper exposed variable 
            //! depending on @defined attribute
            /*!
            * A variable is killed when it is defined or redefined
            * A variable is upper exposed when it is used before of being killed
            * \param defined Action performed over the symbol: 1 if defined, 0 if not
            * \param n Nodecl containg the whole expression about the use/definition
            */
            void fill_use_def_sets(Nodecl::NodeclBase n, bool defined);
            
            //! Wrapper method for #fill_use_def_sets when there is more than one symbol to be analysed
            void fill_use_def_sets(Nodecl::List n_l, bool defined);
            
        public:
            // *** Constructors *** //
            
            //! Empty Node Constructor.
            /*!
            The method sets to -1 the node identifier and has empty entry and exit edges lists.
            The type of Node_type is, by default, UNCLASSIFIED_NODE.
            */
            Node();
            
            //! Node Constructor.
            /*!
            The entry and exit edges lists are empty.
            A node may contain other nodes, depending on its type.
            \param id Last identifier used to built a node (the method increments it by 1).
            \param outer_node Pointer to the wrapper node. If the node does not belong to other
                                node, then this parameter must be NULL.
            */
            Node(int& id, Node_type type, Node* outer_node);
            
            //! Node Constructor for Basic Normal Nodes.
            /*!
            * The entry and exit edges lists are empty.
            * A node may contain other nodes, depending on its type.
            * \param id Last identifier used to built a node (the method increments it by 1).
            * \param outer_node Pointer to the wrapper node. If the node does not belong to other
            *                   node, then this parameter must be NULL.
            * \param nodecls List of Nodecl containing the Statements to be included in the new node
            */
            Node(int& id, Node_type type, Node* outer_node, ObjectList<Nodecl::NodeclBase> nodecls);

            //! Wrapper constructor in the for Basic Nodes with statements in the case that only one statement
            //! must be included in the list
            Node(int& id, Node_type type, Node* outer_node, Nodecl::NodeclBase nodecl);
            
            bool operator==(const Node& node) const;
        
            
            // *** Modifiers *** //
            
            //! Removes an entry edge from the correspondent list.
            /*!
            If the source node does not exist, then a warning message is shown.
            \param source Pointer to the source node of the Edge that will be erased.
            */
            void erase_entry_edge(Node* source);
            
            //! Removes an exit edge from the correspondent list.
            /*!
            * If the target node does not exist, then a warning message is shown.
            * \param source Pointer to the target node of the Edge that will be erased.
            */
            void erase_exit_edge(Node* target);
            
            
            // *** Getters and setters *** //
            
            //! Returns the node identifier
            int get_id() const;
            
            //! Sets the node identifier
            void set_id(int id);
            
            //! Returns a boolean indicating whether the node was visited or not.
            /*!
            * This method is useful when traversals among the nodes are performed.
            * Once the traversal is ended, all nodes must be set to non-visited using
            * set_visited method.
            */
            bool is_visited() const;
            bool is_visited_aux() const;
            
            //! Sets the node as visited.
            void set_visited(bool visited);
            void set_visited_aux(bool visited);
            
            //! Returns true when the node is a task node and its dependencies have already been calculated
            bool has_deps_computed();
            
            //! Sets node dependencies computation to true
            void set_deps_computed();
            
            //! Returns a boolean indicating whether the node is empty or not
            /*!
            * A empty node is created in the cases when we need a node to be returned but 
            * no node is needed to represent data.
            */
            bool is_empty_node();
            
            //! Returns the list of entry edges of the node.
            ObjectList<Edge*> get_entry_edges() const;
            
            //! Adds a new entry edge to the entry edges list.
            void set_entry_edge(Edge *entry_edge);
            
            //! Returns the list of entry edges types of the node.
            ObjectList<Edge_type> get_entry_edge_types();
            
            //! Returns the list of entry edges labels of the node.
            ObjectList<std::string> get_entry_edge_labels();
            
            //! Returns the list parent nodes of the node.
            ObjectList<Node*> get_parents();
            
            //! Returns the list of exit edges of the node.
            ObjectList<Edge*> get_exit_edges() const;
            
            //! Adds a new exit edge to the exit edges list.
            void set_exit_edge(Edge *exit_edge);
            
            //! Returns the list of exit edges types of the node.
            ObjectList<Edge_type> get_exit_edge_types();
            
            //! Returns the list of exit edges labels of the node.
            ObjectList<std::string> get_exit_edge_labels();
            
            //! Returns the edge between the node and a target node, if exists
            Edge* get_exit_edge(Node* target);
            
            //! Returns the list children nodes of the node.
            ObjectList<Node*> get_children();
        
            //! Returns true when the node is not a composite node (does not contain nodes inside)
            bool is_basic_node();
            
            //! Returns true when the node is a composite node (contains nodes inside)
            bool is_graph_node();
            
            //! Returns true when the node is an entry node
            bool is_entry_node();
            
            //! Returns true when the node is an exit node
            bool is_exit_node();
            
            //! Returns true when the node is the exit node of composite node \graph
            bool is_graph_exit_node( Node* graph );
            
            //! Returns true when the node is a composite node of Loop type
            bool is_loop_node( );
            
            bool is_loop_stride( Node* loop );
            
            bool is_normal_node( );
            
            bool is_labeled_node( );
            
            bool is_function_call_node( );
            
            bool is_task_node( );
            
            //! Returns true when the node is connected to any parent and/or any child
            bool is_connected();
            
            //! Returns true when the node is in its children list
            bool has_child(Node* n);
            
            //! Returns true when the node is in its parents list
            bool has_parent(Node* n);
            
            //! Returns the list of nodes contained inside a node with type graph
            /*!
            * When the node is not a GRAPH_NODE, the list is empty.
            * Otherwise, returns all nodes in the the same level of nesting as the entry node of the graph node
            */
            ObjectList<Node*> get_inner_nodes_in_level();
            
            //! Recursive method to store a chain of nodes into a list
            /*!
            * \param node_l list where the nodes are stored
            */
            void get_inner_nodes_rec(ObjectList<Node*>& node_l);
            
            //! Returns the symbol of the function call contained in the node
            //! This method only works for composite nodes of type "function_call"
            Symbol get_function_node_symbol();
            
            
            //! Returns true if the node has the same identifier and the same entries and exits
            bool operator==(const Node* &n) const;
            
            
            // ****************************************************************************** //
            // ********** Getters and setters for PCFG structural nodes and types *********** //
            
            //! Returns the node type.
            Node_type get_type();
            
            //! Returns a string with the node type of the node.
            std::string get_type_as_string();
            
            //! Returns a string with the graph type of the node.
            //! Node must be a GRAPH_NODE
            std::string get_graph_type_as_string();
            
            //! Returns the entry node of a Graph node. Only valid for graph nodes
            Node* get_graph_entry_node();

            //! Set the entry node of a graph node. Only valid for graph nodes
            void set_graph_entry_node(Node* node);
            
            //! Returns the exit node of a Graph node. Only valid for graph nodes
            Node* get_graph_exit_node();
            
            //! Set the exit node of a graph node. Only valid for graph nodes
            void set_graph_exit_node(Node* node);
            
            //! Returns the nodecl containing the label of the graph node (Only valid for graph nodes)
            //! If the graph doesn't have a label, a null Nodecl is returned
            Nodecl::NodeclBase get_graph_label(Nodecl::NodeclBase n = Nodecl::NodeclBase::null());
            
            //! Set the label of the graph node (Only valid for graph nodes)
            void set_graph_label(Nodecl::NodeclBase n);
            
            //! Returns type of the graph (Only valid for graph nodes)
            Graph_type get_graph_type();
            
            //! Set the graph type to the node (Only valid for graph nodes)
            void set_graph_type(Graph_type graph_type);
            
            //! Returns the type of the loop contained in the node. (Only valid for loop graph nodes)
            Loop_type get_loop_node_type();
            
            //! Set the type of loop contained in a loop graph node
            void set_loop_node_type(Loop_type loop_type);
            
            //! Returns a pointer to the node which contains the actual node
            //! When the node don't have an outer node, NULL is returned
            Node* get_outer_node();
            
            //! Set the node that contains the actual node. It must be a graph node
            void set_outer_node(Node* node);
            
            //! Returns the scope of a graph node containing a block of code.
            //! If no block is contained in the grah node, then returns an empty scope
            Scope get_scope();
            
            //! Set the scope of a graph node containing a block code
            void set_scope(Scope sc);
            
            //! Returns the list of statements contained in the node
            //! If the node does not contain statements, an empty list is returned
            ObjectList<Nodecl::NodeclBase> get_statements();
            
            //! Set the node that contains the actual node. It must be a graph node
            //! It is only valid for Normal nodes, Labeled nodes or Function Call nodes
            void set_statements(ObjectList<Nodecl::NodeclBase> stmts);
            
            //! Returns the Symbol of the statement label contained in the node
            //! If is only valid for Goto or Labeled nodes
            Symbol get_label();
            
            //! Returns the symbol of the statement label contained in the node
            //! If is only valid for Goto or Labeled nodes
            void set_label(Symbol s);
            
            // ******** END Getters and setters for PCFG structural nodes and types ********* //
            // ****************************************************************************** //
            
            
            
            // ****************************************************************************** //
            // **************** Getters and setters for constants analysis ****************** //
            
            //! Gets the Lattice Cell values list associated with the node
            ObjectList<LatticeCellValue> get_lattice_val( );
            
            //! Sets a new Lattice Cell value to the list of Lattice Cell values
            void set_lattice_val( LatticeCellValue lcv );
            
            // ************** END getters and setters for constants analysis **************** //
            // ****************************************************************************** //
            
            
            
            // ****************************************************************************** //
            // *********** Getters and setters for induction variables analysis ************* //
            
            //! Returns the map of induction variables associated to the node (Only valid for loop graph nodes)
            IV_map get_induction_variables();
            
            //! Set a new induction variable in a loop graph node
            void set_induction_variable(Nodecl::NodeclBase iv, InductionVariableData iv_data);
            
            // ********* END getters and setters for induction variables analysis *********** //
            // ****************************************************************************** //
            
            
            
            // ****************************************************************************** //
            // ******************* Getters and setters for OmpSs analysis ******************* //
            
            Nodecl::NodeclBase get_task_context();
            
            void set_task_context(Nodecl::NodeclBase c);
            
            Symbol get_task_function();
            
            void set_task_function(Symbol func_sym);
            
            // ***************** END Getters and setters for OmpSs analysis ***************** //
            // ****************************************************************************** //
            
            
            
            // ****************************************************************************** //
            // ******************* Getters and setters for loops analysis ******************* //
            
            Node* get_stride_node();
            
            void set_stride_node(Node* stride);
            
            bool is_stride_node();
            bool is_stride_node( Node* loop );
            
            // ***************** END getters and setters for loops analysis ***************** //
            // ****************************************************************************** //
            
            
            
            // ****************************************************************************** //
            // *************** Getters and setters for use-definition analysis ************** //
//             
            //! Returns the list of upper exposed variables of the node
            Utils::ext_sym_set get_ue_vars();
            
            //! Adds a new upper exposed variable to the node
            void set_ue_var(Utils::ExtendedSymbol new_ue_var);
            
            //! Adds a new set of upper exposed variable to the node
            void set_ue_var(Utils::ext_sym_set new_ue_vars);
            
            //! Deletes an old upper exposed variable from the node
            void unset_ue_var(Utils::ExtendedSymbol old_ue_var);
            
            //! Returns the list of killed variables of the node
            Utils::ext_sym_set get_killed_vars();
            
            //! Adds a new killed variable to the node
            void set_killed_var(Utils::ExtendedSymbol new_killed_var);
            
            //! Adds a new set of killed variables to the node
            void set_killed_var(Utils::ext_sym_set new_killed_vars);
            
            //! Deletes an old killed variable from the node
            void unset_killed_var(Utils::ExtendedSymbol old_killed_var);            
            
            //! Returns the list of undefined behaviour variables of the node
            Utils::ext_sym_set get_undefined_behaviour_vars();
            
            //! Adds a new undefined behaviour variable to the node
            void set_undefined_behaviour_var(Utils::ExtendedSymbol new_undef_var);            
            
            //! Adds a set of undefined behaviour variables to the node
            void set_undefined_behaviour_var(Utils::ext_sym_set new_undef_vars);
            
            //! Deletes an old undefined behaviour variable from the node
            void unset_undefined_behaviour_var(Utils::ExtendedSymbol old_undef_var);
            
            //! Propagates use-def information from inner nodes to its outer nodes
            void set_graph_node_use_def();
            
            // ************* END getters and setters for use-definition analysis ************ //
            // ****************************************************************************** //
            
            
            
            // ****************************************************************************** //
            // ************ Getters and setters for reaching definitions analysis *********** //
            
            //! Return the map containing, for each symbol defined until this moment, its correspondent expression
            nodecl_map get_reaching_definitions();
            
            //! This method computes the reaching definitions of a graph node from the reaching definitions in the nodes within it
            void set_graph_node_reaching_definitions();
            
            //! Set to one variable a new expression value and append this relationship to the node
            void set_reaching_definition(Nodecl::NodeclBase var, Nodecl::NodeclBase init);
            void set_reaching_definition_list(nodecl_map reach_defs_l);
            void rename_reaching_defintion_var(Nodecl::NodeclBase old_var, Nodecl::NodeclBase new_var);
            
            nodecl_map get_auxiliar_reaching_definitions();
            void set_auxiliar_reaching_definition(Nodecl::NodeclBase var, Nodecl::NodeclBase init);
            
            //!Deletes an old reaching definition from the node
            void unset_reaching_definition(Nodecl::NodeclBase var);
        
            // ********** END getters and setters for reaching definitions analysis ********* //
            // ****************************************************************************** //
            
            
            
            // ****************************************************************************** //
            // ****************** Getters and setters for liveness analysis ***************** //
            
            //! Returns the set of variables that are alive at the entry of the node.
            Utils::ext_sym_set get_live_in_vars();
            
            //! Adds a new live in variable to the node.
            void set_live_in(Utils::ExtendedSymbol new_live_in_var);
            
            //! Sets the list of live in variables.
            /*!
                * If there was any other data in the list, it is removed.
                */
            void set_live_in(Utils::ext_sym_set new_live_in_set);
            
            //! Returns the set of variables that are alive at the exit of the node.
            Utils::ext_sym_set get_live_out_vars();
            
            //! Adds a new live out variable to the node.
            void set_live_out(Utils::ExtendedSymbol new_live_out_var);
            
            //! Sets the list of live out variables.
            /*!
            If there was any other data in the list, it is removed.
            */
            void set_live_out(Utils::ext_sym_set new_live_out_set);
            
            // **************** END getters and setters for liveness analysis *************** //
            // ****************************************************************************** //
            
            
            
            // ****************************************************************************** //
            // ************** Getters and setters for task dependence analysis ************** //
            
            //! Returns the list of input dependences of a task node
            Utils::ext_sym_set get_input_deps();
            
            //! Insert a list of input dependencies to the node
            void set_input_deps(Utils::ext_sym_set new_input_deps);
            
            //! Returns the list of output dependences of a task node
            Utils::ext_sym_set get_output_deps();
                            
            //! Insert a list of output dependencies to the node
            void set_output_deps(Utils::ext_sym_set new_output_deps);
            
            //! Returns the list of inout dependences of a task node
            Utils::ext_sym_set get_inout_deps();
                            
            //! Insert a list of inout dependencies to the node
            void set_inout_deps(Utils::ext_sym_set new_inout_deps);
            
            //! Returns the list of undefined dependences of a task node
            Utils::ext_sym_set get_undef_deps();
                            
            //! Insert a list of undefined dependencies to the node
            void set_undef_deps(Utils::ext_sym_set new_undef_deps);
            
            // ************ END getters and setters for task dependence analysis ************ //
            // ****************************************************************************** //
            
            
            
            // ****************************************************************************** //
            // *************** Getters and setters for auto-scoping analysis **************** //
            
            Utils::ext_sym_set get_shared_vars();
            
            void set_shared_var(Utils::ExtendedSymbol ei);
            
            void set_shared_var(Utils::ext_sym_set new_shared_vars);

            Utils::ext_sym_set get_private_vars();
            
            void set_private_var(Utils::ExtendedSymbol ei);
            
            void set_private_var(Utils::ext_sym_set new_private_vars);
            
            Utils::ext_sym_set get_firstprivate_vars();
            
            void set_firstprivate_var(Utils::ExtendedSymbol ei);
            
            void set_firstprivate_var(Utils::ext_sym_set new_firstprivate_vars);
            
            Utils::ext_sym_set get_undef_sc_vars();
            
            void set_undef_sc_var(Utils::ExtendedSymbol ei);
            
            void set_undef_sc_var(Utils::ext_sym_set new_undef_sc_vars);
            
            Utils::ext_sym_set get_race_vars();

            void set_race_var(Utils::ExtendedSymbol ei);
            
            // ************* END getters and setters for auto-scoping analysis ************** //
            // ****************************************************************************** //

            
            
            // ****************************************************************************** //
            // *********************************** Utils ************************************ //

            void print_use_def_chains();
            void print_liveness();
            void print_auto_scoping();
            void print_task_dependencies();

            // ********************************* END utils ********************************** //
            // ****************************************************************************** //
            
        friend class CfgAnalysisVisitor;
    };
}
}

#endif // TL_NODE_HPP