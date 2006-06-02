#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <gc.h>
#include "cxx-driver.h"
#include "cxx-buildscope.h"
#include "cxx-scope.h"
#include "cxx-prettyprint.h"
#include "cxx-typeutils.h"
#include "cxx-utils.h"
#include "cxx-cexpr.h"
#include "cxx-ambiguity.h"
#include "cxx-printscope.h"
#include "hash_iterator.h"

/*
 * This file builds symbol table. If ambiguous nodes are found disambiguating
 * routines will be called prior to filling symbolic information. Note that
 * disambiguating routines will use the currently built symbol table.
 *
 * Note that some "semantic checks" performed here are intended only to verify
 * that lookup and symbol registration are performed correctly. By no means
 * this is a full type checking phase
 */

static void build_scope_declaration(AST a, scope_t* st);
static void build_scope_declaration_sequence(AST a, scope_t* st);
static void build_scope_simple_declaration(AST a, scope_t* st);

static void build_scope_namespace_definition(AST a, scope_t* st);
static scope_entry_t* build_scope_function_definition(AST a, scope_t* st);
static scope_entry_t* build_scope_declarator_with_parameter_scope(AST a, scope_t* st, scope_t** parameters_scope, 
		gather_decl_spec_t* gather_info, simple_type_t* simple_type_info, type_t** declarator_type);

static void build_scope_member_declaration(AST a, scope_t*  st, 
		access_specifier_t current_access, simple_type_t* simple_type_info);
static void build_scope_simple_member_declaration(AST a, scope_t*  st, 
		access_specifier_t current_access, simple_type_t* simple_type_info);
static void build_scope_member_function_definition(AST a, scope_t*  st, 
		access_specifier_t current_access, simple_type_t* class_info);

static void build_scope_statement(AST statement, scope_t* st);

static void gather_type_spec_from_simple_type_specifier(AST a, scope_t* st, simple_type_t* type_info);
static void gather_type_spec_from_enum_specifier(AST a, scope_t* st, simple_type_t* type_info);
static void gather_type_spec_from_class_specifier(AST a, scope_t* st, simple_type_t* type_info);

static void gather_type_spec_from_elaborated_class_specifier(AST a, scope_t* st, simple_type_t* type_info);
static void gather_type_spec_from_elaborated_enum_specifier(AST a, scope_t* st, simple_type_t* type_info);

static void build_scope_declarator_rec(AST a, scope_t* st, scope_t** parameters_scope, type_t** declarator_type, 
		gather_decl_spec_t* gather_info, AST* declarator_name);


static scope_entry_t* build_scope_declarator_name(AST declarator_name, type_t* declarator_type, 
		gather_decl_spec_t* gather_info, scope_t* st);
static scope_entry_t* build_scope_declarator_id_expr(AST declarator_name, type_t* declarator_type, 
		gather_decl_spec_t* gather_info, scope_t* st);

static void build_scope_linkage_specifier(AST a, scope_t* st);
static void build_scope_linkage_specifier_declaration(AST a, scope_t* st);

void build_scope_template_arguments(AST a, scope_t* st, template_argument_list_t** template_arguments);

static void build_scope_base_clause(AST base_clause, scope_t* st, scope_t* class_scope, class_info_t* class_info);

static void build_scope_template_declaration(AST a, scope_t* st);
static void build_scope_explicit_template_specialization(AST a, scope_t* st);

static void build_scope_template_parameter_list(AST a, scope_t* st, 
		template_parameter_t** template_param_info, int* num_parameters);
static void build_scope_template_parameter(AST a, scope_t* st, 
		template_parameter_t* template_param_info, int num_parameter);
static void build_scope_nontype_template_parameter(AST a, scope_t* st,
		template_parameter_t* template_param_info, int num_parameter);
static void build_scope_type_template_parameter(AST a, scope_t* st,
		template_parameter_t* template_param_info, int num_parameter);

static void build_scope_using_directive(AST a, scope_t* st);

static scope_entry_t* register_new_typedef_name(AST declarator_id, type_t* declarator_type, 
		gather_decl_spec_t* gather_info, scope_t* st);
static scope_entry_t* register_new_variable_name(AST declarator_id, type_t* declarator_type, 
		gather_decl_spec_t* gather_info, scope_t* st);
static scope_entry_t* register_function(AST declarator_id, type_t* declarator_type, 
		gather_decl_spec_t* gather_info, scope_t* st);

static void build_scope_template_function_definition(AST a, scope_t* st, scope_t* template_scope, 
		int num_parameters, template_parameter_t** template_param_info);
static void build_scope_template_simple_declaration(AST a, scope_t* st, scope_t* template_scope, 
		int num_parameters, template_parameter_t** template_param_info);

static cv_qualifier_t compute_cv_qualifier(AST a);

static exception_spec_t* build_exception_spec(scope_t* st, AST a);

static AST get_declarator_name(AST a);

static scope_entry_t* find_function_declaration(scope_t* st, AST declarator_id, 
		type_t* declarator_type, char* is_overload);

// Current linkage, by default C++
static char* current_linkage = "\"C++\"";

// Builds scope for the translation unit
void build_scope_translation_unit(AST a)
{
	AST list = ASTSon0(a);

	if (list == NULL)
		return;

	// The global scope is created here
	compilation_options.global_scope = new_namespace_scope(NULL);

	build_scope_declaration_sequence(list, compilation_options.global_scope);

	fprintf(stderr, "============ SYMBOL TABLE ===============\n");
	print_scope(compilation_options.global_scope, 0);
	fprintf(stderr, "========= End of SYMBOL TABLE ===========\n");
}

static void build_scope_declaration_sequence(AST list, scope_t* st)
{
	AST iter;
	for_each_element(list, iter)
	{
		build_scope_declaration(ASTSon1(iter), st);
	}
}

// Build scope for a declaration
static void build_scope_declaration(AST a, scope_t* st)
{
	switch (ASTType(a))
	{
		case AST_SIMPLE_DECLARATION :
			{
				// Simple declarations are of the form.
				//
				//   int a;
				//   class A { ... } [a];
				//   struct C { ... } [c];
				//   enum E { ... } [e];
				//   int f(int [k]);
				//
				// [thing] means that thing is optional
				build_scope_simple_declaration(a, st);
				break;
			}
		case AST_NAMESPACE_DEFINITION :
			{
				// Namespace definitions are of the form
				//   namespace [name]
				//   {
				//      ...
				//   }
				build_scope_namespace_definition(a, st);
				break;
			}
		case AST_FUNCTION_DEFINITION :
			{
				// A function definition is of the form
				//   [T] f(T1 t, T2 t, T3 t)
				//   {
				//     ...
				//   }
				build_scope_function_definition(a, st);
				break;
			}
		case AST_LINKAGE_SPEC :
			{
				build_scope_linkage_specifier(a, st);
				break;
			}
		case AST_LINKAGE_SPEC_DECL :
			{
				build_scope_linkage_specifier_declaration(a, st);
				break;
			}
		case AST_EXPORT_TEMPLATE_DECLARATION :
		case AST_TEMPLATE_DECLARATION :
			{
				build_scope_template_declaration(a, st);
				break;
			}
		case AST_EXPLICIT_INSTANTIATION :
			{
				// Should we construct something on this ?
				break;
			}
		case AST_EXPLICIT_SPECIALIZATION :
			{
				build_scope_explicit_template_specialization(a, st);
				break;
			}
		case AST_USING_DIRECTIVE :
			{
				build_scope_using_directive(a, st);
				break;
			}
		case AST_AMBIGUITY :
			{
				solve_ambiguous_declaration(a, st);
				// Restart function
				build_scope_declaration(a, st);
				break;
			}
		default :
			{
				internal_error("A declaration of kind '%s' is still unsupported\n", 
						ast_print_node_type(ASTType(a)));
				break;
			}
	}
}

static void build_scope_using_directive(AST a, scope_t* st)
{
	int i, j;
	// First get the involved namespace
	AST global_op = ASTSon0(a);
	AST nested_name = ASTSon1(a);
	AST name = ASTSon2(a);

	scope_entry_list_t* result_list = query_nested_name(st, global_op, 
            nested_name, name, FULL_UNQUALIFIED_LOOKUP);

	if (result_list == NULL)
	{
		internal_error("Namespace '%s' not found\n", ASTText(name));
	}

	if (result_list->next != NULL || result_list->entry->kind != SK_NAMESPACE)
	{
		internal_error("Symbol '%s' is not a namespace\n", ASTText(name));
	}

	scope_entry_t* entry = result_list->entry;

	// Now add this namespace to the used namespaces of this scope
	char already_used = 0;
	// Search
	for (i = 0; (i < st->num_used_namespaces) && !already_used; i++)
	{
		already_used = (st->use_namespace[i] == entry->related_scope);
	}

	if (!already_used)
	{
		P_LIST_ADD(st->use_namespace, st->num_used_namespaces, entry->related_scope);
	}

	// Transitively add related scopes but avoid repeating them
	for (j = 0; j < entry->related_scope->num_used_namespaces; j++)
	{
		already_used = 0;
		// Search
		for (i = 0; (i < st->num_used_namespaces) && !already_used; i++)
		{
			already_used = (st->use_namespace[i] == entry->related_scope->use_namespace[j]);
		}

		if (!already_used)
		{
			P_LIST_ADD(st->use_namespace, st->num_used_namespaces, entry->related_scope->use_namespace[j]);
		}
	}
}

// Builds scope for a simple declaration
static void build_scope_simple_declaration(AST a, scope_t* st)
{
	// Empty declarations are meaningless for the symbol table
	// They are of the form
	//    ;
	if (ASTType(a) == AST_EMPTY_DECL)
		return;

	simple_type_t* simple_type_info = NULL;
	gather_decl_spec_t gather_info;
	// Clear stack debris
	memset(&gather_info, 0, sizeof(gather_info));

	/* A simple declaration has two parts 
	 *
	 *    decl_specifier_seq declarator_list ';'
	 *
	 * Both are optional. decl_specifier_seq is ommited for constructors and
	 * may be ommited for conversion functions and destructors.
	 *
	 * The declarator_list can be ommited only when the decl_specifier_seq
	 * includes a class specifier, enum specifier or an elaborated type name.
	 */

	// If there are decl_specifiers gather information about them.
	//   gather_info will have everything not related to the type.
	//   simple_type_info will have the "base" type of every declarator 
	//
	// For instance 'int *f' will have "int" as a base type, but "f" will be
	// a pointer to int.
	if (ASTSon0(a) != NULL)
	{
		// This can declare a type if it is a class specifier or enum specifier
		build_scope_decl_specifier_seq(ASTSon0(a), st, &gather_info, &simple_type_info);
	}

	// A type has been specified and there are declarators ahead
	if (simple_type_info != NULL && (ASTSon1(a) != NULL))
	{
		AST list, iter;
		list = ASTSon1(a);

		// For every declarator create its full type based on the type
		// specified in the decl_specifier_seq
		for_each_element(list, iter)
		{
			AST init_declarator = ASTSon1(iter);

			if (ASTType(init_declarator) == AST_AMBIGUITY)
			{
				solve_ambiguous_init_declarator(init_declarator, st);
			}

			AST declarator = ASTSon0(init_declarator);
			AST initializer = ASTSon1(init_declarator);

			type_t* declarator_type;

			// This will create the symbol if it is unqualified
			scope_t* parameters_scope = NULL;
			build_scope_declarator_with_parameter_scope(declarator, st, &parameters_scope, 
					&gather_info, simple_type_info, &declarator_type);

			// This is a simple declaration, thus if it does not declare an
			// extern variable or function, the symbol is already defined here
			if (!gather_info.is_extern
					&& declarator_type->kind != TK_FUNCTION)
			{
				AST declarator_name = get_declarator_name(declarator);
				scope_entry_list_t* entry_list = query_id_expression(st, declarator_name, 
                        NOFULL_UNQUALIFIED_LOOKUP);

				if (entry_list == NULL)
				{
					internal_error("Symbol just declared has not been found in the scope!", 0);
				}

				// The last entry will hold our symbol, no need to look for it in the list
				if (entry_list->entry->defined)
				{
					running_error("This symbol has already been defined", 0);
				}

				fprintf(stderr, "Defining symbol '");
				prettyprint(stderr, declarator_name);
				fprintf(stderr, "'\n");
				entry_list->entry->defined = 1;

				if (initializer != NULL)
				{
					// We do not fold it here
					entry_list->entry->expression_value = initializer;
				}
			}
			else
			{
				if (initializer != NULL)
				{
					running_error("An extern symbol cannot be initialized");
				}
			}
		}
	}
}


