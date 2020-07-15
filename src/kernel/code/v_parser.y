%{
#include "c_iterator.h"
#include "q_expr.h"
#include "q_helper.h"
#include "ut_stack.h"
#include "os_heap.h"
#include "os_mutex.h"
#include "os_report.h"
#include "os_stdlib.h"
#include "os_string.h"
#include "v_kernel.h"
#include "v_kernelParser.h"

/* yyscan_t is an opaque pointer, a typedef is required here to break a
   circular dependency introduced with bison 2.6 (normally a typedef is
   generated by flex). the define is required to disable the typedef in flex
   generated code */
#define YY_TYPEDEF_YY_SCANNER_T

typedef void *yyscan_t;

#define YYMALLOC os_malloc
#define YYFREE os_free

struct v_context {
    int line;
    int column;
    q_expr expr;
    q_list lastList;
    ut_stack exprStack;
};

static q_list
splitFieldname(
    char *name);

static void
clearExpressionStack(
    ut_stack stack);

static void
clearExpressionList(
    q_list list);

%}

%union {
    c_char      *String;
    c_char      Char;
    c_longlong  Integer;
    c_ulonglong UInteger;
    c_double    Float;
    c_type      Type;
    q_list      List;
    q_expr      Expr;
    q_tag       Tag;
}

%{
/* q_parser_lex must be declared here because q_parser_lex (yylex if no prefix
   would be specified) signature is augmented with an extra argument */
int v__parser_lex(
    YYSTYPE * yylval_param, yyscan_t yyscanner, struct v_context *context);

#define YY_DECL int v__parser_lex \
    (YYSTYPE * yylval_param, yyscan_t yyscanner, struct v_context *context)

static int
yyerror(
    yyscan_t yyscanner, struct v_context *context, char *text);
%}

%define api.pure full
%define api.prefix v__parser_

%lex-param {yyscan_t scanner}
%lex-param {struct v_context *context}
%parse-param {yyscan_t scanner}
%parse-param {struct v_context *context}

%start program

%token AS_KEYWORD NIL TRUET FALSET LRPAR RRPAR
%token ALL LIKE BETWEEN
%token EQUAL NOTEQUAL GREATER LESS GREATEREQUAL LESSEQUAL
%token ORDER BY
%token NOT AND OR
%token SELECT
%token FROM WHERE
%token COMMA
%token NATURAL INNER JOIN
%token UNDEFINEDTOKEN

%token <String>     IDENTIFIER
%token <String>     FIELDNAME
%token <Integer>    PARAM

%token <String>     STRINGVALUE
%token <Char>       CHARVALUE
%token <Integer>    INTEGERVALUE
%token <UInteger>   UINTEGERVALUE
%token <Float>      FLOATVALUE
%token <String>     ENUMERATEDVALUE

%type  <List>    fieldList subjectFieldList
%type  <Expr>    query topicExpression queryExpression
                 aggregation fromClause whereClause
                 joinExpr orderBy topicName subjectField
                 condition condition1 condition2 condition3
                 predicate comparisonPredicate betweenPredicate
                 comparisonPar field parameter
%type  <Tag>     relOp
%type  <String>  ident

/*
%type  <List>    propertyList fieldList scopedName
%type  <Expr>    ID query projectionAttributes
                 fromClause relationalExpr
                 whereClauseOpt expr selectExpr
                 orExpr andExpr equalityExpr literal
                 postfixExpr joinExpr notExpr
                 objectConstruction field
%type  <Tag>     equalityOper relationalOper
*/

%%

/***********************************************************************
 *
 * Query Program
 *
 ***********************************************************************/

program:
    | query
        { context->expr = F1(Q_EXPR_PROGRAM,$1);
          ut_stackPop(context->exprStack);
        }
    ;

query:
      topicExpression
        { $$ = $1; }
    | queryExpression
        { $$ = $1; }
    ;

/***********************************************************************
 *
 * Query Select expression
 *
 ***********************************************************************/

topicExpression:
      SELECT aggregation fromClause whereClause
        { $$ = F3(Q_EXPR_SELECT,$2,$3,$4);
          ut_stackPop(context->exprStack);
          ut_stackPop(context->exprStack);
          ut_stackPop(context->exprStack);
          ut_stackPush(context->exprStack, $$);
        }
    ;

queryExpression:
      condition
        { $$ = $1; }
    | condition orderBy
        { $$ = $1; }
    ;

fromClause:
      FROM joinExpr
        { $$ = F1(Q_EXPR_FROM,$2);
          ut_stackPop(context->exprStack);
          ut_stackPush(context->exprStack, $$);
        }
    ;

