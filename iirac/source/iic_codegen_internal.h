/**
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_ast.h"
#include "ii_diagnostics.h"
#include "ii_source.h"
#include "ii_trace.h"
#include "uf_memory.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct CodegenContext CodegenContext;

struct CodegenContext {
    Ast* ast;
    FILE* output;
    DiagnosticContext* diag;
    TraceContext* trace;
    Source* source;

    /* Arena for temporary allocations */
    UfMemRegion* arena;

    /* Counters for generating unique names */
    uint32_t label_counter;
    uint32_t temp_counter;
    uint32_t string_counter;

    /* Current function context */
    AstFuncDecl* current_func;
    uint32_t current_frame_size;
    uint32_t next_slot;

    /* String literal collection */
    const char** string_literals;
    size_t string_count;
};

/*
 * Type generation.
 */

void _emit_type_def(CodegenContext*, AstBlueprintDecl* bp);

/* Emit QBE type definition for anonymous blueprint type. */
void _emit_anon_type_def(CodegenContext*, AstType* type);

/*
 * Data section generation.
 */

/* Emit all string literals as QBE data definitions. */
void _emit_data_section(CodegenContext*);

/*
 * Function generation.
 */

/* Emit a complete function definition. */
void _emit_function(CodegenContext*, AstFuncDecl* func);

/* Emit all method overloads for a blueprint method. */
void _emit_method(CodegenContext*, AstBlueprintDecl* bp, AstMethod* method);

/*
 * Statement generation.
 */

typedef struct {
    const char* label;
    const char* true_label;
    const char* false_label;
    const char* break_label;
    const char* continue_label;
} BlockInfo;

void _emit_block(CodegenContext*, AstBlock* block, BlockInfo* info);
void _emit_stmt(CodegenContext*, AstStmt* stmt, BlockInfo* info);
void _emit_return(CodegenContext*, AstReturn* ret);
void _emit_decl(CodegenContext*, AstDecl* decl);
void _emit_if(CodegenContext*, AstIf* if_stmt, BlockInfo* info);
void _emit_while(CodegenContext*, AstWhile* while_stmt, BlockInfo* info);
void _emit_for(CodegenContext*, AstFor* for_stmt, BlockInfo* info);
void _emit_do_while(CodegenContext*, AstDoWhile* do_while, BlockInfo* info);
void _emit_break(CodegenContext*, AstBreak* brk, BlockInfo* info);
void _emit_continue(CodegenContext*, AstContinue* cont, BlockInfo* info);

/*
 * Expression generation.
 */

typedef struct GenValue GenValue;
struct GenValue {
    const char* qbe_temp;    // QBE temporary holding the value
    const char* lvalue_addr; // Address of the variable slot
    bool is_lvalue;          // Whether the expression can be assigned to
    const char* type_suffix; // Type suffix of the value (for conversion)
    const char* name;        // Source variable name for comments
};

GenValue _emit_expr(CodegenContext*, AstExpr* expr);
GenValue _emit_literal(CodegenContext*, AstLiteral* lit);
GenValue _emit_ident(CodegenContext*, AstIdent* ident);
GenValue _emit_binary(CodegenContext*, AstBinary* bin);
GenValue _emit_unary(CodegenContext*, AstUnary* un);
GenValue _emit_call(CodegenContext*, AstCall* call);
GenValue _emit_member(CodegenContext*, AstMember* member);
GenValue _emit_index(CodegenContext*, AstIndex* idx);
GenValue _emit_init(CodegenContext*, AstInit* init);
GenValue _emit_cast(CodegenContext*, AstCast* cast);
GenValue _emit_ffi(CodegenContext*, AstFfi* ffi);

/*
 * Helper functions.
 */

const char* _alloc_temp(CodegenContext*);
const char* _alloc_label(CodegenContext*);
const char* _get_type_suffix(AstType* type);
const char* _get_string_label(CodegenContext*, const char* str);

/* Calculate stack slot offset in bytes. */
uint32_t _get_slot_offset(uint32_t slot_index);