/* 
 * This function fills gather_info and simple_type_info with proper information
 *
 * gather_info contains every decl_specifier that is not type related. However
 * it can also include qualifiers like const, volatile, restrict, signed,
 * unsigned and long.
 *
 * unsigned int a;  // "unsigned" will be in gather_info and "int" in simple_type_info
 * unsigned b;      // "unsigned" will be considered directly simple_type_info
 * const A b;       // "const" will be in gather_info "A" in simple_type_info
 * unsigned long b; // There is an ambiguity in this case that should be solved favouring
 *                  // the option where there is a type_spec (either unsigned or long)
 *
 * Recall our grammar defines a decl_specifier_seq as 
 *    decl_specifier_seq -> nontype_decl_specifier_seq[opt] type_spec[opt] nontype_decl_specifier_seq[opt]
 *
 * Note: type_spec can be optional due to some corner cases like the following
 *
 *    struct A
 *    {
 *       // None of the following has type_spec but a nontype_decl_specifier_seq
 *       inline operator int(); 
 *       virtual ~A();
 *    };
 */
void build_scope_decl_specifier_seq(AST a, scope_t* st, gather_decl_spec_t* gather_info, 
		simple_type_t **simple_type_info)
{
	AST iter, list;

	// Gather decl specifier sequence information previous to type_spec
	list = ASTSon0(a);
	if (list != NULL)
	{
		for_each_element(list, iter)
		{
			AST spec = ASTSon1(iter);
			gather_decl_spec_information(spec, st, gather_info);
		}
	}

	// Gather decl specifier sequence information after type_spec
	list = ASTSon2(a);
	if (list != NULL)
	{
		for_each_element(list, iter)
		{
			AST spec = ASTSon1(iter);
			gather_decl_spec_information(spec, st, gather_info);
		}
	}

	// Now gather information of the type_spec
	if (ASTSon1(a) != NULL) 
	{
		*simple_type_info = GC_CALLOC(1, sizeof(**simple_type_info));
		gather_type_spec_information(ASTSon1(a), st, *simple_type_info);
		
		// Now update the type_spec with type information that was caught in the decl_specifier_seq
		if (gather_info->is_long)
		{
			// It is not set to 1 because of gcc long long
			(*simple_type_info)->is_long++;
		}

		if (gather_info->is_short)
		{
			(*simple_type_info)->is_short = 1;
		}

		if (gather_info->is_unsigned)
		{
			(*simple_type_info)->is_unsigned = 1;
		}

		if (gather_info->is_signed)
		{
			(*simple_type_info)->is_signed = 1;
		}
		
		// cv-qualification
		(*simple_type_info)->cv_qualifier = CV_NONE;
		if (gather_info->is_const)
		{
			(*simple_type_info)->cv_qualifier |= CV_CONST;
		}

		if (gather_info->is_volatile)
		{
			(*simple_type_info)->cv_qualifier |= CV_VOLATILE;
		}
	}
}

/*
 * This function gathers everything that is in a decl_spec and fills gather_info
 *
 * scope_t* sc is unused here
 */
void gather_decl_spec_information(AST a, scope_t* st, gather_decl_spec_t* gather_info)
{
	switch (ASTType(a))
	{
		// Storage specs
		case AST_AUTO_SPEC :
			gather_info->is_auto = 1;
			break;
		case AST_REGISTER_SPEC :
			gather_info->is_register = 1;
			break;
		case AST_STATIC_SPEC :
			gather_info->is_static = 1;
			break;
		case AST_EXTERN_SPEC :
			gather_info->is_extern = 1;
			break;
		case AST_MUTABLE_SPEC :
			gather_info->is_mutable = 1;
			break;
		case AST_THREAD_SPEC :
			gather_info->is_thread = 1;
			break;
		// Friend
		case AST_FRIEND_SPEC :
			gather_info->is_friend = 1;
			break;
		// Typedef
		case AST_TYPEDEF_SPEC :
			gather_info->is_typedef = 1;
			break;
		// Type modifiers
		case AST_SIGNED_TYPE :
			gather_info->is_signed = 1;
			break;
		case AST_UNSIGNED_TYPE :
			gather_info->is_unsigned = 1;
			break;
		case AST_LONG_TYPE :
			gather_info->is_long = 1;
			break;
		case AST_SHORT_TYPE :
			gather_info->is_short = 1;
			break;
		// CV qualifiers
		case AST_CONST_SPEC :
			gather_info->is_const = 1;
			break;
		case AST_VOLATILE_SPEC :
			gather_info->is_volatile = 1;
			break;
		// Function specifiers
		case AST_INLINE_SPEC :
			gather_info->is_inline = 1;
			break;
		case AST_VIRTUAL_SPEC :
			gather_info->is_virtual = 1;
			break;
		case AST_EXPLICIT_SPEC :
			gather_info->is_explicit = 1;
			break;
		// Unknown node
		default:
			internal_error("Unknown node '%s'", ast_print_node_type(ASTType(a)));
			break;
	}
}


/*
 * This function fills simple_type_info with type information.
 *
 * scope_t* sc is unused here
 */
void gather_type_spec_information(AST a, scope_t* st, simple_type_t* simple_type_info)
{
	switch (ASTType(a))
	{
		case AST_SIMPLE_TYPE_SPECIFIER :
			gather_type_spec_from_simple_type_specifier(a, st, simple_type_info);
			break;
		case AST_ENUM_SPECIFIER :
			gather_type_spec_from_enum_specifier(a, st, simple_type_info);
			break;
		case AST_CLASS_SPECIFIER :
			gather_type_spec_from_class_specifier(a, st, simple_type_info);
			break;
		case AST_ELABORATED_TYPE_ENUM :
			gather_type_spec_from_elaborated_enum_specifier(a, st, simple_type_info);
			break;
		case AST_ELABORATED_TYPE_CLASS :
			gather_type_spec_from_elaborated_class_specifier(a, st, simple_type_info);
			break;
		case AST_ELABORATED_TYPE_TEMPLATE_TEMPLATE :
			internal_error("Still not supported AST_ELABORATED_TYPE_TEMPLATE_TEMPLATE", 0);
			break;
		case AST_ELABORATED_TYPE_TEMPLATE :
			internal_error("Still not supported AST_ELABORATED_TYPE_TEMPLATE", 0);
			break;
		case AST_CHAR_TYPE :
			simple_type_info->kind = STK_BUILTIN_TYPE;
			simple_type_info->builtin_type= BT_CHAR;
			break;
		case AST_WCHAR_TYPE :
			simple_type_info->kind = STK_BUILTIN_TYPE;
			simple_type_info->builtin_type= BT_WCHAR;
			break;
		case AST_BOOL_TYPE :
			simple_type_info->kind = STK_BUILTIN_TYPE;
			simple_type_info->builtin_type= BT_BOOL;
			break;
		case AST_SHORT_TYPE :
			simple_type_info->kind = STK_BUILTIN_TYPE;
			simple_type_info->builtin_type= BT_INT;
			simple_type_info->is_short = 1;
			break;
		case AST_INT_TYPE :
			simple_type_info->kind = STK_BUILTIN_TYPE;
			simple_type_info->builtin_type= BT_INT;
			break;
		case AST_LONG_TYPE :
			simple_type_info->kind = STK_BUILTIN_TYPE;
			simple_type_info->builtin_type= BT_INT;
			simple_type_info->is_long = 1;
			break;
		case AST_SIGNED_TYPE :
			simple_type_info->kind = STK_BUILTIN_TYPE;
			simple_type_info->builtin_type = BT_INT;
			simple_type_info->is_signed = 1;
			break;
		case AST_UNSIGNED_TYPE :
			simple_type_info->kind = STK_BUILTIN_TYPE;
			simple_type_info->builtin_type = BT_INT;
			simple_type_info->is_unsigned = 1;
			break;
		case AST_FLOAT_TYPE :
			simple_type_info->kind = STK_BUILTIN_TYPE;
			simple_type_info->builtin_type = BT_FLOAT;
			break;
		case AST_DOUBLE_TYPE :
			simple_type_info->kind = STK_BUILTIN_TYPE;
			simple_type_info->builtin_type = BT_DOUBLE;
			break;
		case AST_VOID_TYPE :
			simple_type_info->kind = STK_BUILTIN_TYPE;
			simple_type_info->builtin_type = BT_VOID;
			break;
		default:
			internal_error("Unknown node '%s'", ast_print_node_type(ASTType(a)));
	}
}

static void gather_type_spec_from_elaborated_class_specifier(AST a, scope_t* st, simple_type_t* type_info)
{
	// AST class_key = ASTSon0(a);
	AST global_scope = ASTSon1(a);
	AST nested_name_specifier = ASTSon2(a);
	AST symbol = ASTSon3(a);

	scope_entry_list_t* result_list = NULL;

	result_list = query_nested_name(st, global_scope, nested_name_specifier, symbol,
            FULL_UNQUALIFIED_LOOKUP);

	// Now look for a type
	scope_entry_t* entry = NULL;
	while (result_list != NULL 
			&& entry == NULL)
	{
		if (result_list->entry->kind == SK_CLASS
				|| result_list->entry->kind == SK_TEMPLATE_PRIMARY_CLASS
				|| result_list->entry->kind == SK_TEMPLATE_SPECIALIZED_CLASS)
		{
			entry = result_list->entry;
		}

		result_list = result_list->next;
	}

	if (entry == NULL)
	{
		// Create a stub but only if it is unqualified, otherwise it should exist elsewhere
		if (nested_name_specifier == NULL
				&& global_scope == NULL)
		{
			fprintf(stderr, "Type not found, creating a stub for this scope\n");
			scope_entry_t* new_class = new_symbol(st, ASTText(symbol));
			new_class->kind = SK_CLASS;
			new_class->type_information = GC_CALLOC(1, sizeof(*(new_class->type_information)));
			new_class->type_information->kind = TK_DIRECT;
			new_class->type_information->type = GC_CALLOC(1, sizeof(*(new_class->type_information->type)));
			new_class->type_information->type->kind = STK_CLASS;

			type_info->kind = STK_USER_DEFINED;
			type_info->user_defined_type = new_class;
		}
		else
		{
			fprintf(stderr, "Type not found but not creating it because it belongs to another scope\n");
		}
	}
	else
	{
		fprintf(stderr, "Class type found, using it\n");
		type_info->kind = STK_USER_DEFINED;
		type_info->user_defined_type = entry;
	}
}

static void gather_type_spec_from_elaborated_enum_specifier(AST a, scope_t* st, simple_type_t* type_info)
{
	AST global_scope = ASTSon0(a);
	AST nested_name_specifier = ASTSon1(a);
	AST symbol = ASTSon2(a);

	scope_entry_list_t* result_list = NULL;

	result_list = query_nested_name(st, global_scope, nested_name_specifier, symbol,
            FULL_UNQUALIFIED_LOOKUP);

	// Now look for a type
	scope_entry_t* entry = NULL;
	while (result_list != NULL 
			&& entry == NULL)
	{
		if (result_list->entry->kind == SK_ENUM)
		{
			entry = result_list->entry;
		}

		result_list = result_list->next;
	}

	if (entry == NULL)
	{
		// Create a stub but only if it is unqualified, otherwise it should exist anywhere
		if (nested_name_specifier == NULL
				&& global_scope == NULL)
		{
			fprintf(stderr, "Enum type not found, creating a stub for this scope\n");
			scope_entry_t* new_class = new_symbol(st, ASTText(symbol));
			new_class->kind = SK_ENUM;
			new_class->type_information = GC_CALLOC(1, sizeof(*(new_class->type_information)));
			new_class->type_information->kind = TK_DIRECT;
			new_class->type_information->type = GC_CALLOC(1, sizeof(*(new_class->type_information->type)));
			new_class->type_information->type->kind = STK_ENUM;

			type_info->kind = STK_USER_DEFINED;
			type_info->user_defined_type = new_class;
		}
		else
		{
			fprintf(stderr, "Enum type not found but not creating it because it belongs to another scope\n");
		}
	}
	else
	{
		fprintf(stderr, "Enum type found, using it\n");
		type_info->kind = STK_USER_DEFINED;
		type_info->user_defined_type = entry;
	}
}

/*
 * This routine is called in gather_type_spec_information and its purpose is to fill the simple_type
 * with the proper reference of the user defined type.
 */
