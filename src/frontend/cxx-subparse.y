/*!if GRAMMAR_PROLOGUE*/

%type<ast> subparse_type_list

// Subparsing
%token<token_atrib> SUBPARSE_EXPRESSION "<subparse-expression>"
%token<token_atrib> SUBPARSE_EXPRESSION_LIST "<subparse-expression-list>"
%token<token_atrib> SUBPARSE_STATEMENT "<subparse-statement>"
%token<token_atrib> SUBPARSE_DECLARATION "<subparse-declaration>"
%token<token_atrib> SUBPARSE_MEMBER "<subparse-member>"
%token<token_atrib> SUBPARSE_TYPE "<subparse-type>"
%token<token_atrib> SUBPARSE_TYPE_LIST "<subparse-type-list>"


/*!endif*/
/*!if GRAMMAR_RULES*/

translation_unit : subparsing
{
	*parsed_tree = $1;
}
;

subparsing : SUBPARSE_EXPRESSION expression
{
	$$ = $2;
}
| SUBPARSE_STATEMENT statement_seq
{
	$$ = $2;
}
| SUBPARSE_STATEMENT
{
	$$ = NULL;
}
| SUBPARSE_MEMBER member_specification_seq
{
	$$ = $2;
}
| SUBPARSE_DECLARATION declaration_sequence
{
	$$ = $2;
}
| SUBPARSE_DECLARATION
{
    $$ = NULL;
}
| SUBPARSE_TYPE type_id
{
    $$ = $2;
}
| SUBPARSE_TYPE_LIST subparse_type_list
{
    $$ = $2;
}
| SUBPARSE_EXPRESSION_LIST expression_list
{
    $$ = $2;
}
;

subparse_type_list : type_specifier_seq
{
    $$ = ASTListLeaf($1);
}
| subparse_type_list ',' type_specifier_seq
{
    $$ = ASTList($1, $3);
}
;


/*!endif*/