aggregation:
      ALL
        { $$ = L0(Q_EXPR_PROJECTION);
          ut_stackPush(context->exprStack, $$);
        }
    | subjectFieldList
        { $$ = L1(Q_EXPR_PROJECTION,$1);
          context->lastList = NULL;
          ut_stackPush(context->exprStack, $$);
        }
    ;

joinExpr:
      topicName
        { $$ = $1; }
    | topicName naturalJoin joinExpr
        { $$ = F2(Q_EXPR_JOIN,$1,$3);
          ut_stackPop(context->exprStack);
          ut_stackPush(context->exprStack, $$);
        }
    ;

naturalJoin:
      INNER NATURAL JOIN
    | NATURAL JOIN
    | NATURAL INNER JOIN
    ;

whereClause:
        { $$ = L0(Q_EXPR_WHERE);
          ut_stackPush(context->exprStack, $$);
        }
    | WHERE condition
        { $$ = F1(Q_EXPR_WHERE,$2);
          ut_stackPop(context->exprStack);
          ut_stackPush(context->exprStack, $$);
        }
    ;

orderBy:
      ORDER BY fieldList
        { $$ = L1(Q_EXPR_ORDER,$3);
          context->lastList = NULL;
          ut_stackPush(context->exprStack, $$);
        }
    ;

topicName:
      ident
        { $$ = q_newId($1);
          ut_stackPush(context->exprStack, $$);
          os_free($1);
        }
    ;

subjectFieldList:
      subjectField
        { $$ = List1($1);
          context->lastList = $$;
        }
    | subjectFieldList COMMA subjectField
        { $$ = q_append($1,$3);
          context->lastList = $$;
        }
    ;

subjectField:
      field
        { $$ = $1; }
    | field AS_KEYWORD field
        { $$ = F2(Q_EXPR_BIND,$3,$1);
          ut_stackPop(context->exprStack);
          ut_stackPop(context->exprStack);
          ut_stackPush(context->exprStack, $$);
        }
    | field field
        { $$ = F2(Q_EXPR_BIND,$2,$1);
          ut_stackPop(context->exprStack);
          ut_stackPop(context->exprStack);
          ut_stackPush(context->exprStack, $$);
        }
    ;

fieldList:
      field
        { $$ = List1($1);
          ut_stackPop(context->exprStack);
          context->lastList = $$;
        }
    | fieldList COMMA field
        { $$ = q_append($1,$3);
          context->lastList = $$;
        }
    ;

condition:
      condition2
        { $$ = $1; }
    | condition2 OR condition
        { $$ = F2(Q_EXPR_OR,$1,$3);
          ut_stackPop(context->exprStack);
          ut_stackPop(context->exprStack);
          ut_stackPush(context->exprStack, $$);
        }
    ;

condition1:
      predicate
        { $$ = $1; }
    | LRPAR condition RRPAR
        { $$ = $2; }
    ;

condition2:
      condition3
        { $$ = $1; }
    | condition3 AND condition
        { $$ = F2(Q_EXPR_AND,$1,$3);
          ut_stackPop(context->exprStack);
          ut_stackPop(context->exprStack);
          ut_stackPush(context->exprStack, $$);
        }
    ;

condition3:
      condition1
        { $$ = $1; }
    | NOT condition1
        { $$ = F1(Q_EXPR_NOT,$2);
          ut_stackPop(context->exprStack);
          ut_stackPush(context->exprStack, $$);
        }
    ;

predicate:
      comparisonPredicate
        { $$ = $1; }
    | betweenPredicate
        { $$ = $1; }
    ;

comparisonPredicate:
      comparisonPar relOp comparisonPar
        { $$ = F2($2,$1,$3);
          ut_stackPop(context->exprStack);
          ut_stackPop(context->exprStack);
          ut_stackPush(context->exprStack, $$);
        }
    ;

comparisonPar:
      field
        { $$ = $1; }
    | parameter
        { $$ = $1; }
    ;

betweenPredicate:
      field BETWEEN parameter AND parameter
        { $$ = F2(Q_EXPR_AND,F2(Q_EXPR_GE,$1,$3),F2(Q_EXPR_LE,q_newId(q_getId($1)),$5));
          ut_stackPop(context->exprStack);
          ut_stackPop(context->exprStack);
          ut_stackPop(context->exprStack);
          ut_stackPush(context->exprStack, $$);
        }
    | field NOT BETWEEN parameter AND parameter
        { $$ = F2(Q_EXPR_OR,F2(Q_EXPR_LT,$1,$4),F2(Q_EXPR_GT,q_newId(q_getId($1)),$6));
          ut_stackPop(context->exprStack);
          ut_stackPop(context->exprStack);
          ut_stackPop(context->exprStack);
          ut_stackPush(context->exprStack, $$);
        }
    ;