static void gather_type_spec_from_simple_type_specifier(AST a, scope_t* st, simple_type_t* simple_type_info)
{
	AST global_op = ASTSon0(a);
	AST nested_name_spec = ASTSon1(a);
	AST type_name = ASTSon2(a) != NULL ? ASTSon2(a) : ASTSon3(a);

	scope_entry_list_t* entry_list = query_nested_name(st, global_op, nested_name_spec, 
            type_name, FULL_UNQUALIFIED_LOOKUP);

	// Filter for non types hiding this type name
	// Fix this, it sounds a bit awkward
	if (entry_list == NULL)
	{
		internal_error("The list of types is already empty!\n", 0);
	}
	scope_entry_t* simple_type_entry = filter_simple_type_specifier(entry_list);

	if (simple_type_entry == NULL)
	{
		internal_error("Identifier '%s' in line %d is not a type\n", ASTText(type_name), 
				ASTLine(type_name));
	}

	if (simple_type_entry->type_information == NULL
			|| simple_type_entry->type_information->kind != TK_DIRECT
			|| simple_type_entry->type_information->type == NULL)
	{
		internal_error("The named type '%s' has no direct type entry in symbol table\n", 
				ASTText(type_name));
	}

	simple_type_info->kind = STK_USER_DEFINED;
	simple_type_info->user_defined_type = simple_type_entry;
}

/*
 * This function is called for enum specifiers. It saves all enumerated values
 * and if it has been given a name, it is registered in the scope.
 */
void gather_type_spec_from_enum_specifier(AST a, scope_t* st, simple_type_t* simple_type_info)
{
	simple_type_info->enum_info = (enum_info_t*) GC_CALLOC(1, sizeof(*simple_type_info->enum_info));

	simple_type_info->kind = STK_ENUM;

	AST enum_name = ASTSon0(a);
	// If it has name, we register this type name in the symbol table
	// but only if it has not been declared previously
	if (enum_name != NULL)
	{
		scope_entry_list_t* enum_entry_list = query_unqualified_name(st, ASTText(enum_name));

		scope_entry_t* new_entry;
			
		if (enum_entry_list != NULL 
				&& enum_entry_list->entry->kind == SK_ENUM 
				&& enum_entry_list->next == NULL)
		{
			fprintf(stderr, "Enum '%s' already declared in %p\n", ASTText(enum_name), st);
			new_entry = enum_entry_list->entry;
		}
		else
		{
			fprintf(stderr, "Registering enum '%s' in %p\n", ASTText(enum_name), st);
			new_entry = new_symbol(st, ASTText(enum_name));
			new_entry->kind = SK_ENUM;
		}

		// Copy the type because we are creating it and we would clobber it
		// otherwise
		new_entry->type_information = copy_type(simple_type_to_type(simple_type_info));
		new_entry->defined = 1;

		// Since this type is not anonymous we'll want that simple_type_info
		// refers to this newly created type
		memset(simple_type_info, 0, sizeof(*simple_type_info));
		simple_type_info->kind = STK_USER_DEFINED;
		simple_type_info->user_defined_type = new_entry;
	}

	AST list, iter;
	list = ASTSon1(a);
	
	literal_value_t enum_value = literal_value_minus_one();

	if (list != NULL)
	{
		// If the type had name, refer to the enum type
		if (simple_type_info->kind == STK_USER_DEFINED)
		{
			simple_type_info = simple_type_info->user_defined_type->type_information->type;
		}

		// For every enumeration, sign them up in the symbol table
		for_each_element(list, iter)
		{
			AST enumeration = ASTSon1(iter);
			AST enumeration_name = ASTSon0(enumeration);
			AST enumeration_expr = ASTSon1(enumeration);

			// Note that enums do not define an additional scope
			fprintf(stderr, "Registering enumerator '%s'\n", ASTText(enumeration_name));
			scope_entry_t* enumeration_item = new_symbol(st, ASTText(enumeration_name));

			enumeration_item->kind = SK_ENUMERATOR;
			if (enumeration_expr == NULL)
			{
				// If no value, take the previous and increment it
				enum_value = increment_literal_value(enum_value);
			}
			else
			{
				enum_value = evaluate_constant_expression(enumeration_expr, st);
			}

			enumeration_item->expression_value = tree_from_literal_value(enum_value);

			// DEBUG
			fprintf(stderr, "Enumerator '%s' has value = ", ASTText(enumeration_name));
			prettyprint(stderr, enumeration_item->expression_value);
			fprintf(stderr, "\n");
			// - DEBUG

			P_LIST_ADD(simple_type_info->enum_info->enumeration_list, 
					simple_type_info->enum_info->num_enumeration,
					enumeration_item);
		}

	}

}

static void build_scope_base_clause(AST base_clause, scope_t* st, scope_t* class_scope, class_info_t* class_info)
{
	AST list = ASTSon0(base_clause);
	AST iter;
	for_each_element(list, iter)
	{
		AST base_specifier = ASTSon1(iter);

		AST access_spec = NULL;
		AST global_op; 
		AST nested_name_specifier; 
		AST name;

		switch (ASTType(base_specifier))
		{
			case AST_BASE_SPECIFIER :
				{
					global_op = ASTSon0(base_specifier);
					nested_name_specifier = ASTSon1(base_specifier);
					name = ASTSon2(base_specifier);
					break;
				}
			case AST_BASE_SPECIFIER_ACCESS :
			case AST_BASE_SPECIFIER_VIRTUAL :
			case AST_BASE_SPECIFIER_ACCESS_VIRTUAL :
				{
					access_spec = ASTSon0(base_specifier);
					global_op = ASTSon1(base_specifier);
					nested_name_specifier = ASTSon2(base_specifier);
					name = ASTSon3(base_specifier);
					break;
				}
			default :
				internal_error("Unexpected node '%s'\n", ast_print_node_type(ASTType(base_specifier)));
		}

		scope_entry_list_t* result_list = query_nested_name(st, global_op, nested_name_specifier, name, FULL_UNQUALIFIED_LOOKUP);

		enum cxx_symbol_kind filter[3] = {SK_CLASS, SK_TEMPLATE_PRIMARY_CLASS, SK_TEMPLATE_SPECIALIZED_CLASS};
		result_list = filter_symbol_kind_set(result_list, 3, filter);

		if (result_list == NULL)
		{
			internal_error("Base class not found!\n", 0);
		}

		P_LIST_ADD(class_scope->base_scope, class_scope->num_base_scopes, result_list->entry->related_scope);

		base_class_info_t* base_class = GC_CALLOC(1, sizeof(*base_class));
		base_class->class_type = result_list->entry->type_information;
#warning Missing access specifier for bases

		P_LIST_ADD(class_info->base_classes_list, class_info->num_bases, base_class);
	}
}

/*
 * This function is called for class specifiers
 */
void gather_type_spec_from_class_specifier(AST a, scope_t* st, simple_type_t* simple_type_info)
{
	AST class_head = ASTSon0(a);
	AST class_key = ASTSon0(class_head);
	AST base_clause = ASTSon3(class_head);

	AST class_head_identifier = ASTSon2(class_head);

	simple_type_info->class_info = GC_CALLOC(1, sizeof(*simple_type_info->class_info));
	simple_type_info->kind = STK_CLASS;

	scope_t* inner_scope = new_class_scope(st);

	// Save the inner scope in the class type
	// (it is used when checking member acesses)
	simple_type_info->class_info->inner_scope = inner_scope;

	
	// Now add the bases
	if (base_clause != NULL)
	{
		build_scope_base_clause(base_clause, st, inner_scope, simple_type_info->class_info);
	}

	scope_entry_t* class_entry = NULL;
	
	if (class_head_identifier != NULL)
	{
		// If the class has name, register it in the symbol table but only if
		// it does not exist
		char* name;
		if (ASTType(class_head_identifier) == AST_SYMBOL
				|| ASTType(class_head_identifier) == AST_TEMPLATE_ID)
		{
			if (ASTType(class_head_identifier) == AST_SYMBOL)
			{
				name = ASTText(class_head_identifier);
			}
			else // AST_TEMPLATE_ID
			{
				name = ASTText(ASTSon0(class_head_identifier));

				build_scope_template_arguments(class_head_identifier, st, &(simple_type_info->template_arguments));
			}

			// Check if it exists
			scope_entry_list_t* class_entry_list = query_unqualified_name(st, name);

			if (class_entry_list != NULL 
					&& class_entry_list->entry->kind == SK_CLASS
					&& class_entry_list->next == NULL)
			{
				fprintf(stderr, "Class '%s' already declared in %p\n", name, st);
				class_entry = class_entry_list->entry;
			}
			
			if (class_entry == NULL)
			{
				fprintf(stderr, "Registering class '%s' in %p\n", name, st);
				class_entry = new_symbol(st, name);
				class_entry->kind = SK_CLASS;
			}

			// Copy the type because we are creating it and we would clobber it
			// otherwise
			class_entry->type_information = copy_type(simple_type_to_type(simple_type_info));

			class_entry->related_scope = inner_scope;

			// Since this type is not anonymous we'll want that simple_type_info
			// refers to this newly created type
			memset(simple_type_info, 0, sizeof(*simple_type_info));
			simple_type_info->kind = STK_USER_DEFINED;
			simple_type_info->user_defined_type = class_entry;
		}
		else
		{
			internal_error("Unknown node '%s'\n", ast_print_node_type(ASTType(class_head_identifier)));
		}
	}

	// Member specification
	access_specifier_t current_access;
	// classes have a private by default
	if (ASTType(class_key) == AST_CLASS_KEY_CLASS)
	{
		current_access = AS_PRIVATE;
	}
	// otherwise this is public (for union and structs)
	else
	{
		current_access = AS_PUBLIC;
	}

	AST member_specification = ASTSon1(a);


	// For every member_declaration
	while (member_specification != NULL)
	{
		// If it has an access specifier, update it
		if (ASTSon0(member_specification) != NULL)
		{
			switch (ASTType(ASTSon0(member_specification)))
			{
				case AST_PRIVATE_SPEC : 
					current_access = AS_PRIVATE;
					break;
				case AST_PUBLIC_SPEC :
					current_access = AS_PUBLIC;
					break;
				case AST_PROTECTED_SPEC :
					current_access = AS_PROTECTED;
					break;
				default :
					internal_error("Unknown node type '%s'\n", ast_print_node_type(ASTType(ASTSon0(a))));
			}
		}

		// For every member declaration, sign it up in the symbol table for this class
		if (ASTSon1(member_specification) != NULL)
		{
			build_scope_member_declaration(ASTSon1(member_specification), inner_scope, current_access, simple_type_info);
		}

		member_specification = ASTSon2(member_specification);
	}

	if (class_entry != NULL)
	{
		// If the class had a name, it is completely defined here
		class_entry->defined = 1;
	}
}


/*
 * This function creates a full type using the declarator tree in "a".
 *
 * The base type is fetched from "simple_type_info" and then
 * build_scope_declarator_rec will modify this type to properly represent the
 * correct type.
 *
 * I.e.   int (*f)();
 *
 * Has as a base type "int", but after build_scope_declarator_rec it will be
 * "pointer to function returning int"
 *
 * If the declarator is not abstract, therefore it has a name,
 * build_scope_declarator_name is called to sign it up in the symbol table.
 */
scope_entry_t* build_scope_declarator(AST a, scope_t* st, 
		gather_decl_spec_t* gather_info, simple_type_t* simple_type_info, type_t** declarator_type)
{
	scope_entry_t* entry = build_scope_declarator_with_parameter_scope(a, 
			st, NULL, gather_info, simple_type_info, declarator_type);

	return entry;
}

static scope_entry_t* build_scope_declarator_with_parameter_scope(AST a, scope_t* st, scope_t** parameters_scope, 
		gather_decl_spec_t* gather_info, simple_type_t* simple_type_info, type_t** declarator_type)
{
	scope_entry_t* entry = NULL;
	// Set base type
	*declarator_type = simple_type_to_type(simple_type_info);

	AST declarator_name = NULL;

	build_scope_declarator_rec(a, st, parameters_scope, declarator_type, gather_info, &declarator_name);
	
	if (declarator_name != NULL)
	{
		// Special case for conversion function ids
		// We fix the return type according to the standard
		if ((*declarator_type)->kind == TK_FUNCTION
				&& (*declarator_type)->function->return_type->type == NULL)
		{
			// This looks like a conversion function id
			AST id_expression = ASTSon0(declarator_name);

			AST conversion_function_id = NULL;
			if (ASTType(id_expression) == AST_QUALIFIED_ID)
			{
				if (ASTType(ASTSon2(id_expression)) == AST_CONVERSION_FUNCTION_ID)
				{
					conversion_function_id = ASTSon2(id_expression);
				}
			}

			if (ASTType(id_expression) == AST_CONVERSION_FUNCTION_ID)
			{
				conversion_function_id = id_expression;
			}

			if (conversion_function_id != NULL)
			{
				type_t* conversion_function_type;
				get_conversion_function_name(conversion_function_id, st, &conversion_function_type);

				(*declarator_type)->function->return_type = conversion_function_type;
			}
		}

		entry = build_scope_declarator_name(declarator_name, *declarator_type, gather_info, st);

		fprintf(stderr, "declaring ");
		prettyprint(stderr, declarator_name);
		fprintf(stderr, " as ");
	}
	print_declarator(*declarator_type, st); fprintf(stderr, "\n");

	return entry;
}

