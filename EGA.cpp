// EGA.cpp --- The Programming Language EGA
// Copyright (C) 2020 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// This file is public domain software.

#define _CRT_SECURE_NO_WARNINGS
#include "EGA.hpp"
#include <unordered_map>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdarg>

typedef std::unordered_map<std::string, fn_t> fn_map_t;
typedef std::unordered_map<std::string, arg_t> var_map_t;
static fn_map_t s_fn_map;
static var_map_t s_var_map;

#define EGA_FN

fn_t EGA_get_fn(const std::string& name);
arg_t EGA_eval_fn(const std::string& name, const args_t& args);
arg_t EGA_eval_var(const std::string& name);
arg_t EGA_eval_program(const args_t& args);
arg_t EGA_eval_arg(arg_t ast, bool do_check = false);

//////////////////////////////////////////////////////////////////////////////

// Is the specified character valid for the first character of the identifier?
inline bool is_ident_fchar(char ch)
{
    return std::isalpha(ch) || std::strchr("_+-[]<>=!~*&|%^?:", ch) != NULL;
}

// Is the specified character valid for the second character of the identifier?
inline bool is_ident_char(char ch)
{
    return std::isalnum(ch) || std::strchr("_+-[]<>=!~*&|%^?:", ch) != NULL;
}

/*static*/ int Token::s_alive_count = 0;

/*static*/ void Token::alive_count(bool add)
{
    if (add)
    {
        assert(s_alive_count >= 0);
        ++s_alive_count;
    }
    else
    {
        --s_alive_count;
        assert(s_alive_count >= 0);
    }
}

/*static*/ int AstBase::s_alive_count = 0;

/*static*/ void AstBase::alive_count(bool add)
{
    if (add)
    {
        assert(s_alive_count >= 0);
        ++s_alive_count;
    }
    else
    {
        --s_alive_count;
        assert(s_alive_count >= 0);
    }
}

//////////////////////////////////////////////////////////////////////////////
// Dumping

void EGA_default_print(const char *fmt, va_list va)
{
    vprintf(fmt, va);
}

static EGA_PRINT_FN s_print_fn = EGA_default_print;

void EGA_set_print_fn(EGA_PRINT_FN fn)
{
    s_print_fn = fn;
}

void EGA_do_print(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    s_print_fn(fmt, va);
    va_end(va);
}

void Token::print() const
{
    EGA_do_print("%s", dump().c_str());
    fflush(stdout);
}

void TokenStream::print() const
{
    EGA_do_print("%s", dump().c_str());
    fflush(stdout);
}

void AstBase::print() const
{
    EGA_do_print("%s\n", dump(true).c_str());
}

std::string EGA_dump_token_type(TokenType type)
{
    switch (type)
    {
    case TOK_EOF: return "TOK_EOF";
    case TOK_INT: return "TOK_INT";
    case TOK_STR: return "TOK_STR";
    case TOK_IDENT: return "TOK_IDENT";
    case TOK_SYMBOL: return "TOK_SYMBOL";
    }
    return "TOK_broken";
}

std::string Token::dump() const
{
    std::string ret = "(";
    ret += EGA_dump_token_type(m_type);
    ret += ", ";
    ret += mstr_to_string(m_lineno);
    ret += ", '";
    ret += m_str;
    ret += "', ";
    ret += mstr_to_string(m_int);
    ret += ")";
    return ret;
}

std::string TokenStream::dump() const
{
    std::string ret = "(";
    if (size() > 0)
    {
        if (m_index == 0)
            ret += "(*) ";

        ret += m_tokens[0]->dump();
        for (size_t i = 1; i < size(); ++i)
        {
            ret += ", ";

            if (m_index == i)
                ret += "(*) ";

            ret += m_tokens[i]->dump();
        }
    }
    ret += ")";
    return ret;
}

std::string EGA_dump_ast_type(AstType type)
{
    switch (type)
    {
    case AST_INT: return "AST_INT";
    case AST_STR: return "AST_STR";
    case AST_ARRAY: return "AST_ARRAY";
    case AST_VAR: return "AST_VAR";
    case AST_CALL: return "AST_CALL";
    case AST_PROGRAM: return "AST_PROGRAM";
    }
    return "(AST_none)";
}

//////////////////////////////////////////////////////////////////////////////
// TokenStream

arg_t TokenStream::do_parse()
{
    return visit_translation_unit();
}

bool TokenStream::do_lexical(const char *input)
{
    int lineno = 1;
    const char *pch = input;

    for (; *pch; ++pch)
    {
        if (*pch == '\n')
            ++lineno;

        if (std::isspace(*pch))
        {
            continue;
        }

        std::string str;
        if (is_ident_fchar(*pch))
        {
            str += *pch;
            for (;;)
            {
                ++pch;
                if (!*pch || !is_ident_char(*pch))
                    break;
                str += *pch;
            }

            add(TOK_IDENT, lineno, str);
            --pch;
            continue;
        }

        if (std::isdigit(*pch))
        {
            str += *pch;
            ++pch;
            for (;;)
            {
                if (!std::isdigit(*pch))
                    break;
                str += *pch;
                ++pch;
            }
            add(TOK_INT, lineno, str);
            --pch;
            continue;
        }

        switch (*pch)
        {
        case '"':
            ++pch;
            for (;;)
            {
                if (*pch == '"')
                {
                    if (pch[1] == '"')
                    {
                        str += *pch;
                        ++pch;
                        ++pch;
                        continue;
                    }
                    else
                    {
                        break;
                    }
                }
                str += *pch;
                ++pch;
            }
            add(TOK_STR, lineno, str);
            continue;

        case '(':
        case ')':
        case ',':
        case '{':
        case '}':
        case ';':
            str += *pch;
            add(TOK_SYMBOL, lineno, str);
            continue;
        }

        if (*pch == 0x7F) // EOF
            break;

        EGA_do_print("ERROR: invalid character '%c' (%u)\n", *pch, (*pch & 0xFF));
        m_error = -1;
        return false;
    }

    add(TOK_EOF, lineno, "");

    m_error = 0;
    return true;
}