field:
      IDENTIFIER
        { $$ = q_newId($1);
          ut_stackPush(context->exprStack, $$);
          os_free($1);
        }
    | FIELDNAME
        { q_list list = splitFieldname($1);
          assert(list != NULL);
          $$ = L1(Q_EXPR_PROPERTY,list); ;
          ut_stackPush(context->exprStack, $$);
          os_free($1);
        }
    ;

relOp:
      EQUAL
        { $$ = Q_EXPR_EQ; }
    | NOTEQUAL
        { $$ = Q_EXPR_NE; }
    | LESS
        { $$ = Q_EXPR_LT; }
    | LESSEQUAL
        { $$ = Q_EXPR_LE; }
    | GREATER
        { $$ = Q_EXPR_GT; }
    | GREATEREQUAL
        { $$ = Q_EXPR_GE; }
    | LIKE
        { $$ = Q_EXPR_LIKE; }
    ;

parameter:
      INTEGERVALUE
        { $$ = q_newInt($1);
          ut_stackPush(context->exprStack, $$);
        }
    | UINTEGERVALUE
        { $$ = q_newUInt($1);
          ut_stackPush(context->exprStack, $$);
        }
    | FLOATVALUE
        { $$ = q_newDbl($1);
          ut_stackPush(context->exprStack, $$);
        }
    | CHARVALUE
        { $$ = q_newChr($1);
          ut_stackPush(context->exprStack, $$);
        }
    | STRINGVALUE
        { $$ = q_newStr($1);
          ut_stackPush(context->exprStack, $$);
          os_free($1);
        }
    | ENUMERATEDVALUE
        { $$ = q_newStr($1);
          ut_stackPush(context->exprStack, $$);
          os_free($1);
        }
    | PARAM
        { $$ = q_newVar($1);
          ut_stackPush(context->exprStack, $$);
        }
    | NIL
        { $$ = NULL; }
    ;

ident:
      IDENTIFIER
        { $$ = $1; }
    ;

%%
#define YY_NO_UNPUT
#define YY_NO_INPUT

#include "v_parser.h"

static int
yyerror(
    yyscan_t yyscanner, struct v_context *context, char *text)
{
    OS_UNUSED_ARG(yyscanner);
    OS_REPORT(OS_ERROR,"SQL parse failed",V_RESULT_ILL_PARAM,"%s at line: %d, column: %d",
              text, context->line, context->column);

    q_dispose(context->expr);
    context->expr = NULL;

    return 0;
}

q_expr
v_parser_parse(
    const char *queryString)
{
    YY_BUFFER_STATE buffer;
    yyscan_t scanner;
    struct v_context context = { 1, 0, NULL, NULL, NULL };

    assert(queryString != NULL);

    context.exprStack = ut_stackNew(32);
    if (context.exprStack != NULL) {
        v__parser_lex_init(&scanner);
        buffer = v__parser__scan_string(queryString, scanner);
        if (yyparse(scanner, &context) == 0) {
            q_exprSetText(context.expr, queryString);
        } else {
            /* 1 on invalid input, 2 on memory exhaustion */
            q_dispose(context.expr);
            context.expr = NULL;
        }

        v__parser__delete_buffer(buffer, scanner);
        v__parser_lex_destroy(scanner);

        clearExpressionStack(context.exprStack);
        clearExpressionList(context.lastList);
    }

    return context.expr;
}

static char *
getProperty(
    char **str)
{
    char *e;
    char *result = NULL;

    if ( *str ) {
        e = strchr(*str, '.');
        if ( e != NULL ) {
            *e = '\0';
            result = *str;
            *str = e + 1;
        } else {
            result = *str;
            *str = NULL;
        }
    }

    return result;
}

static q_list
splitFieldname (
    char *name)
{
    char  *str;
    char  *property;
    q_list list = NULL;

    str = name;

    property = getProperty(&str);
    while ( property ) {
        list = q_append(list, q_newId(property));
        property = getProperty(&str);
    }

    return list;
}

static void
clearExpressionStack (
    ut_stack stack)
{
    while ( !ut_stackIsEmpty(stack) ) {
        q_expr e = (q_expr) ut_stackPop(stack);
        q_dispose(e);
    }

    ut_stackFree(stack);
}

static void
clearExpressionList (
    q_list list)
{
    if ( list ) {
        q_expr q = L1(Q_EXPR_ARRAY, list);
        q_dispose(q);
    }
}