/*
 * This functions converts a type "T" to a "pointer to T"
 */
static void set_pointer_type(type_t** declarator_type, scope_t* st, AST pointer_tree)
{
	type_t* pointee_type = *declarator_type;

	(*declarator_type) = GC_CALLOC(1, sizeof(*(*declarator_type)));
	(*declarator_type)->pointer = GC_CALLOC(1, sizeof(*((*declarator_type)->pointer)));
	(*declarator_type)->pointer->pointee = pointee_type;

	switch (ASTType(pointer_tree))
	{
		case AST_POINTER_SPEC :
			if (ASTSon0(pointer_tree) == NULL
					&& ASTSon1(pointer_tree) == NULL)
			{
				(*declarator_type)->kind = TK_POINTER;
			}
			else
			{
				(*declarator_type)->kind = TK_POINTER_TO_MEMBER;

				scope_entry_list_t* entry_list = NULL;
				query_nested_name_spec(st, ASTSon0(pointer_tree), ASTSon1(pointer_tree), &entry_list);

				if (entry_list != NULL)
				{
					(*declarator_type)->pointer->pointee_class = entry_list->entry;
				}
			}
			(*declarator_type)->pointer->cv_qualifier = compute_cv_qualifier(ASTSon2(pointer_tree));
			break;
		case AST_REFERENCE_SPEC :
			(*declarator_type)->kind = TK_REFERENCE;
			break;
		default :
			internal_error("Unhandled node type '%s'\n", ast_print_node_type(ASTType(pointer_tree)));
			break;
	}

	(*declarator_type)->function = NULL;
	(*declarator_type)->array = NULL;
	(*declarator_type)->type = NULL;
}

/*
 * This function converts a type "T" to a "array x of T"
 */
static void set_array_type(type_t** declarator_type, scope_t* st, AST constant_expr)
{
	type_t* element_type = *declarator_type;

	(*declarator_type) = GC_CALLOC(1, sizeof(*(*declarator_type)));
	(*declarator_type)->kind = TK_ARRAY;
	(*declarator_type)->array = GC_CALLOC(1, sizeof(*((*declarator_type)->array)));
	(*declarator_type)->array->element_type = element_type;
	(*declarator_type)->array->array_expr = constant_expr;

	(*declarator_type)->function = NULL;
	(*declarator_type)->type = NULL;
	(*declarator_type)->pointer = NULL;
}

/*
 * This function fetches information for every declarator in the
 * parameter_declaration_clause of a functional declarator
 */
static void set_function_parameter_clause(type_t* declarator_type, scope_t* st, 
		scope_t** parameter_sc, AST parameters)
{
	declarator_type->function->num_parameters = 0;
	declarator_type->function->parameter_list = NULL;
	
	// An empty parameter declaration clause is like (void) in C++
	if (ASTType(parameters) == AST_EMPTY_PARAMETER_DECLARATION_CLAUSE)
	{
		// Maybe this needs some kind of fixing
		return;
	}

	AST iter, list;
	list = parameters;
	
	// Do not contaminate the current symbol table
	scope_t* parameters_scope;
	parameters_scope = new_prototype_scope(st);

	// Save this parameter scope
	if (parameter_sc != NULL)
	{
		*parameter_sc = parameters_scope;
	}

	for_each_element(list, iter)
	{
		AST parameter_declaration = ASTSon1(iter);

		if (ASTType(parameter_declaration) == AST_VARIADIC_ARG)
		{
			parameter_info_t* new_parameter = GC_CALLOC(1, sizeof(*new_parameter));
			new_parameter->is_ellipsis = 1;

			P_LIST_ADD(declarator_type->function->parameter_list, declarator_type->function->num_parameters, new_parameter);
			continue;
		}

		// This is never null
		AST parameter_decl_spec_seq = ASTSon0(parameter_declaration);
		// Declarator can be null
		AST parameter_declarator = ASTSon1(parameter_declaration);
		// Default value can be null
		// The scope of this parameter declaration should be "st" and not parameters_scope
		AST default_argument = ASTSon2(parameter_declaration);

		gather_decl_spec_t gather_info;
		memset(&gather_info, 0, sizeof(gather_info));
		
		simple_type_t* simple_type_info;

		build_scope_decl_specifier_seq(parameter_decl_spec_seq, parameters_scope, &gather_info, &simple_type_info);

		// It is valid in a function declaration not having a declarator at all
		// (note this is different from having an abstract declarator).
		//
		// int f(int, int*);
		//
		// The first "int" does not contain any declarator while the second has
		// an abstract one

		// If we have a declarator compute its type
		if (parameter_declarator != NULL)
		{
			type_t* type_info;
			build_scope_declarator(parameter_declarator, parameters_scope, 
					&gather_info, simple_type_info, &type_info);

			parameter_info_t* new_parameter = GC_CALLOC(1, sizeof(*new_parameter));
			new_parameter->type_info = type_info;
			new_parameter->default_argument = default_argument;

			P_LIST_ADD(declarator_type->function->parameter_list, 
					declarator_type->function->num_parameters, new_parameter);
		}
		// If we don't have a declarator just save the base type
		else
		{
			type_t* type_info = simple_type_to_type(simple_type_info);

			parameter_info_t* new_parameter = GC_CALLOC(1, sizeof(*new_parameter));
			new_parameter->type_info = type_info;
			new_parameter->default_argument = default_argument;

			P_LIST_ADD(declarator_type->function->parameter_list, declarator_type->function->num_parameters, new_parameter);
		}
	}
}

/*
 * This function converts a type "T" into a "function (...) returning T" type
 */
static void set_function_type(type_t** declarator_type, scope_t* st, scope_t** parameters_scope, 
		gather_decl_spec_t* gather_info, AST parameter, AST cv_qualif, AST except_spec)
{
	type_t* returning_type = *declarator_type;

	(*declarator_type) = GC_CALLOC(1, sizeof(*(*declarator_type)));
	(*declarator_type)->kind = TK_FUNCTION;
	(*declarator_type)->function = GC_CALLOC(1, sizeof(*((*declarator_type)->function)));
	(*declarator_type)->function->return_type = returning_type;

	set_function_parameter_clause(*declarator_type, st, parameters_scope, parameter);

	(*declarator_type)->function->cv_qualifier = compute_cv_qualifier(cv_qualif);

	(*declarator_type)->function->exception_spec = build_exception_spec(st, except_spec);

	(*declarator_type)->function->is_static = gather_info->is_static;
	(*declarator_type)->function->is_inline = gather_info->is_inline;
	(*declarator_type)->function->is_virtual = gather_info->is_virtual;
	(*declarator_type)->function->is_explicit = gather_info->is_explicit;
	
	(*declarator_type)->array = NULL;
	(*declarator_type)->pointer = NULL;
	(*declarator_type)->type = NULL;
}

/*
 * This function builds the full type a declarator is representing.  For
 * instance
 *
 *   int (*f)[3];
 *
 * Starts with a base type of "int" and ends being a "pointer to array 3 of int"
 */
static void build_scope_declarator_rec(AST a, scope_t* st, scope_t** parameters_scope, type_t** declarator_type, 
		gather_decl_spec_t* gather_info, AST* declarator_name)
{
	if (a == NULL)
	{
		internal_error("This function does not admit NULL trees", 0);
	}

	switch(ASTType(a))
	{
		case AST_DECLARATOR :
		case AST_PARENTHESIZED_ABSTRACT_DECLARATOR :
		case AST_PARENTHESIZED_DECLARATOR :
			{
				build_scope_declarator_rec(ASTSon0(a), st, parameters_scope, declarator_type, gather_info, declarator_name); 
				break;
			}
		case AST_CONVERSION_DECLARATOR :
		case AST_ABSTRACT_DECLARATOR :
			{
				set_pointer_type(declarator_type, st, ASTSon0(a));
				if (ASTSon1(a) != NULL)
				{
					build_scope_declarator_rec(ASTSon1(a), st, parameters_scope, declarator_type, gather_info, declarator_name);
				}
				break;
			}
		case AST_POINTER_DECL :
			{
				set_pointer_type(declarator_type, st, ASTSon0(a));
				build_scope_declarator_rec(ASTSon1(a), st, parameters_scope, declarator_type, gather_info, declarator_name);
				break;
			}
		case AST_ABSTRACT_ARRAY :
			{
				set_array_type(declarator_type, st, ASTSon1(a));
				if (ASTSon0(a) != NULL)
				{
					build_scope_declarator_rec(ASTSon0(a), st, parameters_scope, declarator_type, gather_info, declarator_name);
				}
				break;
			}
		case AST_DIRECT_NEW_DECLARATOR :
			{
				set_array_type(declarator_type, st, ASTSon1(a));
				if (ASTSon0(a) != NULL)
				{
					build_scope_declarator_rec(ASTSon0(a), st, parameters_scope, declarator_type, gather_info, declarator_name);
				}
				break;
			}
		case AST_NEW_DECLARATOR :
			{
				set_pointer_type(declarator_type, st, ASTSon0(a));
				if (ASTSon0(a) != NULL)
				{
					build_scope_declarator_rec(ASTSon0(a), st, parameters_scope, declarator_type, gather_info, declarator_name);
				}
				break;
			}
		case AST_DECLARATOR_ARRAY :
			{
				set_array_type(declarator_type, st, ASTSon1(a));
				build_scope_declarator_rec(ASTSon0(a), st, parameters_scope, declarator_type, gather_info, declarator_name);
				break;
			}
		case AST_ABSTRACT_DECLARATOR_FUNC :
			{
				set_function_type(declarator_type, st, parameters_scope, gather_info, ASTSon1(a), ASTSon2(a), ASTSon3(a));
				if (ASTSon0(a) != NULL)
				{
					build_scope_declarator_rec(ASTSon0(a), st, parameters_scope, declarator_type, gather_info, declarator_name);
				}
				break;
			}
		case AST_DECLARATOR_FUNC :
			{
				set_function_type(declarator_type, st, parameters_scope, gather_info, ASTSon1(a), ASTSon2(a), ASTSon3(a));
				build_scope_declarator_rec(ASTSon0(a), st, parameters_scope, declarator_type, gather_info, declarator_name);
				break;
			}
		case AST_DECLARATOR_ID_EXPR :
			{
				if (declarator_name != NULL)
				{
					*declarator_name = a;
				}

				break;
			}
		case AST_AMBIGUITY :
			{
				solve_ambiguous_declarator(a, st);
				// Restart function
				build_scope_declarator_rec(a, st, parameters_scope, declarator_type, gather_info, declarator_name);
				break;
			}
		default:
			{
				internal_error("Unknown node '%s'\n", ast_print_node_type(ASTType(a)));
			}
	}
}

/*
 * This function returns the node that holds the name for a non-abstract
 * declarator
 */
static AST get_declarator_name(AST a)
{
	if (a == NULL)
	{
		internal_error("This function does not admit NULL trees", 0);
	}

	switch(ASTType(a))
	{
		case AST_DECLARATOR :
		case AST_PARENTHESIZED_DECLARATOR :
			{
				return get_declarator_name(ASTSon0(a)); 
				break;
			}
		case AST_POINTER_DECL :
			{
				return get_declarator_name(ASTSon1(a));
				break;
			}
		case AST_DECLARATOR_ARRAY :
			{
				return get_declarator_name(ASTSon0(a));
				break;
			}
		case AST_DECLARATOR_FUNC :
			{
				return get_declarator_name(ASTSon0(a));
				break;
			}
		case AST_DECLARATOR_ID_EXPR :
			{
				return ASTSon0(a);
				break;
			}
		default:
			{
				internal_error("Unknown node '%s'\n", ast_print_node_type(ASTType(a)));
			}
	}
}



/*
 * This function fills the symbol table with the information of this declarator
 */
static scope_entry_t* build_scope_declarator_name(AST declarator_name, type_t* declarator_type, 
		gather_decl_spec_t* gather_info, scope_t* st)
{
	switch (ASTType(declarator_name))
	{
		case AST_DECLARATOR_ID_EXPR :
			return build_scope_declarator_id_expr(declarator_name, declarator_type, gather_info, st);
			break;
		default:
			internal_error("Unknown node '%s'\n", ast_print_node_type(ASTType(declarator_name)));
			break;
	}

	return NULL;
}