//////////////////////////////////////////////////////////////////////////////
// parser

#if defined(NDEBUG) || 1
    #define PARSE_DEBUG()
#else
    #define PARSE_DEBUG() do { puts(__func__); EGA_print_stream(*this); } while (0)
#endif

void EGA_print_stream(TokenStream& stream)
{
    stream.print();
    puts("");
    fflush(stdout);
}

arg_t TokenStream::visit_translation_unit()
{
    PARSE_DEBUG();

    auto call = make_arg<AstContainer>(AST_PROGRAM, "program");

    for (;;)
    {
        if (token_type() == TOK_EOF)
            return call;

        if (auto expr = visit_expression())
        {
            call->add(expr);
            if (token_type() == TOK_SYMBOL && token_str() == ";")
            {
                go_next();
                if (token_type() == TOK_EOF)
                    return call;
                continue;
            }
            continue;
        }

        EGA_do_print("ERROR: unexpected token (2): '%s'\n", token_str().c_str());
        return NULL;
    }
}

arg_t TokenStream::visit_expression()
{
    PARSE_DEBUG();

    char ch;
    std::string name;

    switch (token_type())
    {
    case TOK_EOF:
        return NULL;

    case TOK_INT:
        return visit_integer_literal();

    case TOK_STR:
        return visit_string_literal();

    case TOK_IDENT:
        name = token()->get_str();
        if (EGA_get_fn(name))
        {
            go_next();
            return visit_call(name);
        }
        else
        {
            auto var = make_arg<AstVar>(name);
            go_next();
            if (token()->get_str() == "(")
                throw EGA_syntax_error();
            return var;
        }

    case TOK_SYMBOL:
        ch = token()->get_str()[0];
        switch (ch)
        {
        case '(':
            return visit_call("");

        case ')':
            break;

        case ',':
            break;

        case '{':
            return visit_array_literal();

        case '}':
            break;

        case ';':
            break;

        default:
            break;
        }
        break;

    default:
        break;
    }

    return NULL;
}

arg_t TokenStream::visit_integer_literal()
{
    PARSE_DEBUG();

    if (token_type() != TOK_INT)
        return NULL;

    auto ai = make_arg<AstInt>(token()->get_int());
    go_next();
    return ai;
}

arg_t TokenStream::visit_string_literal()
{
    PARSE_DEBUG();

    if (token_type() != TOK_STR)
        return NULL;
    auto as = make_arg<AstStr>(token()->get_str());
    go_next();
    return as;
}

arg_t TokenStream::visit_array_literal()
{
    PARSE_DEBUG();

    if (token_type() != TOK_SYMBOL || token_str() != "{")
        return NULL;

    go_next();

    if (token_type() == TOK_SYMBOL && token_str() == "}")
    {
        go_next();
        return make_arg<AstContainer>(AST_ARRAY);
    }

    if (auto list = visit_expression_list(AST_ARRAY, "array"))
    {
        if (token_type() == TOK_SYMBOL && token_str() == "}")
        {
            go_next();
            return list;
        }

        EGA_do_print("ERROR: unexpected token (3): '%s'\n", token_str().c_str());
    }

    return NULL;
}

arg_t TokenStream::visit_call(const std::string& name)
{
    PARSE_DEBUG();

    if (token_type() != TOK_SYMBOL || token_str() != "(")
        return NULL;

    go_next();

    auto list = make_arg<AstContainer>(AST_CALL, name);

    if (token_type() == TOK_SYMBOL)
    {
        if (token_str() == ")")
        {
            go_next();
            return list;
        }
    }

    if (auto expr = visit_expression())
    {
        list->add(expr);
    }
    else
    {
        return NULL;
    }

    for (;;)
    {
        if (token_str() == ")")
        {
            go_next();
            break;
        }
        else if (token_str() != ",")
        {
            return NULL;
        }
        go_next();

        if (auto expr = visit_expression())
        {
            list->add(expr);
        }
        else
        {
            break;
        }
    }

    return list;
}

arg_t TokenStream::visit_expression_list(AstType type, const std::string& name)
{
    PARSE_DEBUG();

    size_t index = get_index();

    auto expr = visit_expression();
    if (!expr)
        return NULL;

    auto list = make_arg<AstContainer>(type, name);
    list->add(expr);

    for (;;)
    {
        if (token_type() == TOK_SYMBOL)
        {
            if (token_str() == ",")
            {
                go_next();
                continue;
            }

            if (token_str() == ")" || token_str() == "}")
            {
                break;
            }
        }

        expr = visit_expression();
        if (!expr)
        {
            get_index() = index;
            return NULL;
        }
        list->add(expr);
    }

    return list;
}

//////////////////////////////////////////////////////////////////////////////
// Evaluation

#if defined(NDEBUG) || 1
    #define EVAL_DEBUG()
#else
    #define EVAL_DEBUG() do { puts(__func__); fflush(stdout); } while (0)
