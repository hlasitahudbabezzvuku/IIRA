/**
 * @brief Semantic analyzer core module.
 *
 * The semantic analyzer performs type checking and analysis in 4 sequential phases:
 * 1. Register all symbols (functions, blueprints) in global scope.
 * 2. Resolve parent blueprints, flatten fields/methods, detect cycles.
 * 3. Resolve all types, compute sizes and alignments.
 * 4. Analyze function/method bodies, type-check expressions.
 *
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_semantic.h"
#include "ii_semantic_internal.h"
#include "ii_trace.h"
#include "uf_memory.h"

/*
 * Scope management.
 */

Scope* _scope_new(SemanticContext* context, Scope* parent, const char* owner)
{
    Scope* scope = uf_mem_region_zalloc(context->symbol_arena, sizeof(Scope));
    scope->parent = parent;
    scope->owner_name = owner;
    scope->symbols = uf_con_map_new();
    return scope;
}

void _scope_push(SemanticContext* context, Scope* scope)
{
    TRACE_SCOPE(context->trace);
    context->current_scope = scope;
}

void _scope_pop(SemanticContext* context)
{
    TRACE_SCOPE(context->trace);
    uf_assert(context->current_scope != nullptr);
    Scope* scope = context->current_scope;
    context->current_scope = scope->parent;
    uf_con_map_free(scope->symbols);
}

Symbol* _scope_lookup(const Scope* scope, const char* name)
{
    return uf_con_map_get(scope->symbols, name);
}

Symbol* _scope_lookup_in_chain(const Scope* scope, const char* name)
{
    const Scope* current = scope;
    while (current != nullptr) {
        Symbol* found = uf_con_map_get(current->symbols, name);
        if (found != nullptr) {
            return found;
        }
        current = current->parent;
    }
    return nullptr;
}

bool _scope_exists_in_parent(const Scope* scope, const char* name)
{
    const Scope* parent = scope->parent;
    while (parent != nullptr) {
        if (uf_con_map_get(parent->symbols, name) != nullptr) {
            return true;
        }
        parent = parent->parent;
    }
    return false;
}

Symbol* _scope_insert(SemanticContext* context, Scope* scope, const char* name, enum SymbolKind kind,
                      void* decl, AstType* type, SourceSpan span)
{
    Symbol* existing = uf_con_map_get(scope->symbols, name);
    if (existing != nullptr) {
        _diag_error(context, span, "Symbol '%s' is already defined in this scope", name);
        return nullptr;
    }

    if (_scope_exists_in_parent(scope, name)) {
        ii_diag_report(context->diag, UF_LOG_WARNING, span, "Symbol '%s' shadows declaration in parent scope",
                       name);
    }

    Symbol* symbol = uf_mem_region_zalloc(context->symbol_arena, sizeof(Symbol));
    symbol->name = name;
    symbol->kind = kind;
    symbol->decl = decl;
    symbol->type = type;
    symbol->decl_span = span;

    uf_con_map_put(scope->symbols, name, symbol);

    return symbol;
}

/*
 * Context management.
 */

SemanticContext* ii_sem_context_new(Ast* ast, DiagnosticContext* diag, TraceContext* trace_context)
{
    SemanticContext* context = uf_mem_zalloc(sizeof(SemanticContext));
    context->ast = ast;
    context->src = ii_ast_get_source(ast);
    context->diag = diag;
    context->had_error = false;
    context->trace = trace_context;
    context->symbol_arena = uf_mem_region_new(4096);
    context->global_scope = _scope_new(context, nullptr, "global");
    context->current_scope = context->global_scope;

    TRACE_SCOPE(context->trace);

    if (_likely_(!context->had_error)) {
        TRACE_LOG(context->trace, "Phase 1: Declaration analysis");
        _analyze_declarations(context);
    }

    if (_likely_(!context->had_error)) {
        TRACE_LOG(context->trace, "Phase 2: Inheritance analysis");
        _resolve_inheritance(context);
    }

    if (_likely_(!context->had_error)) {
        TRACE_LOG(context->trace, "Phase 3: Type resolution");
        _resolve_all_types(context);
    }

    if (_likely_(!context->had_error)) {
        TRACE_LOG(context->trace, "Phase 3b: Compute field offsets");
        _compute_all_field_offsets(context);
    }

    if (_likely_(!context->had_error)) {
        TRACE_LOG(context->trace, "Phase 4: Expression analysis");
        _analyze_function_bodies(context);
    }

    ii_ast_program_finalize(context->ast);

    return context;
}

void ii_sem_context_free(SemanticContext* context)
{
    if (context == nullptr) {
        return;
    }

    uf_con_map_free(context->global_scope->symbols);
    uf_mem_region_free(context->symbol_arena);
    uf_mem_free(context);
}

void ii_sem_context_freep(SemanticContext** context_ptr)
{
    if (context_ptr == nullptr || *context_ptr == nullptr) {
        return;
    }

    ii_sem_context_free(*context_ptr);
    *context_ptr = nullptr;
}