/*
 * This function fills information for a declarator_id_expr. Actually only
 * unqualified names can be signed up since qualified names should have been
 * declared elsewhere.
 */
static scope_entry_t* build_scope_declarator_id_expr(AST declarator_name, type_t* declarator_type, 
		gather_decl_spec_t* gather_info, scope_t* st)
{
	AST declarator_id = ASTSon0(declarator_name);

	switch (ASTType(declarator_id))
	{
		// Unqualified ones
		case AST_SYMBOL :
			{
				// A simply unqualified symbol "name"

				// We are not declaring a variable but a type
				if (gather_info->is_typedef)
				{
					return register_new_typedef_name(declarator_id, declarator_type, gather_info, st);
				}
				else
				{
					return register_new_variable_name(declarator_id, declarator_type, gather_info, st);
				}
				break;
			}
		case AST_DESTRUCTOR_ID :
			{
				// An unqualified destructor name "~name"
				// 'name' should be a class in this scope
				AST destructor_id = ASTSon0(declarator_id);
				return register_new_variable_name(destructor_id, declarator_type, gather_info, st);
				break;
			}
		case AST_TEMPLATE_ID :
			{
				// This can only happen in an explicit template function instantiation.
				WARNING_MESSAGE("Template id not supported. Skipping it", 0);
				break;
			}
		case AST_OPERATOR_FUNCTION_ID :
			{
				// An unqualified operator_function_id "operator +"
				char* operator_function_name = get_operator_function_name(declarator_id);
				AST operator_id = ASTLeaf(AST_SYMBOL, 0, operator_function_name);
				return register_new_variable_name(operator_id, declarator_type, gather_info, st);
				break;
			}
		case AST_CONVERSION_FUNCTION_ID :
			{
				fprintf(stderr, "Registering a conversion function ID !!!\n");
				// Ok, according to the standard, this function returns the
				// type defined in the conversion function id
				type_t* conversion_type_info = NULL;

				// Get the type and its name
				char* conversion_function_name = get_conversion_function_name(declarator_id,  st, &conversion_type_info);

				scope_entry_t* entry = new_symbol(st, conversion_function_name);

				entry->kind = SK_FUNCTION;
				entry->type_information = declarator_type;

				return entry;
				break;
			}
		// Qualified ones
		case AST_QUALIFIED_ID :
			{
				// A qualified id "a::b::c"
				fprintf(stderr, "--> JANDERKLANDER\n");
				if (declarator_type->kind != TK_FUNCTION)
				{
					scope_entry_list_t* entry_list = query_id_expression(st, declarator_id, FULL_UNQUALIFIED_LOOKUP);
					if (entry_list == NULL)
					{
						internal_error("Qualified id name not found", 0);
					}
					return entry_list->entry;
				}
				else
				{
					char is_overload;
					scope_entry_t* entry = find_function_declaration(st, declarator_id, declarator_type, &is_overload);
					return entry;
				}
				break;
			}
		case AST_QUALIFIED_TEMPLATE :
			{
				// A qualified template "a::b::template c" [?]
				break;
			}
		case AST_QUALIFIED_TEMPLATE_ID :
			{
				// A qualified template_id "a::b::c<int>"
				break;
			}
		case AST_QUALIFIED_OPERATOR_FUNCTION_ID :
			{
				// A qualified operator function_id "a::b::operator +"
				break;
			}
		default :
			{
				internal_error("Unknown node '%s'\n", ast_print_node_type(ASTType(declarator_id)));
				break;
			}
	}

	return NULL;
}

/*
 * This function registers a new typedef name.
 */
static scope_entry_t* register_new_typedef_name(AST declarator_id, type_t* declarator_type, 
		gather_decl_spec_t* gather_info, scope_t* st)
{
	// First query for an existing entry
	scope_entry_list_t* list = query_unqualified_name(st, ASTText(declarator_id));

	// Only enum or classes can exist, otherwise this is an error
	if (list != NULL)
	{
		if (list->next == NULL)
		{
			scope_entry_t* entry = filter_simple_type_specifier(list);
			// This means this was not just a type specifier 
			if (entry == NULL)
			{
				running_error("Symbol '%s' in line %d has been redeclared as a different symbol kind.",
						ASTText(declarator_id), ASTLine(declarator_id));
			}
		}
		else // More than one symbol sounds extremely suspicious
		{
			running_error("Symbol '%s' in line %d has been redeclared as a different symbol kind.",
					ASTText(declarator_id), ASTLine(declarator_id));
		}
	}

	scope_entry_t* entry = new_symbol(st, ASTText(declarator_id));

	fprintf(stderr, "Registering typedef '%s'\n", ASTText(declarator_id));

	// Save aliased type under the type of this declaration
	entry->kind = SK_TYPEDEF;
	entry->type_information = GC_CALLOC(1, sizeof(*(entry->type_information)));
	entry->type_information->kind = TK_DIRECT;
	entry->type_information->type = GC_CALLOC(1, sizeof(*(entry->type_information->type)));
	entry->type_information->type->kind = STK_TYPEDEF;
	entry->type_information->type->aliased_type = declarator_type;

	// TODO - cv qualification
	return entry;
}

/*
 * This function registers a new "variable" (non type) name
 */
static scope_entry_t* register_new_variable_name(AST declarator_id, type_t* declarator_type, 
		gather_decl_spec_t* gather_info, scope_t* st)
{
	if (declarator_type->kind != TK_FUNCTION)
	{
		// Check for existence of this symbol in this scope
		scope_entry_list_t* entry_list = query_id_expression(st, declarator_id, NOFULL_UNQUALIFIED_LOOKUP);

		enum cxx_symbol_kind valid_kind[2] = {SK_CLASS, SK_ENUM};
		scope_entry_list_t* check_list = filter_symbol_non_kind_set(entry_list, 2, valid_kind);
		if (check_list != NULL)
		{
			running_error("Symbol '%s' has been redefined as another symbol kind", 
					ASTText(declarator_id), entry_list->entry->kind);
		}

		fprintf(stderr, "Registering variable '%s' in %p\n", ASTText(declarator_id), st);
		scope_entry_t* entry = new_symbol(st, ASTText(declarator_id));
		entry->kind = SK_VARIABLE;
		entry->type_information = declarator_type;

		return entry;
	}
	else
	{
		return register_function(declarator_id, declarator_type, gather_info, st);
	}
}

static scope_entry_t* register_function(AST declarator_id, type_t* declarator_type, 
		gather_decl_spec_t* gather_info, scope_t* st)
{
	scope_entry_t* entry;

	char is_overload;
	entry = find_function_declaration(st, declarator_id, declarator_type, &is_overload);

	if (entry == NULL)
	{
		if (!is_overload)
		{
			fprintf(stderr, "Registering function '%s'\n", ASTText(declarator_id));
		}
		else
		{
			fprintf(stderr, "Registering overload for function '%s'\n", ASTText(declarator_id));
		}
		scope_entry_t* new_entry = new_symbol(st, ASTText(declarator_id));
		new_entry->kind = SK_FUNCTION;
		new_entry->type_information = declarator_type;

		return new_entry;
	}
	else
	{
		return entry;
	}
}

static scope_entry_t* find_function_declaration(scope_t* st, AST declarator_id, type_t* declarator_type, char* is_overload)
{
	// This function is a mess and should be rewritten
	scope_entry_list_t* entry_list = query_id_expression(st, declarator_id, NOFULL_UNQUALIFIED_LOOKUP);

	function_info_t* function_being_declared = declarator_type->function;
	scope_entry_t* equal_entry = NULL;

	char found_equal = 0;
	*is_overload = 0;

	while (entry_list != NULL && !found_equal)
	{
		scope_entry_t* entry = entry_list->entry;

		if (entry->kind != SK_FUNCTION)
		{
			// Ignore it for now, constructors clash with symbol name
			// running_error("Symbol '%s' already declared as a different symbol type", ASTText(declarator_id), entry->kind);
			entry_list = entry_list->next;
			continue;
		}

		function_info_t* current_function = entry->type_information->function;

		found_equal = !overloaded_function(function_being_declared, current_function, st, CVE_CONSIDER);
		if (found_equal)
		{
			equal_entry = entry;
		}
		else
		{
			*is_overload = 1;
		}

		entry_list = entry_list->next;
	}

	if (!found_equal)
	{
		return NULL;
	}
	else
	{
		return equal_entry;
	}
}

/*
 * This function saves the current linkage, sets the new and restores it back.
 */
static void build_scope_linkage_specifier(AST a, scope_t* st)
{
	AST declaration_sequence = ASTSon1(a);

	if (declaration_sequence == NULL)
		return;

	char* previous_linkage = current_linkage;

	AST linkage_spec = ASTSon0(a);
	current_linkage = ASTText(linkage_spec);

	build_scope_declaration_sequence(declaration_sequence, st);

	current_linkage = previous_linkage;
}

/*
 * Similar to build_scope_linkage_specifier but for just one declaration
 */
static void build_scope_linkage_specifier_declaration(AST a, scope_t* st)
{
	AST declaration = ASTSon1(a);

	char* previous_linkage = current_linkage;

	AST linkage_spec = ASTSon0(a);
	current_linkage = ASTText(linkage_spec);

	build_scope_declaration(declaration, st);

	current_linkage = previous_linkage;
}

/*
 * This function registers a template declaration
 */
static void build_scope_template_declaration(AST a, scope_t* st)
{
	/*
	 * The declaration after the template parameter list can be
	 * a simple declaration or a function definition.
	 *
	 * For the case of a simple_declaration, the following are examples
	 * of what can appear there
	 *
	 *   template <class P, class Q>
	 *   class A                 // A primary template class
	 *   {
	 *   };
	 *
	 *   template <class P>
	 *   class A<P, int>         // A partial specialized class
	 *   {
	 *   };
	 *
	 *   template <class P>
	 *   T A<P>::d = expr;       // For static member initialization
	 *   
	 *   template <class P>           
	 *   void f(..., P q, ...);  // Function declaration
	 *
	 * Template classes are saved in a special form since the may be
	 * specialized in several ways.
	 *
	 */

	/*
	 * Template parameter information is constructed first
	 */
	scope_t* template_scope = new_template_scope(st);
	template_parameter_t** template_param_info = GC_CALLOC(1, sizeof(*template_param_info));
	int num_parameters = 0;
	
	// Construct parameter information
	build_scope_template_parameter_list(ASTSon0(a), template_scope, template_param_info, &num_parameters);

	switch (ASTType(ASTSon1(a)))
	{
		case AST_FUNCTION_DEFINITION :
			build_scope_template_function_definition(ASTSon1(a), st, template_scope, num_parameters, template_param_info);
			break;
		case AST_SIMPLE_DECLARATION :
			build_scope_template_simple_declaration(ASTSon1(a), st, template_scope, num_parameters, template_param_info);
			break;
		default :
			internal_error("Unknown node type '%s'\n", ast_print_node_type(ASTType(a)));
	}
}

/*
 * This function registers an explicit template specialization
 */
static void build_scope_explicit_template_specialization(AST a, scope_t* st)
{
	scope_t* template_scope = new_template_scope(st);
	template_parameter_t** template_param_info = GC_CALLOC(1, sizeof(*template_param_info));
	int num_parameters = 0;

	switch (ASTType(ASTSon0(a)))
	{
		case AST_FUNCTION_DEFINITION :
			build_scope_template_function_definition(ASTSon1(a), st, template_scope, num_parameters, template_param_info);
			break;
		case AST_SIMPLE_DECLARATION :
			build_scope_template_simple_declaration(ASTSon0(a), st, template_scope, num_parameters, template_param_info);
			break;
		default :
			internal_error("Unknown node type '%s'\n", ast_print_node_type(ASTType(a)));
	}
}

static void build_scope_template_function_definition(AST a, scope_t* st, scope_t* template_scope, 
		int num_parameters, template_parameter_t** template_param_info)
{
	template_scope->template_scope = st->template_scope;
	st->template_scope = template_scope;

	// Define the function within st scope but being visible template_scope
	scope_entry_t* entry = build_scope_function_definition(a, st);
    entry = NULL;

	st->template_scope = template_scope->template_scope;
	template_scope->template_scope = NULL;
}