#endif

arg_t AstVar::eval() const
{
    return EGA_eval_var(m_name);
}

arg_t AstContainer::eval() const
{
    switch (m_type)
    {
    case AST_ARRAY:
        if (auto ret = make_arg<AstContainer>(AST_ARRAY, "array"))
        {
            for (size_t i = 0; i < size(); ++i)
            {
                ret->add(m_children[i]->eval());
            }
            return ret;
        }
        break;

    case AST_CALL:
        return EGA_eval_fn(m_str, m_children);

    case AST_PROGRAM:
        return EGA_eval_program(m_children);

    default:
        assert(0);
        break;
    }

    return NULL;
}


fn_t EGA_get_fn(const std::string& name)
{
    EVAL_DEBUG();
    fn_map_t::iterator it = s_fn_map.find(name);
    if (it == s_fn_map.end())
        return NULL;
    return it->second;
}

bool
EGA_add_fn(const std::string& name, size_t min_args, size_t max_args,
           EGA_PROC proc, const std::string& help)
{
    auto fn = std::make_shared<EGA_FUNCTION>(name, min_args, max_args, proc, help);
    s_fn_map[name] = fn;
    return true;
}

arg_t EGA_eval_var(const std::string& name)
{
    EVAL_DEBUG();

    var_map_t::iterator it = s_var_map.find(name);
    if (it == s_var_map.end() || !it->second)
        throw EGA_undefined_variable(name);

    return it->second->eval();
}

arg_t
EGA_eval_program(const args_t& args)
{
    arg_t arg;

    for (size_t i = 0; i < args.size(); ++i)
    {
        arg = args[i]->eval();
    }

    return arg;
}

arg_t
EGA_eval_fn(const std::string& name, const args_t& args)
{
    EVAL_DEBUG();
    if (name.size())
    {
        if (auto fn = EGA_get_fn(name))
        {
            if (fn->min_args <= args.size() && args.size() <= fn->max_args)
                return (*(fn->proc))(args);
            else
                throw EGA_argument_number_exception();
        }
    }
    else
    {
        return EGA_eval_program(args);
    }
    return NULL;
}

arg_t EGA_eval_arg(arg_t ast, bool do_check)
{
    EVAL_DEBUG();
    if (!ast)
        throw EGA_syntax_error();
    auto ret = ast->eval();
    if (!ret && do_check)
        throw EGA_illegal_operation();
    return ret;
}

void EGA_eval_text(const char *text)
{
    TokenStream stream;
    if (!stream.do_lexical(text))
        throw EGA_syntax_error();

    auto ast = stream.do_parse();
    if (!ast)
        throw EGA_syntax_error();

    auto evaled = EGA_eval_arg(ast);
    if (evaled)
    {
        evaled->print();
    }
}

void EGA_eval_text_ex(const char *text)
{
    try
    {
        EGA_eval_text(text);
    }
    catch (EGA_exit_exception& e)
    {
        if (e.m_arg)
        {
            if (auto evaled = EGA_eval_arg(e.m_arg))
            {
                evaled->print();
            }
        }
    }
    catch (EGA_exception& e)
    {
        EGA_do_print("ERROR: %s\n", e.what());
    }
}

int EGA_int(arg_t ast)
{
    EVAL_DEBUG();
    if (ast->get_type() != AST_INT)
        throw EGA_type_mismatch();
    return std::static_pointer_cast<AstInt>(ast)->get_int();
}

std::shared_ptr<AstArray> EGA_array(arg_t ast)
{
    EVAL_DEBUG();
    if (ast->get_type() != AST_ARRAY)
        throw EGA_type_mismatch();
    return std::static_pointer_cast<AstArray>(ast);
}

std::string EGA_str(arg_t ast)
{
    EVAL_DEBUG();
    if (ast->get_type() != AST_STR)
        throw EGA_type_mismatch();
    return std::static_pointer_cast<AstStr>(ast)->get_str();
}

std::shared_ptr<AstInt>
EGA_compare_0(arg_t a1, arg_t a2)
{
    EVAL_DEBUG();

    auto ast1 = EGA_eval_arg(a1, true);
    auto ast2 = EGA_eval_arg(a2, true);

    if (ast1->get_type() < ast2->get_type())
    {
        return make_arg<AstInt>(-1);
    }

    if (ast1->get_type() > ast2->get_type())
    {
        return make_arg<AstInt>(1);
    }

    switch (ast1->get_type())
    {
    case AST_ARRAY:
        {
            auto array1 = std::static_pointer_cast<AstContainer>(ast1);
            auto array2 = std::static_pointer_cast<AstContainer>(ast2);
            size_t size = std::min(array1->size(), array2->size());
            for (size_t i = 0; i < size; ++i)
            {
                auto ai = EGA_compare_0((*array1)[i], (*array2)[i]);
                if (ai == NULL)
                {
                    return NULL;
                }
                if (EGA_int(ai) != 0)
                {
                    return ai;
                }
            }
            if (array1->size() < array2->size())
            {
                return make_arg<AstInt>(-1);
            }
            if (array1->size() > array2->size())
            {
                return make_arg<AstInt>(1);
            }
            return make_arg<AstInt>(0);
        }
    case AST_INT:
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            if (i1 < i2)
                return make_arg<AstInt>(-1);
            if (i1 > i2)
                return make_arg<AstInt>(1);
            return make_arg<AstInt>(0);
        }
    case AST_STR:
        {
            std::string str1 = EGA_str(ast1);
            std::string str2 = EGA_str(ast2);
            if (str1 < str2)
                return make_arg<AstInt>(-1);
            if (str1 > str2)
                return make_arg<AstInt>(1);
            return make_arg<AstInt>(0);
        }
    default:
        throw EGA_type_mismatch();
    }

    return NULL;
}

arg_t EGA_FN EGA_compare(const args_t& args)
{
    EVAL_DEBUG();

    if (auto ai = EGA_compare_0(args[0], args[1]))
    {
        return ai;
    }
    return NULL;
}

arg_t EGA_FN EGA_less(const args_t& args)
{
    EVAL_DEBUG();
    if (auto ai = EGA_compare_0(args[0], args[1]))
    {
        ai->get_int() = (ai->get_int() < 0);
        return ai;
    }
    return NULL;
}

arg_t EGA_FN EGA_greater(const args_t& args)
{
    EVAL_DEBUG();
    if (auto ai = EGA_compare_0(args[0], args[1]))
    {
        ai->get_int() = (ai->get_int() > 0);
        return ai;
    }
    return NULL;
}

arg_t EGA_FN EGA_less_equal(const args_t& args)
{
    EVAL_DEBUG();
    if (auto ai = EGA_compare_0(args[0], args[1]))
    {
        ai->get_int() = (ai->get_int() <= 0);
        return ai;
    }
    return NULL;
}

arg_t EGA_FN EGA_greater_equal(const args_t& args)
{
    EVAL_DEBUG();
    if (auto ai = EGA_compare_0(args[0], args[1]))
    {
        ai->get_int() = (ai->get_int() >= 0);
        return ai;
    }
    return NULL;
}

arg_t EGA_FN EGA_equal(const args_t& args)
{
    EVAL_DEBUG();
    if (auto ai = EGA_compare_0(args[0], args[1]))
    {
        ai->get_int() = (ai->get_int() == 0);
        return ai;
    }
    return NULL;
}

arg_t EGA_FN EGA_not_equal(const args_t& args)
{
    EVAL_DEBUG();
    if (auto ai = EGA_compare_0(args[0], args[1]))
    {
        ai->get_int() = (ai->get_int() != 0);
        return ai;
    }
    return NULL;
}

arg_t EGA_FN EGA_print(const args_t& args)
{
    EVAL_DEBUG();

    for (size_t i = 0; i < args.size(); ++i)
    {
        if (auto ast = EGA_eval_arg(args[i]))
        {
            EGA_do_print("%s", ast->dump(false).c_str());
        }
    }
    return NULL;
}

arg_t EGA_FN EGA_println(const args_t& args)
{
    EVAL_DEBUG();

    EGA_print(args);
    EGA_do_print("\n");
    return NULL;
}

arg_t EGA_FN EGA_dump(const args_t& args)
{
    EVAL_DEBUG();

    for (size_t i = 0; i < args.size(); ++i)
    {
        if (auto ast = EGA_eval_arg(args[i]))
        {
            EGA_do_print("%s", ast->dump(true).c_str());
        }
    }
    return NULL;
}

arg_t EGA_FN EGA_dumpln(const args_t& args)
{
    EVAL_DEBUG();

    EGA_dump(args);
    EGA_do_print("\n");
    return NULL;
}

arg_t EGA_FN EGA_len(const args_t& args)
{
    EVAL_DEBUG();
    if (auto ast1 = EGA_eval_arg(args[0], true))
    {
        switch (ast1->get_type())
        {
        case AST_STR:
            {
                int len = int(EGA_str(ast1).size());
                return make_arg<AstInt>(len);
            }

        case AST_ARRAY:
            {
                int len = int(EGA_array(ast1)->size());
                return make_arg<AstInt>(len);
            }

        default:
            throw EGA_type_mismatch();
        }
    }
    return NULL;
}

arg_t EGA_FN EGA_cat(const args_t& args)
{
    EVAL_DEBUG();

    if (auto ast1 = EGA_eval_arg(args[0], true))
    {
        switch (ast1->get_type())
        {
        case AST_STR:
            {
                std::string str = EGA_str(ast1);
                for (size_t i = 1; i < args.size(); ++i)
                {
                    ast1 = args[i]->eval();
                    str += EGA_str(ast1);
                }
                return make_arg<AstStr>(str);
            }

        case AST_ARRAY:
            if (auto array = make_arg<AstArray>(AST_ARRAY))
            {
                for (size_t i = 0; i < args.size(); ++i)
                {
                    if (auto array2 = std::static_pointer_cast<AstArray>(args[i]))
                    {
                        for (size_t k = 0; k < array2->size(); ++k)
                        {
                            array->add((*array2)[k]->eval());
                        }
                    }
                }
                return array;
            }
            break;

        default:
            throw EGA_type_mismatch();
        }
    }

    return NULL;
}

arg_t EGA_FN EGA_plus(const args_t& args)
{
    EVAL_DEBUG();

    if (auto ast1 = EGA_eval_arg(args[0], true))
    {
        if (auto ast2 = EGA_eval_arg(args[1], true))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            return make_arg<AstInt>(i1 + i2);
        }
    }
    return NULL;
}

arg_t EGA_FN EGA_minus(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() == 1)
    {
        if (auto ast1 = EGA_eval_arg(args[0], true))
        {
            int i1 = EGA_int(ast1);
            return make_arg<AstInt>(-i1);
        }
        return NULL;
    }

    if (args.size() == 2)
    {
        if (auto ast1 = EGA_eval_arg(args[0], true))
        {
            if (auto ast2 = EGA_eval_arg(args[1], true))
            {
                int i1 = EGA_int(ast1);
                int i2 = EGA_int(ast2);
                return make_arg<AstInt>(i1 - i2);
            }
        }
    }

    throw EGA_argument_number_exception();
}