static void build_scope_template_simple_declaration(AST a, scope_t* st, scope_t* template_scope, 
		int num_parameters, template_parameter_t** template_param_info)
{
	/*
	 * A templated simple declaration can be 
	 *
	 *   template <class P, class Q>
	 *   class A                 // A primary template class
	 *   {
	 *   };
	 *
	 *   template <class P>
	 *   class A<P, int>         // A partial specialized class
	 *   {
	 *   };
	 *
	 *   template <class P>
	 *   const T A<P>::d = expr;       // For static const member initialization
	 *
	 * For the last case we won't do anything at the moment.
	 *
	 * For classes if it is a primary template we will register it in the
	 * current scope as a SK_TEMPLATE_CLASS. Otherwise nothing is done since
	 * when declaring a specialization the primary template is extended to hold
	 * the specialization.
	 */

	AST decl_specifier_seq = ASTSon0(a);
	// This list should only contain one element according
	// to the standard
	AST init_declarator_list = ASTSon1(a);

	simple_type_t* simple_type_info = NULL;
	gather_decl_spec_t gather_info;
	memset(&gather_info, 0, sizeof(gather_info));

	if (decl_specifier_seq != NULL)
	{
		// Save the previous template scope in the current one
		// and set the template scope of the current scope 
		template_scope->template_scope = st->template_scope;
		st->template_scope = template_scope;

		// If a class specifier appears here it will be properly declarated in the scope (not within
		// in the template one)
		build_scope_decl_specifier_seq(decl_specifier_seq, st, &gather_info, &simple_type_info);

		// Restore the original template scope
		st->template_scope = template_scope->template_scope;
		template_scope->template_scope = NULL;
	}

	// Let's see what has got declared here
	if (simple_type_info->kind == STK_USER_DEFINED)
	{
		scope_entry_t* entry = simple_type_info->user_defined_type;
		if (entry->kind == SK_CLASS)
		{
			// This is a primary template class if its template arguments are null
			if (entry->type_information->type->template_arguments == NULL)
			{
				entry->kind = SK_TEMPLATE_PRIMARY_CLASS;
			}
			else
			{
				// Otherwise this is a specialization (either partial or 'total')
				entry->kind = SK_TEMPLATE_SPECIALIZED_CLASS;
			}

			// Save the template parameters
			entry->num_template_parameters = num_parameters;
			entry->template_parameter_info = template_param_info;
		}
	}

	// There can be just one declarator here if this is not a class specifier nor a function declaration
	// otherwise no declarator can appear
	//
	//    template <class P>
	//    const T A<P>::d = expr;       // For static const member initialization
	//            ^^^^^^^^^^^^^^
	//            we are handling this
	if (init_declarator_list != NULL)
	{
		if (ASTSon0(init_declarator_list) != NULL)
		{
			running_error("In template declarations only one declarator is valid", 0);
		}

		AST init_declarator = ASTSon1(init_declarator_list);
		AST declarator = ASTSon0(init_declarator);

		// Save the previous template scope in the current one
		// and set the template scope of the current scope 
		template_scope->template_scope = st->template_scope;
		st->template_scope = template_scope;

		// Note that the scope where this declarator will be declared includes
		// the template parameters, since the symbol will have to be qualified
		// it will not create a symbol in "st" but will fetch the previously
		// declared one within the class.
		type_t* declarator_type = NULL;
		scope_entry_t* entry = build_scope_declarator(declarator, st, 
				&gather_info, simple_type_info, &declarator_type);
		
		// Restore the original template scope
		st->template_scope = template_scope->template_scope;
		template_scope->template_scope = NULL;

		if (entry->kind == SK_FUNCTION)
		{
			entry->kind = SK_TEMPLATE_FUNCTION;
		}

		// This is a simple declaration, thus if it does not declare an
		// extern variable or function, the symbol is already defined here
		if (!gather_info.is_extern
				&& declarator_type->kind != TK_FUNCTION)
		{
			AST declarator_name = get_declarator_name(declarator);
			scope_entry_list_t* entry_list = query_id_expression(st, declarator_name, NOFULL_UNQUALIFIED_LOOKUP);

			if (entry_list == NULL)
			{
				internal_error("Symbol just declared has not been found in the scope!", 0);
			}

			// The last entry will hold our symbol, no need to look for it in the list
			if (entry_list->entry->defined)
			{
				running_error("This symbol has already been defined", 0);
			}

			fprintf(stderr, "Defining symbol '");
			prettyprint(stderr, declarator_name);
			fprintf(stderr, "'\n");
			entry_list->entry->defined = 1;

			// if (initializer != NULL)
			// {
			// 	// We do not fold it here
			// 	entry_list->entry->expression_value = initializer;
			// }
		}
	}
}

/*
 * This function registers templates parameters in a given scope
 */
static void build_scope_template_parameter_list(AST a, scope_t* st, 
		template_parameter_t** template_param_info, int* num_parameters)
{
	AST iter;
	AST list = a;

	for_each_element(list, iter)
	{
		AST template_parameter = ASTSon1(iter);

		template_parameter_t* new_template_param = GC_CALLOC(1, sizeof(*new_template_param));

		build_scope_template_parameter(template_parameter, st, new_template_param, *num_parameters);

		P_LIST_ADD(template_param_info, *num_parameters, new_template_param);
	}
}

/*
 * This function registers one template parameter in a given scope
 */
static void build_scope_template_parameter(AST a, scope_t* st, 
		template_parameter_t* template_param_info, int num_parameter)
{
	switch (ASTType(a))
	{
		case AST_PARAMETER_DECL :
			build_scope_nontype_template_parameter(a, st, template_param_info, num_parameter);
			break;
		case AST_TYPE_PARAMETER_CLASS :
		case AST_TYPE_PARAMETER_TYPENAME :
			build_scope_type_template_parameter(a, st, template_param_info, num_parameter);
			break;
		case AST_TYPE_PARAMETER_TEMPLATE :
			// Think about it
			internal_error("Node template template-parameters still not supported", 0);
			break;
		case AST_AMBIGUITY :
			// The ambiguity here is parameter_class vs parameter_decl
			solve_parameter_declaration_vs_type_parameter_class(a);
			// Restart this routine
			build_scope_template_parameter(a, st, template_param_info, num_parameter);
			break;
		default :
			internal_error("Unknown node type '%s'", ast_print_node_type(ASTType(a)));
	}
}

static void build_scope_type_template_parameter(AST a, scope_t* st,
		template_parameter_t* template_param_info, int num_parameter)
{
	// This parameters have the form
	//    CLASS [name] [ = type_id]
	//    TYPENAME [name] [ = type_id]
	//
	// The trick here is create a simple_type that will be of type
	// STK_TYPE_TEMPLATE_PARAMETER. If it is named, register it in the symbol
	// table
	//
	// Create the type
	type_t* new_type = GC_CALLOC(1, sizeof(*new_type));
	new_type->kind = TK_DIRECT;
	new_type->type = GC_CALLOC(1, sizeof(*(new_type->type)));
	new_type->type->kind = STK_TYPE_TEMPLATE_PARAMETER;
	new_type->type->template_parameter_num = num_parameter;

	// Save the info
	template_param_info->type_info = new_type;

	AST name = ASTSon0(a);
	AST type_id = ASTSon1(a);
	
	if (name != NULL)
	{
		// This is a named type parameter. Register it in the symbol table
		fprintf(stderr, "Registering type template-parameter '%s'\n", ASTText(name));
		scope_entry_t* new_entry = new_symbol(st, ASTText(name));
		new_entry->type_information = new_type;
		new_entry->kind = SK_TEMPLATE_PARAMETER;
	}
	
	template_param_info->default_argument = type_id;
}

static void build_scope_nontype_template_parameter(AST a, scope_t* st,
		template_parameter_t* template_param_info, int num_parameter)
{
	// As usual there are three parts
	//     decl_specifier_seq [declarator] [ = expression ]
	simple_type_t* simple_type_info;
	gather_decl_spec_t gather_info;
	memset(&gather_info, 0, sizeof(gather_info));

	AST decl_specifier_seq = ASTSon0(a);
	AST parameter_declarator = ASTSon1(a);

	build_scope_decl_specifier_seq(decl_specifier_seq, st, &gather_info, &simple_type_info);

	simple_type_info->template_parameter_num = num_parameter;

	if (parameter_declarator != NULL)
	{
		// This will add into the symbol table if it has a name
		scope_entry_t* entry = build_scope_declarator(parameter_declarator, st, 
				&gather_info, simple_type_info, &template_param_info->type_info);

		if (entry != NULL)
		{
			fprintf(stderr, "Remembering '%s' as a non-type template parameter\n", entry->symbol_name);
			// This is not a variable, but a template parameter
			entry->kind = SK_TEMPLATE_PARAMETER;
		}
	}
	// If we don't have a declarator just save the base type
	else
	{
		template_param_info->type_info = simple_type_to_type(simple_type_info);
	}
}

/*
 * This function builds symbol table information for a namespace definition
 */
static void build_scope_namespace_definition(AST a, scope_t* st)
{
	AST namespace_name = ASTSon0(a);

	if (namespace_name != NULL)
	{
		// Register this namespace if it does not exist
		scope_entry_list_t* list = query_unqualified_name(st, ASTText(namespace_name));

		scope_entry_list_t* check_list = filter_symbol_non_kind(list, SK_NAMESPACE);
		if (check_list != NULL)
		{
		 	running_error("Identifier '%s' has already been declared as another symbol kind\n", ASTText(namespace_name));
		}

		scope_entry_t* entry;
		if (list != NULL && list->entry->kind == SK_NAMESPACE)
		{
			entry = list->entry;
		}
		else
		{
			// We register a symbol of type namespace and link to a newly created scope.
			scope_t* namespace_scope = new_namespace_scope(st);

			entry = new_symbol(st, ASTText(namespace_name));
			entry->kind = SK_NAMESPACE;
			entry->related_scope = namespace_scope;
		}


		build_scope_declaration_sequence(ASTSon1(a), entry->related_scope);
	}
	else
	{
		// build_scope_declaration_sequence(ASTSon1(a), compilation_options.global_scope);
#warning Unnamed namespace support is missing
	}
}

/*
 * This function builds symbol table information for a function definition
 */
static scope_entry_t* build_scope_function_definition(AST a, scope_t* st)
{
	fprintf(stderr, "Function definition!\n");
	// A function definition has four parts
	//   decl_specifier_seq declarator ctor_initializer function_body

	// decl_specifier_seq [optional]
	// If there is no decl_specifier_seq this has to be a destructor, constructor or conversion function
	gather_decl_spec_t gather_info;
	memset(&gather_info, 0, sizeof(gather_info));
	simple_type_t* type_info = NULL;

	if (ASTSon0(a) != NULL)
	{
		AST decl_spec_seq = ASTSon0(a);

		build_scope_decl_specifier_seq(decl_spec_seq, st, &gather_info, &type_info);
	}

	// declarator
	type_t* declarator_type = NULL;
	scope_entry_t* entry = NULL;
	scope_t* parameter_scope = NULL;
	entry = build_scope_declarator_with_parameter_scope(ASTSon1(a), st, &parameter_scope,
			&gather_info, type_info, &declarator_type);
	if (entry == NULL)
	{
		internal_error("This function does not exist!", 0);
	}

	if (entry->kind != SK_FUNCTION)
	{
		internal_error("This is not a function!!!", 0);
	}


	// Nothing will be done with ctor_initializer at the moment
	// Function_body
	AST function_body = ASTSon3(a);
	AST statement = ASTSon0(function_body);

	scope_t* inner_scope = new_function_scope(st, parameter_scope);

	entry->related_scope = inner_scope;

	if (entry->type_information->function->is_member)
	{
		// If is a member function sign up additional information
		// Note: When this function is being defined within the class is_member
		// will be false, and build_scope_member_definition will be the one that
		// will add "this"
		// Introduce "this" if needed
		if (!entry->type_information->function->is_static)
		{
			type_t* this_type = GC_CALLOC(1, sizeof(*this_type));
			this_type->kind = TK_POINTER;
			this_type->pointer = GC_CALLOC(1, sizeof(*(this_type->pointer)));
			this_type->pointer->pointee = simple_type_to_type(entry->type_information->function->class_type);

			// "this" pseudovariable has the same cv-qualification of this member
			this_type->pointer->pointee->type->cv_qualifier = 
				entry->type_information->function->cv_qualifier;

			// This will put the symbol in the parameter scope, but this is fine
			scope_entry_t* this_symbol = new_symbol(entry->related_scope, "this");

			this_symbol->kind = SK_VARIABLE;
			this_symbol->type_information = this_type;
		}

	}

	build_scope_statement(statement, inner_scope);

	if (entry == NULL)
	{
		running_error("This symbol is undeclared here", 0);
	}
	else 
	{
		fprintf(stderr, "Function '%s' is defined\n", entry->symbol_name);
		entry->defined = 1;
	}

	return entry;
}