arg_t EGA_FN EGA_mul(const args_t& args)
{
    EVAL_DEBUG();

    if (auto ast1 = EGA_eval_arg(args[0], true))
    {
        if (auto ast2 = EGA_eval_arg(args[1], true))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            return make_arg<AstInt>(i1 * i2);
        }
    }

    return NULL;
}

arg_t EGA_FN EGA_div(const args_t& args)
{
    EVAL_DEBUG();

    if (auto ast1 = EGA_eval_arg(args[0], true))
    {
        if (auto ast2 = EGA_eval_arg(args[1], true))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            return make_arg<AstInt>(i1 / i2);
        }
    }

    return NULL;
}

arg_t EGA_FN EGA_mod(const args_t& args)
{
    EVAL_DEBUG();

    if (auto ast1 = EGA_eval_arg(args[0], true))
    {
        if (auto ast2 = EGA_eval_arg(args[1], true))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            return make_arg<AstInt>(i1 % i2);
        }
    }

    return NULL;
}

arg_t EGA_FN EGA_if(const args_t& args)
{
    EVAL_DEBUG();

    if (auto ast1 = EGA_eval_arg(args[0], true))
    {
        int i1 = EGA_int(ast1);
        if (i1)
        {
            if (auto ast2 = EGA_eval_arg(args[1]))
            {
                return ast2;
            }
        }
        else if (args.size() == 3)
        {
            if (auto ast3 = EGA_eval_arg(args[2]))
            {
                return ast3;
            }
        }
    }

    return NULL;
}

void EGA_set_var(const std::string& name, arg_t arg)
{
    if (arg)
        s_var_map[name] = arg;
    else
        s_var_map.erase(name);
}

arg_t EGA_FN EGA_set(const args_t& args)
{
    EVAL_DEBUG();

    if (args[0]->get_type() != AST_VAR)
        throw EGA_type_mismatch();

    std::string name = std::static_pointer_cast<AstVar>(args[0])->get_name();

    if (args.size() == 2)
    {
        auto value = args[1]->eval();
        EGA_set_var(name, value);
        return value;
    }
    else
    {
        EGA_set_var(name, NULL);
        return NULL;
    }
}

arg_t EGA_FN EGA_define(const args_t& args)
{
    EVAL_DEBUG();

    if (args[0]->get_type() != AST_VAR)
        throw EGA_type_mismatch();

    std::string name = std::static_pointer_cast<AstVar>(args[0])->get_name();

    if (args.size() == 2)
    {
        auto expr = args[1]->clone();
        EGA_set_var(name, expr);
        return expr;
    }
    else
    {
        EGA_set_var(name, NULL);
        return NULL;
    }
}

arg_t EGA_FN EGA_for(const args_t& args)
{
    EVAL_DEBUG();

    if (args[0]->get_type() != AST_VAR)
        throw EGA_type_mismatch();

    arg_t arg;
    if (auto ast1 = EGA_eval_arg(args[1], true))
    {
        if (auto ast2 = EGA_eval_arg(args[2], true))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);

            for (int i = i1; i <= i2; ++i)
            {
                auto ai = make_arg<AstInt>(i);
                auto var = std::static_pointer_cast<AstVar>(args[0]);
                EGA_set_var(var->get_name(), ai);

                try
                {
                    arg = EGA_eval_arg(args[3]);
                }
                catch (EGA_break_exception&)
                {
                    break;
                }
            }
        }
    }

    return arg;
}

arg_t EGA_FN EGA_foreach(const args_t& args)
{
    EVAL_DEBUG();

    if (args[0]->get_type() != AST_VAR)
        throw EGA_type_mismatch();

    arg_t arg;
    if (auto var = std::static_pointer_cast<AstVar>(args[0]))
    {
        if (auto ast = EGA_eval_arg(args[1], true))
        {
            if (auto array = EGA_array(ast))
            {
                for (size_t i = 0; i < array->size(); ++i)
                {
                    EGA_set_var(var->get_name(), (*array)[i]);

                    try
                    {
                        arg = EGA_eval_arg(args[2]);
                    }
                    catch (EGA_break_exception&)
                    {
                        break;
                    }
                }
            }
        }
    }

    return arg;
}

arg_t EGA_FN EGA_while(const args_t& args)
{
    EVAL_DEBUG();

    arg_t arg;
    for (;;)
    {
        auto ast1 = EGA_eval_arg(args[0], true);
        if (ast1)
        {
            int i1 = EGA_int(ast1);
            if (!i1)
                break;
        }

        try
        {
            arg = EGA_eval_arg(args[1]);
        }
        catch (EGA_break_exception&)
        {
            break;
        }
    }

    return arg;
}

arg_t EGA_FN EGA_do(const args_t& args)
{
    EVAL_DEBUG();

    arg_t arg;
    for (size_t i = 0; i < args.size(); ++i)
    {
        try
        {
            arg = EGA_eval_arg(args[i]);
        }
        catch (EGA_break_exception&)
        {
            break;
        }
    }

    return arg;
}

arg_t EGA_FN EGA_exit(const args_t& args)
{
    EVAL_DEBUG();
    if (args.size() == 1)
        throw EGA_exit_exception(args[0]);
    else
        throw EGA_exit_exception(NULL);
}

arg_t EGA_FN EGA_break(const args_t& args)
{
    EVAL_DEBUG();
    throw EGA_break_exception();
}

arg_t EGA_FN EGA_at(const args_t& args)
{
    EVAL_DEBUG();

    if (auto ast1 = EGA_eval_arg(args[0], true))
    {
        if (auto ast2 = EGA_eval_arg(args[1], true))
        {
            switch (ast1->get_type())
            {
            case AST_ARRAY:
                if (auto array = EGA_array(ast1))
                {
                    size_t index = EGA_int(ast2);
                    if (index < array->size())
                    {
                        auto base = (*array)[index]->eval();
                        return base;
                    }
                }
                break;
            case AST_STR:
                {
                    std::string str = EGA_str(ast1);
                    size_t index = EGA_int(ast2);
                    if (index < str.size())
                    {
                        return make_arg<AstInt>(str[index]);
                    }
                }
                break;
            default:
                break;
            }
        }
    }

    return NULL;
}

arg_t EGA_FN EGA_not(const args_t& args)
{
    EVAL_DEBUG();

    if (auto ast1 = EGA_eval_arg(args[0], true))
    {
        int i = EGA_int(ast1);
        return make_arg<AstInt>(!i);
    }

    return NULL;
}

arg_t EGA_FN EGA_or(const args_t& args)
{
    EVAL_DEBUG();

    if (auto ast1 = EGA_eval_arg(args[0], true))
    {
        if (auto ast2 = EGA_eval_arg(args[1], true))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            return make_arg<AstInt>(i1 || i2);
        }
    }

    return NULL;
}

arg_t EGA_FN EGA_and(const args_t& args)
{
    EVAL_DEBUG();

    if (auto ast1 = EGA_eval_arg(args[0], true))
    {
        if (auto ast2 = EGA_eval_arg(args[1], true))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            return make_arg<AstInt>(i1 && i2);
        }
    }

    return NULL;
}

arg_t EGA_FN EGA_compl(const args_t& args)
{
    EVAL_DEBUG();

    if (auto ast1 = EGA_eval_arg(args[0], true))
    {
        int i = EGA_int(ast1);
        return make_arg<AstInt>(~i);
    }

    return NULL;
}

arg_t EGA_FN EGA_bitor(const args_t& args)
{
    EVAL_DEBUG();

    if (auto ast1 = EGA_eval_arg(args[0], true))
    {
        if (auto ast2 = EGA_eval_arg(args[1], true))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            return make_arg<AstInt>(i1 | i2);
        }
    }

    return NULL;
}

arg_t EGA_FN EGA_bitand(const args_t& args)
{
    EVAL_DEBUG();

    if (auto ast1 = EGA_eval_arg(args[0], true))
    {
        if (auto ast2 = EGA_eval_arg(args[1], true))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            return make_arg<AstInt>(i1 & i2);
        }
    }

    return NULL;
}

arg_t EGA_FN EGA_xor(const args_t& args)
{
    EVAL_DEBUG();

    if (auto ast1 = EGA_eval_arg(args[0], true))
    {
        if (auto ast2 = EGA_eval_arg(args[1], true))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            return make_arg<AstInt>(i1 ^ i2);
        }
    }

    return NULL;
}

arg_t EGA_FN EGA_left(const args_t& args)
{
    EVAL_DEBUG();

    if (auto ast1 = EGA_eval_arg(args[0], true))
    {
        if (auto ast2 = EGA_eval_arg(args[1], true))
        {
            size_t i2 = EGA_int(ast2);
            switch (ast1->get_type())
            {
            case AST_STR:
                {
                    std::string str = EGA_str(ast1);
                    if (i2 <= str.size())
                    {
                        std::string str2 = str.substr(0, i2);
                        return make_arg<AstStr>(str2);
                    }
                    else
                        throw EGA_index_out_of_range();
                }
                break;
            case AST_ARRAY:
                {
                    auto array1 = make_arg<AstArray>(AST_ARRAY);
                    auto array2 = EGA_array(ast1);
                    if (i2 <= array2->size())
                    {
                        for (size_t i = 0; i < i2; ++i)
                        {
                            array1->add((*array2)[i]->clone());
                        }
                        return array1;
                    }
                    else
                        throw EGA_index_out_of_range();
                }
                break;
            default:
                throw EGA_type_mismatch();
            }
        }
    }

    return NULL;
}

arg_t EGA_FN EGA_right(const args_t& args)
{
    EVAL_DEBUG();

    if (auto ast1 = EGA_eval_arg(args[0], true))
    {
        if (auto ast2 = EGA_eval_arg(args[1], true))
        {
            size_t i2 = EGA_int(ast2);
            switch (ast1->get_type())
            {
            case AST_STR:
                {
                    std::string str = EGA_str(ast1);
                    if (i2 <= str.size())
                    {
                        std::string str2 = str.substr(str.size() - i2, i2);
                        return make_arg<AstStr>(str2);
                    }
                    else
                        throw EGA_index_out_of_range();
                }
                break;
            case AST_ARRAY:
                {
                    auto array1 = make_arg<AstArray>(AST_ARRAY);
                    auto array2 = EGA_array(ast1);
                    if (i2 <= array2->size())
                    {
                        size_t k1 = array2->size() - i2;
                        size_t k2 = array2->size();
                        for (size_t i = k1; i < k2; ++i)
                        {
                            array1->add((*array2)[i]->clone());
                        }
                        return array1;
                    }
                    else
                        throw EGA_index_out_of_range();
                }
                break;
            default:
                throw EGA_type_mismatch();
            }
        }
    }

    return NULL;
}