static void build_scope_member_declaration(AST a, scope_t*  st, 
		access_specifier_t current_access, simple_type_t* class_info)
{
	switch (ASTType(a))
	{
		case AST_MEMBER_DECLARATION :
			{
				build_scope_simple_member_declaration(a, st, current_access, class_info);
				break;
			}
		case AST_FUNCTION_DEFINITION :
			{
				build_scope_member_function_definition(a, st, current_access, class_info);
				break;
			}
		default:
			{
				internal_error("Unsupported node '%s'\n", ast_print_node_type(ASTType(a)));
				break;
			}
	}
}

static void build_scope_member_function_definition(AST a, scope_t*  st, 
		access_specifier_t current_access, simple_type_t* class_info)
{
	char* class_name = "";
	class_info_t* class_type = NULL;
	if (class_info->kind == STK_USER_DEFINED)
	{
		class_name = class_info->user_defined_type->symbol_name;
		class_type = class_info->user_defined_type->type_information->type->class_info;
	}

	AST declarator = ASTSon1(a);

	AST declarator_name = get_declarator_name(declarator);

	// Get the declarator name
	scope_entry_t* entry = build_scope_function_definition(a, st);
	switch (ASTType(declarator_name))
	{
		case AST_SYMBOL :
			{
				if (strcmp(ASTText(declarator_name), class_name) == 0)
				{
					// This is a constructor
					P_LIST_ADD(class_type->constructor_list, class_type->num_constructors, entry);
				}
				break;
			}
		case AST_DESTRUCTOR_ID :
			{
				// This is the destructor
				class_type->destructor = entry;
				break;
			}
		case AST_OPERATOR_FUNCTION_ID :
			{
				P_LIST_ADD(class_type->operator_function_list, class_type->num_operator_functions, entry);
				break;
			}
		case AST_CONVERSION_FUNCTION_ID :
			{
				conversion_function_t* new_conversion = GC_CALLOC(1, sizeof(*new_conversion));
				
				// The conversion type is the return of the conversion function id
				new_conversion->conversion_type = entry->type_information->function->return_type;
				new_conversion->cv_qualifier = entry->type_information->function->cv_qualifier;

				P_LIST_ADD(class_type->conversion_function_list, class_type->num_conversion_functions, new_conversion);
				break;
			}
		default :
			{
				internal_error("Unknown node '%s'\n", ast_print_node_type(ASTType(declarator_name)));
				break;
			}
	}

	entry->type_information->function->is_member = 1;
	entry->type_information->function->class_type = class_info;
	// Introduce pseudo variable 'this' to the routine unless it is static
	if (!entry->type_information->function->is_static)
	{
		type_t* this_type = GC_CALLOC(1, sizeof(*this_type));
		this_type->kind = TK_POINTER;
		this_type->pointer = GC_CALLOC(1, sizeof(*(this_type->pointer)));
		this_type->pointer->pointee = simple_type_to_type(class_info);

		// "this" pseudovariable has the same cv-qualification of this member
		this_type->pointer->pointee->type->cv_qualifier = 
			entry->type_information->function->cv_qualifier;

		// This will put the symbol in the parameter scope, but this is fine
		scope_entry_t* this_symbol = new_symbol(entry->related_scope, "this");

		this_symbol->kind = SK_VARIABLE;
		this_symbol->type_information = this_type;
	}
}

static void build_scope_simple_member_declaration(AST a, scope_t*  st, 
		access_specifier_t current_access, simple_type_t* class_info)
{
	gather_decl_spec_t gather_info;
	simple_type_t* simple_type_info = NULL;

	memset(&gather_info, 0, sizeof(gather_info));

	if (ASTSon0(a) != NULL)
	{
		build_scope_decl_specifier_seq(ASTSon0(a), st, &gather_info, &simple_type_info);
	}

	if (ASTSon1(a) != NULL)
	{
		AST list = ASTSon1(a);
		AST iter;

		for_each_element(list, iter)
		{
			AST declarator = ASTSon1(iter);

			switch (ASTType(declarator))
			{
				case AST_MEMBER_DECLARATOR :
					{
						type_t* declarator_type = NULL;
						scope_entry_t* entry = build_scope_declarator(ASTSon0(declarator), st, &gather_info, simple_type_info, &declarator_type);

						// If we are declaring a function, state it is a member and
						// save its class_type
						//
						// This will be used further when defining this function.
						if (entry->type_information->kind == SK_FUNCTION)
						{
							entry->type_information->function->is_member = 1;
							entry->type_information->function->class_type = class_info;

							// Also add additional information about this member function
							char* class_name = "";
							class_info_t* class_type = NULL;
							if (class_info->kind == STK_USER_DEFINED)
							{
								class_name = class_info->user_defined_type->symbol_name;
								class_type = class_info->user_defined_type->type_information->type->class_info;
							}

							// Update information in the class about this member function
							AST declarator_name = get_declarator_name(ASTSon1(a));
							switch (ASTType(declarator_name))
							{
								case AST_SYMBOL :
									{
										if (strcmp(ASTText(declarator_name), class_name) == 0)
										{
											// This is a constructor
											P_LIST_ADD(class_type->constructor_list, class_type->num_constructors, entry);
										}
										break;
									}
								case AST_DESTRUCTOR_ID :
									{
										// This is the destructor
										class_type->destructor = entry;
										break;
									}
								case AST_OPERATOR_FUNCTION_ID :
									{
										P_LIST_ADD(class_type->operator_function_list, class_type->num_operator_functions, entry);
										break;
									}
								case AST_CONVERSION_FUNCTION_ID :
									{
										conversion_function_t* new_conversion = GC_CALLOC(1, sizeof(*new_conversion));

										// The conversion type is the return of the conversion function id
										new_conversion->conversion_type = entry->type_information->function->return_type;
										new_conversion->cv_qualifier = entry->type_information->function->cv_qualifier;

										P_LIST_ADD(class_type->conversion_function_list, class_type->num_conversion_functions, new_conversion);
										break;
									}
								default :
									{
										internal_error("Unknown node '%s'\n", ast_print_node_type(ASTType(declarator_name)));
										break;
									}
							}
						}
						break;
					}
				default :
					{
						internal_error("Unhandled node '%s'", ast_print_node_type(ASTType(declarator)));
						break;
					}
			}
		}
	}
}

/*
 * This function computes a cv_qualifier_t from an AST
 * containing a list of cv_qualifiers
 */
static cv_qualifier_t compute_cv_qualifier(AST a)
{
	cv_qualifier_t result = CV_NONE;

	// Allow empty trees to ease us the use of this function
	if (a == NULL)
	{
		return result;
	}

	if (ASTType(a) != AST_NODE_LIST)
	{
		internal_error("This function expects a list", 0);
	}

	AST list, iter;
	list = a;

	for_each_element(list, iter)
	{
		AST cv_qualifier = ASTSon1(iter);

		switch (ASTType(cv_qualifier))
		{
			case AST_CONST_SPEC :
				result |= CV_CONST;
				break;
			case AST_VOLATILE_SPEC :
				result |= CV_VOLATILE;
				break;
			default:
				internal_error("Unknown node type '%s'", ast_print_node_type(ASTType(cv_qualifier)));
				break;
		}
	}

	return result;
}

// This function fills returns an exception_spec_t* It returns NULL if no
// exception spec has been defined. Note that 'throw ()' is an exception spec
// and non-NULL is returned in this case.
static exception_spec_t* build_exception_spec(scope_t* st, AST a)
{
	// No exception specifier at all
	if (a == NULL)
		return NULL;

	exception_spec_t* result = GC_CALLOC(1, sizeof(*result));

	AST type_id_list = ASTSon0(a);

	if (type_id_list == NULL)
		return result;

	AST iter;

	for_each_element(type_id_list, iter)
	{
		AST type_id = ASTSon1(iter);

		// A type_id is a type_specifier_seq followed by an optional abstract
		// declarator
		AST type_specifier_seq = ASTSon0(type_id);
		AST abstract_decl = ASTSon1(type_id);

		// A type_specifier_seq is essentially a subset of a
		// declarator_specifier_seq so we can reuse existing functions
		simple_type_t* type_info = NULL;
		gather_decl_spec_t gather_info;
		memset(&gather_info, 0, sizeof(gather_info));
	
		build_scope_decl_specifier_seq(type_specifier_seq, st, &gather_info, &type_info);

		if (abstract_decl != NULL)
		{
			type_t* declarator_type;
			build_scope_declarator(abstract_decl, st, &gather_info, type_info, &declarator_type);
			P_LIST_ADD(result->exception_type_seq, result->num_exception_types,
					declarator_type);
		}
		else
		{
			type_t* declarator_type = simple_type_to_type(type_info);
			P_LIST_ADD(result->exception_type_seq, result->num_exception_types,
					declarator_type);
		}
	}

	return result;
}

void build_scope_template_arguments(AST class_head_id, scope_t* st, template_argument_list_t** template_arguments)
{
	AST list, iter;
	*template_arguments = GC_CALLOC(sizeof(1), sizeof(*(*template_arguments)));

	(*template_arguments)->num_arguments = 0;

	int num_arguments = 0;


	list = ASTSon1(class_head_id);
	// Count the arguments
	for_each_element(list, iter)
	{
		num_arguments++;
	}
	
	// Complete arguments with default ones
	// First search primary template
	AST template_name = ASTSon0(class_head_id);
	scope_entry_list_t* templates_list = query_unqualified_name(st, ASTText(template_name));
	
	scope_entry_t* primary_template = NULL;

	while ((templates_list != NULL) 
			&& (primary_template == NULL))
	{
		if (templates_list->entry->kind == SK_TEMPLATE_PRIMARY_CLASS)
		{
			primary_template = templates_list->entry;
		}
		
		templates_list = templates_list->next;
	}

	if (primary_template == NULL)
	{
		internal_error("Primary template for '%s' not found", ASTText(template_name));
	}

	if (primary_template->num_template_parameters > num_arguments)
	{
		// We have to complete with default arguments
		fprintf(stderr, "Completing template arguments with default arguments\n");
		
		AST default_arg_list = ASTSon1(class_head_id);
		int k;
		for (k = num_arguments; 
				k < (primary_template->num_template_parameters);
				k++)
		{
			if (primary_template->template_parameter_info[k]->default_argument == NULL)
			{
				internal_error("Parameter '%d' of template '%s' has no default argument", 
						k, ASTText(template_name));
			}

			default_arg_list = ASTMake2(AST_NODE_LIST, default_arg_list, 
					primary_template->template_parameter_info[k]->default_argument, 0, NULL);
		}

		// Relink correctly
		ASTParent(default_arg_list) = class_head_id;
		ASTSon1(class_head_id) = default_arg_list;
	}

	list = ASTSon1(class_head_id);
	for_each_element(list, iter)
	{
		AST template_argument = ASTSon1(iter);

		// We should check if this names a type
		// There is an ambiguity around here that will have to be handled
		switch (ASTType(template_argument))
		{
			case AST_TEMPLATE_TYPE_ARGUMENT:
				{
					template_argument_t* new_template_argument = GC_CALLOC(1, sizeof(*new_template_argument));
					new_template_argument->kind = TAK_TYPE;
					// Create the type_spec
					// A type_id is a type_specifier_seq followed by an optional abstract
					// declarator
					AST type_template_argument = ASTSon0(template_argument);
					AST type_specifier_seq = ASTSon0(type_template_argument);
					AST abstract_decl = ASTSon1(type_template_argument);

					// A type_specifier_seq is essentially a subset of a
					// declarator_specifier_seq so we can reuse existing functions
					simple_type_t* type_info;
					gather_decl_spec_t gather_info;
					memset(&gather_info, 0, sizeof(gather_info));

					build_scope_decl_specifier_seq(type_specifier_seq, st, &gather_info, &type_info);

					type_t* declarator_type;
					if (abstract_decl != NULL)
					{
						build_scope_declarator(abstract_decl, st, &gather_info, type_info, &declarator_type);
					}
					else
					{
						declarator_type = simple_type_to_type(type_info);
					}
					new_template_argument->type = declarator_type;
					P_LIST_ADD((*template_arguments)->argument_list, (*template_arguments)->num_arguments, new_template_argument);
					break;
				}
			case AST_TEMPLATE_EXPRESSION_ARGUMENT :
				{
					template_argument_t* new_template_argument = GC_CALLOC(1, sizeof(*new_template_argument));
					new_template_argument->kind = TAK_NONTYPE;

					AST expr_template_argument = ASTSon0(template_argument);
					// Fold the expression and save it folded
					literal_value_t constant_expr = evaluate_constant_expression(expr_template_argument, st);

					new_template_argument->expression = tree_from_literal_value(constant_expr);

					P_LIST_ADD((*template_arguments)->argument_list, (*template_arguments)->num_arguments, new_template_argument);
					break;
				}
			case AST_AMBIGUITY :
				{
					internal_error("Ambiguous node\n", 0);
					break;
				}
			default :
				internal_error("Unexpected node '%s'\n", ast_print_node_type(ASTType(template_argument)));
				break;
		}
	}

}