arg_t EGA_FN EGA_mid(const args_t& args)
{
    EVAL_DEBUG();

    if (auto ast1 = EGA_eval_arg(args[0], true))
    {
        if (auto ast2 = EGA_eval_arg(args[1], true))
        {
            if (auto ast3 = EGA_eval_arg(args[2], true))
            {
                size_t i2 = EGA_int(ast2);
                size_t i3 = EGA_int(ast3);
                switch (ast1->get_type())
                {
                case AST_STR:
                    {
                        std::string str = EGA_str(ast1);
                        if (i2 <= str.size() && i2 + i3 <= str.size())
                        {
                            std::string str2 = str.substr(i2, i3);
                            return make_arg<AstStr>(str2);
                        }
                        else
                            throw EGA_index_out_of_range();
                    }
                    break;
                case AST_ARRAY:
                    {
                        auto array1 = make_arg<AstArray>(AST_ARRAY);
                        auto array2 = EGA_array(ast1);
                        if (i2 <= array2->size() && i2 + i3 <= array2->size())
                        {
                            size_t k1 = array2->size() - i2;
                            size_t k2 = k1 + i3;
                            for (size_t i = k1; i < k2; ++i)
                            {
                                array1->add((*array2)[i]->clone());
                            }
                            return array1;
                        }
                        else
                            throw EGA_index_out_of_range();
                    }
                    break;
                default:
                    throw EGA_type_mismatch();
                }
            }
        }
    }

    return NULL;
}

bool EGA_init(void)
{
    // assignment
    EGA_add_fn("set", 1, 2, EGA_set, "set(var[, value])");
    EGA_add_fn("=", 1, 2, EGA_set, "set(var[, value])");
    EGA_add_fn("define", 1, 2, EGA_define, "define(var[, expr])");
    EGA_add_fn(":=", 1, 2, EGA_define, "define(var[, expr])");

    // control structure
    EGA_add_fn("if", 2, 3, EGA_if, "if(cond, true_case[, false_case])");
    EGA_add_fn("?:", 2, 3, EGA_if, "if(cond, true_case[, false_case])");
    EGA_add_fn("for", 4, 4, EGA_for, "for(var, min, max, expr)");
    EGA_add_fn("foreach", 3, 3, EGA_foreach, "foreach(var, array, expr)");
    EGA_add_fn("while", 2, 2, EGA_while, "while(cond, expr)");
    EGA_add_fn("do", 0, 256, EGA_do, "do(expr, ...)");
    EGA_add_fn("exit", 0, 1, EGA_exit, "exit([value])");
    EGA_add_fn("break", 0, 0, EGA_break, "break()");

    // comparison
    EGA_add_fn("equal", 2, 2, EGA_equal, "equal(value1, value2)");
    EGA_add_fn("==", 2, 2, EGA_equal, "equal(value1, value2)");
    EGA_add_fn("not_equal", 2, 2, EGA_not_equal, "not_equal(value1, value2)");
    EGA_add_fn("!=", 2, 2, EGA_not_equal, "not_equal(value1, value2)");
    EGA_add_fn("compare", 2, 2, EGA_compare, "compare(value1, value2)");
    EGA_add_fn("less", 2, 2, EGA_less, "less(value1, value2)");
    EGA_add_fn("<", 2, 2, EGA_less, "less(value1, value2)");
    EGA_add_fn("less_equal", 2, 2, EGA_less_equal, "less_equal(value1, value2)");
    EGA_add_fn("<=", 2, 2, EGA_less_equal, "less_equal(value1, value2)");
    EGA_add_fn("greater", 2, 2, EGA_greater, "greater(value1, value2)");
    EGA_add_fn(">", 2, 2, EGA_greater, "greater(value1, value2)");
    EGA_add_fn("greater_equal", 2, 2, EGA_greater_equal, "greater_equal(value1, value2)");
    EGA_add_fn(">=", 2, 2, EGA_greater_equal, "greater_equal(value1, value2)");

    // print
    EGA_add_fn("print", 0, 256, EGA_print, "print(value, ...)");
    EGA_add_fn("println", 0, 256, EGA_println, "println(value, ...)");
    EGA_add_fn("dump", 0, 256, EGA_dump, "dump(value, ...)");
    EGA_add_fn("dumpln", 0, 256, EGA_dumpln, "dumpln(value, ...)");
    EGA_add_fn("?", 0, 256, EGA_dumpln, "dumpln(value, ...)");

    // arithmetic
    EGA_add_fn("plus", 2, 2, EGA_plus, "plus(int1, int2)");
    EGA_add_fn("+", 2, 2, EGA_plus, "plus(int1, int2)");
    EGA_add_fn("minus", 1, 2, EGA_minus, "minus(int1[, int2])");
    EGA_add_fn("-", 1, 2, EGA_minus, "minus(int1[, int2])");
    EGA_add_fn("mul", 2, 2, EGA_mul, "mul(int1, int2)");
    EGA_add_fn("*", 2, 2, EGA_mul, "mul(int1, int2)");
    EGA_add_fn("div", 2, 2, EGA_div, "div(int1, int2)");
    EGA_add_fn("/", 2, 2, EGA_div, "div(int1, int2)");
    EGA_add_fn("mod", 2, 2, EGA_mod, "mod(int1, int2)");
    EGA_add_fn("%", 2, 2, EGA_mod, "mod(int1, int2)");

    // logical
    EGA_add_fn("not", 1, 1, EGA_not, "not(value)");
    EGA_add_fn("!", 1, 1, EGA_not, "not(value)");
    EGA_add_fn("or", 2, 2, EGA_or, "or(value1, value2)");
    EGA_add_fn("||", 2, 2, EGA_or, "or(value1, value2)");
    EGA_add_fn("and", 2, 2, EGA_and, "and(value1, value2)");
    EGA_add_fn("&&", 2, 2, EGA_and, "and(value1, value2)");

    // bit operation
    EGA_add_fn("compl", 1, 1, EGA_compl, "compl(value)");
    EGA_add_fn("~", 1, 1, EGA_compl, "compl(value)");
    EGA_add_fn("bitor", 2, 2, EGA_bitor, "bitor(value1, value2)");
    EGA_add_fn("|", 2, 2, EGA_bitor, "bitor(value1, value2)");
    EGA_add_fn("bitand", 2, 2, EGA_bitand, "bitand(value1, value2)");
    EGA_add_fn("&", 2, 2, EGA_bitand, "bitand(value1, value2)");
    EGA_add_fn("xor", 2, 2, EGA_xor, "xor(value1, value2)");
    EGA_add_fn("^", 2, 2, EGA_xor, "xor(value1, value2)");

    // array/string manipulation
    EGA_add_fn("len", 1, 1, EGA_len, "len(array_or_string)");
    EGA_add_fn("cat", 1, 256, EGA_cat, "len(array_or_string_1, array_or_string_2, ...)");
    EGA_add_fn("[]", 2, 2, EGA_at, "at(array_or_string, index)");
    EGA_add_fn("at", 2, 2, EGA_at, "at(array_or_string, index)");
    EGA_add_fn("left", 2, 2, EGA_left, "left(array_or_string, count)");
    EGA_add_fn("right", 2, 2, EGA_right, "right(array_or_string, count)");
    EGA_add_fn("mid", 3, 3, EGA_mid, "mid(array_or_string, index, count)");

    return true;
}