// Gives a name to an operator
char* get_operator_function_name(AST declarator_id)
{
	if (ASTType(declarator_id) != AST_OPERATOR_FUNCTION_ID)
	{
		internal_error("This node is not valid here '%s'", ast_print_node_type(ASTType(declarator_id)));
	}

	AST operator  = ASTSon0(declarator_id);

	switch (ASTType(operator))
	{
		case AST_NEW_OPERATOR :
			return "operator new";
		case AST_DELETE_OPERATOR :
			return "operator delete";
		case AST_NEW_ARRAY_OPERATOR :
			return "operator new[]";
		case AST_DELETE_ARRAY_OPERATOR :
			return "operator delete[]";
		case AST_ADD_OPERATOR :
			return "operator +";
		case AST_MINUS_OPERATOR :
			return "operator -";
		case AST_MULT_OPERATOR :
			return "operator *";
		case AST_DIV_OPERATOR :
			return "operator /";
		case AST_MOD_OPERATOR :
			return "operator %";
		case AST_BITWISE_XOR_OPERATOR :
			return "operator ^";
		case AST_BITWISE_AND_OPERATOR :
			return "operator &";
		case AST_BITWISE_OR_OPERATOR :
			return "operator |";
		case AST_BITWISE_NEG_OPERATOR :
			return "operator ~";
		case AST_LOGICAL_NOT_OPERATOR :
			return "operator !";
		case AST_ASSIGNMENT_OPERATOR :
			return "operator =";
		case AST_LOWER_OPERATOR :
			return "operator <";
		case AST_GREATER_OPERATOR :
			return "operator >";
		case AST_ADD_ASSIGN_OPERATOR :
			return "operator +=";
		case AST_SUB_ASSIGN_OPERATOR :
			return "operator -=";
		case AST_MUL_ASSIGN_OPERATOR :
			return "operator *=";
		case AST_DIV_ASSIGN_OPERATOR :
			return "operator /=";
		case AST_MOD_ASSIGN_OPERATOR :
			return "operator %=";
		case AST_XOR_ASSIGN_OPERATOR :
			return "operator ^=";
		case AST_AND_ASSIGN_OPERATOR :
			return "operator &=";
		case AST_OR_ASSIGN_OPERATOR :
			return "operator |=";
		case AST_LEFT_OPERATOR :
			return "operator <<";
		case AST_RIGHT_OPERATOR :
			return "operator >>";
		case AST_LEFT_ASSIGN_OPERATOR :
			return "operator <<=";
		case AST_RIGHT_ASSIGN_OPERATOR :
			return "operator >>=";
		case AST_EQUAL_OPERATOR :
			return "operator ==";
		case AST_DIFFERENT_OPERATOR :
			return "operator !=";
		case AST_LESS_OR_EQUAL_OPERATOR :
			return "operator <=";
		case AST_GREATER_OR_EQUAL_OPERATOR :
			return "operator >=";
		case AST_LOGICAL_AND_OPERATOR :
			return "operator &&";
		case AST_LOGICAL_OR_OPERATOR :
			return "operator ||";
		case AST_INCREMENT_OPERATOR :
			return "operator ++";
		case AST_DECREMENT_OPERATOR :
			return "operator --";
		case AST_COMMA_OPERATOR :
			return "operator ,";
		case AST_POINTER_OPERATOR :
			return "operator ->";
		case AST_POINTER_DERREF_OPERATOR :
			return "operator ->*";
		case AST_FUNCTION_CALL_OPERATOR :
			return "operator ()";
		case AST_SUBSCRIPT_OPERATOR :
			return "operator []";
		default :
			internal_error("Invalid node type '%s'\n", ast_print_node_type(ASTType(declarator_id)));
	}
}


/*
 * Building scope for statements
 */

typedef void (*stmt_scope_handler_t)(AST a, scope_t* st);

static void build_scope_compound_statement(AST a, scope_t* st)
{
	scope_t* block_scope = new_block_scope(st, st->prototype_scope, st->function_scope);

	AST list = ASTSon0(a);
	if (list != NULL)
	{
		AST iter;
		for_each_element(list, iter)
		{
			build_scope_statement(ASTSon1(iter), block_scope);
		}
	}
}

static void build_scope_condition(AST a, scope_t* st)
{
	if (ASTSon0(a) != NULL 
			&& ASTSon1(a) != NULL)
	{
		// This condition declares something in this scope
		AST type_specifier_seq = ASTSon0(a);
		AST declarator = ASTSon1(a);

		if (ASTType(type_specifier_seq) == AST_AMBIGUITY)
		{
			solve_ambiguous_type_spec_seq(type_specifier_seq, st);
		}
		
		if (ASTType(declarator) == AST_AMBIGUITY)
		{
			internal_error("Unexpected ambiguity", 0);
		}

		// A type_specifier_seq is essentially a subset of a
		// declarator_specifier_seq so we can reuse existing functions
		simple_type_t* type_info = NULL;
		gather_decl_spec_t gather_info;
		memset(&gather_info, 0, sizeof(gather_info));
	
		build_scope_decl_specifier_seq(type_specifier_seq, st, &gather_info, &type_info);

		type_t* declarator_type = NULL;
		scope_entry_t* entry = build_scope_declarator(declarator, st, &gather_info, type_info, &declarator_type);

		solve_possibly_ambiguous_expression(ASTSon2(a), st);
		
		entry->expression_value = ASTSon2(a);
	}
	else
	{
		solve_possibly_ambiguous_expression(ASTSon2(a), st);
	}
}

static void build_scope_while_statement(AST a, scope_t* st)
{
	scope_t* block_scope = new_block_scope(st, st->prototype_scope, st->function_scope);
	build_scope_condition(ASTSon0(a), block_scope);

	if (ASTSon1(a) != NULL)
	{
		build_scope_statement(ASTSon1(a), block_scope);
	}
}

static void build_scope_ambiguity_handler(AST a, scope_t* st)
{
	solve_ambiguous_statement(a, st);
	// Restart
	build_scope_statement(a, st);
}

static void build_scope_declaration_statement(AST a, scope_t* st)
{
	AST declaration = ASTSon0(a);

	build_scope_declaration(declaration, st);
}

static void solve_expression_ambiguities(AST a, scope_t* st)
{
	solve_possibly_ambiguous_expression(ASTSon0(a), st);
}

static void build_scope_if_else_statement(AST a, scope_t* st)
{
	scope_t* block_scope = new_block_scope(st, st->prototype_scope, st->function_scope);

	AST condition = ASTSon0(a);
	build_scope_condition(condition, block_scope);

	AST then_branch = ASTSon1(a);
	build_scope_statement(then_branch, block_scope);

	AST else_branch = ASTSon2(a);
	if (else_branch != NULL)
	{
		build_scope_statement(else_branch, block_scope);
	}
}

static void build_scope_for_statement(AST a, scope_t* st)
{
	AST for_init_statement = ASTSon0(a);
	AST condition = ASTSon1(a);
	AST expression = ASTSon2(a);
	AST statement = ASTSon3(a);

	if (ASTType(for_init_statement) == AST_AMBIGUITY)
	{
		solve_ambiguous_for_init_statement(for_init_statement, st);
	}

	scope_t* block_scope = new_block_scope(st, st->prototype_scope, st->function_scope);

	if (condition != NULL)
	{
		build_scope_condition(condition, block_scope);
	}

	if (expression != NULL)
	{
		solve_possibly_ambiguous_expression(expression, block_scope);
	}
	
	build_scope_statement(statement, block_scope);
}

static void build_scope_switch_statement(AST a, scope_t* st)
{
	scope_t* block_scope = new_block_scope(st, st->prototype_scope, st->function_scope);
	AST condition = ASTSon0(a);
	AST statement = ASTSon1(a);

	build_scope_condition(condition, block_scope);
	build_scope_statement(statement, block_scope);
}

static void build_scope_labeled_statement(AST a, scope_t* st)
{
	AST statement = ASTSon0(a);
	build_scope_statement(statement, st);
}

static void build_scope_default_statement(AST a, scope_t* st)
{
	AST statement = ASTSon0(a);
	build_scope_statement(statement, st);
}

static void build_scope_case_statement(AST a, scope_t* st)
{
	AST constant_expression = ASTSon0(a);
	AST statement = ASTSon1(a);
	solve_possibly_ambiguous_expression(constant_expression, st);

	build_scope_statement(statement, st);
}

static void build_scope_return_statement(AST a, scope_t* st)
{
	AST expression = ASTSon0(a);
	if (expression != NULL)
	{
		solve_possibly_ambiguous_expression(expression, st);
	}
}

static void build_scope_try_block(AST a, scope_t* st)
{
	AST compound_statement = ASTSon0(a);

	build_scope_statement(compound_statement, st);

	AST handler_seq = ASTSon1(a);
	AST iter;

	for_each_element(handler_seq, iter)
	{
		AST handler = ASTSon1(iter);

		if (ASTType(handler) == AST_ANY_EXCEPTION)
		{
			continue;
		}

		AST exception_declaration = ASTSon0(handler);
		AST compound_statement = ASTSon1(handler);

		scope_t* block_scope = new_block_scope(st, st->prototype_scope, st->function_scope);

		AST type_specifier_seq = ASTSon0(exception_declaration);
		// This declarator can be null
		AST declarator = ASTSon1(exception_declaration);

		simple_type_t* type_info = NULL;
		gather_decl_spec_t gather_info;
		memset(&gather_info, 0, sizeof(gather_info));

		build_scope_decl_specifier_seq(type_specifier_seq, block_scope, &gather_info, &type_info);

		if (declarator != NULL)
		{
			type_t* declarator_type = NULL;
			build_scope_declarator(declarator, block_scope, &gather_info, type_info, &declarator_type);
		}

		build_scope_statement(compound_statement, st);
	}
}

static void build_scope_do_statement(AST a, scope_t* st)
{
	AST statement = ASTSon0(a);
	AST expression = ASTSon1(a);

	build_scope_statement(statement, st);
	solve_possibly_ambiguous_expression(expression, st);
}

static void build_scope_null(AST a, scope_t* st)
{
	// Do nothing
}

#define STMT_HANDLER(type, hndl) [type] = hndl

static stmt_scope_handler_t stmt_scope_handlers[] =
{
	STMT_HANDLER(AST_AMBIGUITY, build_scope_ambiguity_handler),
	STMT_HANDLER(AST_EXPRESSION_STATEMENT, solve_expression_ambiguities),
	STMT_HANDLER(AST_DECLARATION_STATEMENT, build_scope_declaration_statement),
	STMT_HANDLER(AST_COMPOUND_STATEMENT, build_scope_compound_statement),
	STMT_HANDLER(AST_DO_STATEMENT, build_scope_do_statement),
	STMT_HANDLER(AST_WHILE_STATEMENT, build_scope_while_statement),
	STMT_HANDLER(AST_IF_ELSE_STATEMENT, build_scope_if_else_statement),
	STMT_HANDLER(AST_FOR_STATEMENT, build_scope_for_statement),
	STMT_HANDLER(AST_LABELED_STATEMENT, build_scope_labeled_statement),
	STMT_HANDLER(AST_DEFAULT_STATEMENT, build_scope_default_statement),
	STMT_HANDLER(AST_CASE_STATEMENT, build_scope_case_statement),
	STMT_HANDLER(AST_RETURN_STATEMENT, build_scope_return_statement),
	STMT_HANDLER(AST_TRY_BLOCK, build_scope_try_block),
	STMT_HANDLER(AST_SWITCH_STATEMENT, build_scope_switch_statement),
	STMT_HANDLER(AST_EMPTY_STATEMENT, build_scope_null),
	STMT_HANDLER(AST_BREAK_STATEMENT, build_scope_null),
	STMT_HANDLER(AST_CONTINUE_STATEMENT, build_scope_null),
	STMT_HANDLER(AST_GOTO_STATEMENT, build_scope_null),
};


static void build_scope_statement(AST a, scope_t* st)
{
	stmt_scope_handler_t f = stmt_scope_handlers[ASTType(a)];

	if (f != NULL)
	{
		f(a, st);
	}
	else
	{
		WARNING_MESSAGE("Statement node type '%s' doesn't have handler", ast_print_node_type(ASTType(a)));
	}
}