void
EGA_uninit(void)
{
    s_fn_map.clear();

    s_var_map.clear();
}

//////////////////////////////////////////////////////////////////////////////

void EGA_show_help(void)
{
    EGA_do_print("EGA has the following functions:\n");
    std::vector<std::string> names;
    for (auto pair : s_fn_map)
    {
        names.push_back(pair.first);
    }
    std::sort(names.begin(), names.end());
    for (auto& name : names)
    {
        EGA_do_print("  %s\n", name.c_str());
    }
}

void EGA_show_help(const std::string& name)
{
    auto it = s_fn_map.find(name);
    if (it == s_fn_map.end() || !it->second)
    {
        EGA_do_print("ERROR: No such function: '%s'\n", name.c_str());
        return;
    }

    EGA_do_print("EGA function '%s':\n", name.c_str());

    if (it->second->min_args == it->second->max_args)
    {
        EGA_do_print("  argument number: %d\n", int(it->second->min_args));
    }
    else
    {
        EGA_do_print("  argument number: %d..%d\n", int(it->second->min_args), int(it->second->max_args));
    }

    EGA_do_print("  usage: %s\n", it->second->help.c_str());
}

int EGA_interactive(bool echo)
{
    char buf[512];

    EGA_do_print("Type 'exit' to exit. Type 'help' to see help.\n");

    for (;;)
    {
        EGA_do_print("\nEGA> ");
        std::fflush(stdout);

        if (!fgets(buf, sizeof(buf), stdin))
            break;

        if (echo)
            EGA_do_print("%s", buf);

        mstr_trim(buf, " \t\r\n\f\v;");

        if (strcmp(buf, "exit") == 0)
            break;

        if (strcmp(buf, "help") == 0)
        {
            EGA_show_help();
            continue;
        }

        if (memcmp(buf, "help", 4) == 0 &&
            std::isspace(buf[4]))
        {
            std::string name = &buf[5];
            mstr_trim(name, " \t\r\n\f\v;");
            EGA_show_help(name);
            continue;
        }

        EGA_eval_text_ex(buf);
    }

    return 0;
}

bool EGA_file_input(const char *filename)
{
    std::string str;
    if (FILE *fp = fopen(filename, "r"))
    {
        char buf[512];
        while (fgets(buf, sizeof(buf), fp))
        {
            str += buf;
        }
        fclose(fp);

        EGA_eval_text_ex(str.c_str());
        return true;
    }

    EGA_do_print("ERROR: cannot open file '%s'\n", filename);
    return false;
}

#ifndef EGA_LIB
int main(int argc, char **argv)
{
    mstr_unittest();

    if (argc <= 1)
    {
        EGA_init();
        EGA_interactive();
    }
    else
    {
        std::string arg = argv[1];
        if (arg == "--help")
        {
            printf("Usage: EGA [options] [input-file]\n");
            printf("Options:\n");
            printf("  --help      Show this message.\n");
            printf("  --version   Show version info.\n");
            return 0;
        }

        if (arg == "--version")
        {
            printf("EGA Version %d by katahiromz\n", EGA_HPP_);
            return 0;
        }

        EGA_init();
        EGA_file_input(argv[1]);
    }

    EGA_uninit();

    assert(Token::s_alive_count == 0);
    assert(AstBase::s_alive_count == 0);
    return 0;
}
#endif  // ndef EGA_LIB
