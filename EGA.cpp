// EGA.cpp --- The Programming Language EGA
// Copyright (C) 2020 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// This file is public domain software.

#define _CRT_SECURE_NO_WARNINGS
#include "EGA.hpp"
#include <cstdio>
#include <cstring>
#include <unordered_map>

static int s_alive_tokens = 0;
static int s_alive_ast = 0;

typedef std::unordered_map<std::string, fn_t> fn_map_t;
typedef std::unordered_map<std::string, arg_t> var_map_t;
static fn_map_t s_fn_map;
static var_map_t s_var_map;

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

void token_alive_count(bool add)
{
    if (add)
    {
        assert(s_alive_tokens >= 0);
        ++s_alive_tokens;
    }
    else
    {
        --s_alive_tokens;
        assert(s_alive_tokens >= 0);
    }
}

void ast_alive_count(bool add)
{
    if (add)
    {
        assert(s_alive_ast >= 0);
        ++s_alive_ast;
    }
    else
    {
        --s_alive_ast;
        assert(s_alive_ast >= 0);
    }
}

//////////////////////////////////////////////////////////////////////////////
// Dumping

std::string dump_token_type(TokenType type)
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
    ret += dump_token_type(m_type);
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

std::string dump_ast_type(AstType type)
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
            ++pch;
            for (;;)
            {
                if (!is_ident_char(*pch))
                    break;
                str += *pch;
                ++pch;
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
                    break;
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

        printf("ERROR: invalid character '%c'\n", *pch);
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
    #define PARSE_DEBUG() do { puts(__func__); do_print_stream(*this); } while (0)
#endif

void do_print_stream(TokenStream& stream)
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

        printf("ERROR: unexpected token (2): '%s'\n", token_str().c_str());
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

        printf("ERROR: unexpected token (3): '%s'\n", token_str().c_str());
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

fn_t EGA_get_fn(const std::string& name)
{
    EVAL_DEBUG();
    fn_map_t::iterator it = s_fn_map.find(name);
    if (it == s_fn_map.end())
        return NULL;
    return it->second;
}

bool
EGA_add_fn(const std::string& name, size_t min_args, size_t max_args, EGA_PROC proc)
{
    auto fn = std::make_shared<EGA_FUNCTION>(name, min_args, max_args, proc);
    s_fn_map[name] = fn;
    return true;
}

arg_t EGA_eval_var(const std::string& name)
{
    EVAL_DEBUG();

    var_map_t::iterator it = s_var_map.find(name);
    if (it == s_var_map.end())
        return NULL;

    return it->second->eval();
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
        }
    }
    else
    {
        for (size_t i = 0; i < args.size(); ++i)
        {
            args[i]->eval();
        }
    }
    return NULL;
}

arg_t do_eval_ast(const arg_t& ast)
{
    EVAL_DEBUG();
    if (!ast)
        throw new EGA_exception;
    return ast->eval();
}

int do_eval_text(const char *text)
{
    TokenStream stream;
    if (stream.do_lexical(text))
    {
        if (auto ast = stream.do_parse())
        {
            //ast->print();
            auto evaled = do_eval_ast(ast);
            if (evaled)
            {
                evaled->print();
            }
            return 0;
        }
        printf("do_parse failed\n");
        return -2;
    }
    printf("do_lexical failed\n");
    return -1;
}

int EGA_int(arg_t ast)
{
    EVAL_DEBUG();
    if (ast->get_type() != AST_INT)
        throw new EGA_exception;
    return std::static_pointer_cast<AstInt>(ast)->get_int();
}

std::shared_ptr<AstArray> EGA_array(arg_t ast)
{
    EVAL_DEBUG();
    if (ast->get_type() != AST_ARRAY)
        throw new EGA_exception;
    return std::static_pointer_cast<AstArray>(ast);
}

std::string EGA_str(arg_t ast)
{
    EVAL_DEBUG();
    if (ast->get_type() != AST_STR)
        throw new EGA_exception;
    return std::static_pointer_cast<AstStr>(ast)->get_str();
}

std::shared_ptr<AstInt>
EGA_compare_0(arg_t ast1, arg_t ast2)
{
    EVAL_DEBUG();

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
        break;
    }

    return NULL;
}

arg_t EGA_compare(const args_t& args)
{
    EVAL_DEBUG();
    if (args.size() != 2)
        return NULL;

    if (auto ai = EGA_compare_0(args[0], args[1]))
    {
        return ai;
    }
    return NULL;
}

arg_t EGA_less(const args_t& args)
{
    EVAL_DEBUG();
    if (args.size() != 2)
        return NULL;

    if (auto ai = EGA_compare_0(args[0], args[1]))
    {
        ai->get_int() = (ai->get_int() < 0);
        return ai;
    }
    return NULL;
}

arg_t EGA_greater(const args_t& args)
{
    EVAL_DEBUG();
    if (args.size() != 2)
        return NULL;

    if (auto ai = EGA_compare_0(args[0], args[1]))
    {
        ai->get_int() = (ai->get_int() > 0);
        return ai;
    }
    return NULL;
}

arg_t EGA_less_equal(const args_t& args)
{
    EVAL_DEBUG();
    if (args.size() != 2)
        return NULL;

    if (auto ai = EGA_compare_0(args[0], args[1]))
    {
        ai->get_int() = (ai->get_int() <= 0);
        return ai;
    }
    return NULL;
}

arg_t EGA_greater_equal(const args_t& args)
{
    EVAL_DEBUG();
    if (args.size() != 2)
        return NULL;

    if (auto ai = EGA_compare_0(args[0], args[1]))
    {
        ai->get_int() = (ai->get_int() >= 0);
        return ai;
    }
    return NULL;
}

arg_t EGA_equal(const args_t& args)
{
    EVAL_DEBUG();
    if (args.size() != 2)
        return NULL;

    if (auto ai = EGA_compare_0(args[0], args[1]))
    {
        ai->get_int() = (ai->get_int() == 0);
        return ai;
    }
    return NULL;
}

arg_t EGA_not_equal(const args_t& args)
{
    EVAL_DEBUG();
    if (args.size() != 2)
        return NULL;

    if (auto ai = EGA_compare_0(args[0], args[1]))
    {
        ai->get_int() = (ai->get_int() != 0);
        return ai;
    }
    return NULL;
}

arg_t EGA_print(const args_t& args)
{
    EVAL_DEBUG();

    for (size_t i = 0; i < args.size(); ++i)
    {
        if (auto ast = do_eval_ast(args[i]))
        {
            printf("%s\n", ast->dump().c_str());
        }
    }
    return NULL;
}

arg_t EGA_println(const args_t& args)
{
    EVAL_DEBUG();

    EGA_print(args);
    printf("\n");
    return NULL;
}

arg_t EGA_len(const args_t& args)
{
    EVAL_DEBUG();
    if (args.size() != 1)
        return NULL;

    if (auto ast1 = do_eval_ast(args[0]))
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
            break;
        }
    }
    return NULL;
}

arg_t EGA_cat(const args_t& args)
{
    EVAL_DEBUG();
    if (args.size() < 1)
        return NULL;

    if (auto ast1 = do_eval_ast(args[0]))
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
            break;
        }
    }

    return NULL;
}

arg_t EGA_plus(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() == 2)
    {
        if (auto ast1 = do_eval_ast(args[0]))
        {
            if (auto ast2 = do_eval_ast(args[1]))
            {
                int i1 = EGA_int(ast1);
                int i2 = EGA_int(ast2);
                return make_arg<AstInt>(i1 + i2);
            }
        }
    }
    return NULL;
}

arg_t EGA_minus(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() == 1)
    {
        if (auto ast1 = do_eval_ast(args[0]))
        {
            int i1 = EGA_int(ast1);
            return make_arg<AstInt>(-i1);
        }
        return NULL;
    }

    if (args.size() == 2)
    {
        if (auto ast1 = do_eval_ast(args[0]))
        {
            if (auto ast2 = do_eval_ast(args[1]))
            {
                int i1 = EGA_int(ast1);
                int i2 = EGA_int(ast2);
                return make_arg<AstInt>(i1 - i2);
            }
        }
    }
    return NULL;
}

arg_t EGA_mul(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() != 2)
        return NULL;

    if (auto ast1 = do_eval_ast(args[0]))
    {
        if (auto ast2 = do_eval_ast(args[1]))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            return make_arg<AstInt>(i1 * i2);
        }
    }

    return NULL;
}

arg_t EGA_div(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() != 2)
        return NULL;

    if (auto ast1 = do_eval_ast(args[0]))
    {
        if (auto ast2 = do_eval_ast(args[1]))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            return make_arg<AstInt>(i1 / i2);
        }
    }

    return NULL;
}

arg_t EGA_mod(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() != 2)
        return NULL;

    if (auto ast1 = do_eval_ast(args[0]))
    {
        if (auto ast2 = do_eval_ast(args[1]))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            return make_arg<AstInt>(i1 % i2);
        }
    }

    return NULL;
}

arg_t EGA_if(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() != 2 && args.size() != 3)
        return NULL;

    if (auto ast1 = do_eval_ast(args[0]))
    {
        int i1 = EGA_int(ast1);
        if (i1)
        {
            if (auto ast2 = do_eval_ast(args[1]))
            {
                return ast2;
            }
        }
        else if (args.size() == 3)
        {
            if (auto ast3 = do_eval_ast(args[2]))
            {
                return ast3;
            }
        }
    }

    return NULL;
}

void EGA_set_var(const std::string& name, arg_t arg)
{
    s_var_map[name] = arg;
}

arg_t EGA_set(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() != 2 || args[0]->get_type() != AST_VAR)
        return NULL;

    std::string name = std::static_pointer_cast<AstVar>(args[0])->get_name();
    auto value = args[1]->eval();
    EGA_set_var(name, value);
    return NULL;
}

arg_t EGA_for(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() != 4)
        return NULL;

    if (args[0]->get_type() != AST_VAR)
        return NULL;

    if (auto ast1 = do_eval_ast(args[1]))
    {
        if (auto ast2 = do_eval_ast(args[2]))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);

            for (int i = i1; i <= i2; ++i)
            {
                auto ai = make_arg<AstInt>(i);
                auto var = std::static_pointer_cast<AstVar>(args[0]);
                EGA_set_var(var->get_name(), ai);
                do_eval_ast(args[3]);
            }
        }
    }

    return NULL;
}

arg_t EGA_while(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() != 2)
        return NULL;

    while (true)
    {
        auto ast1 = do_eval_ast(args[0]);
        if (ast1)
        {
            int i1 = EGA_int(ast1);
            if (!i1)
                break;
        }

        do_eval_ast(args[1]);
    }

    return NULL;
}

arg_t EGA_at(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() != 2)
        return NULL;

    if (auto ast1 = do_eval_ast(args[0]))
    {
        if (auto ast2 = do_eval_ast(args[1]))
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

arg_t EGA_not(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() != 1)
        return NULL;

    if (auto ast1 = do_eval_ast(args[0]))
    {
        int i = EGA_int(ast1);
        return make_arg<AstInt>(!i);
    }

    return NULL;
}

arg_t EGA_logical_or(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() != 2)
        return NULL;

    if (auto ast1 = do_eval_ast(args[0]))
    {
        if (auto ast2 = do_eval_ast(args[1]))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            return make_arg<AstInt>(i1 || i2);
        }
    }

    return NULL;
}

arg_t EGA_logical_and(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() != 2)
        return NULL;

    if (auto ast1 = do_eval_ast(args[0]))
    {
        if (auto ast2 = do_eval_ast(args[1]))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            return make_arg<AstInt>(i1 && i2);
        }
    }

    return NULL;
}

arg_t EGA_compl(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() != 1)
        return NULL;

    if (auto ast1 = do_eval_ast(args[0]))
    {
        int i = EGA_int(ast1);
        return make_arg<AstInt>(~i);
    }

    return NULL;
}

arg_t EGA_bitor(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() != 2)
        return NULL;

    if (auto ast1 = do_eval_ast(args[0]))
    {
        if (auto ast2 = do_eval_ast(args[1]))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            return make_arg<AstInt>(i1 | i2);
        }
    }

    return NULL;
}

arg_t EGA_bitand(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() != 2)
        return NULL;

    if (auto ast1 = do_eval_ast(args[0]))
    {
        if (auto ast2 = do_eval_ast(args[1]))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            return make_arg<AstInt>(i1 & i2);
        }
    }

    return NULL;
}

arg_t EGA_xor(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() != 2)
        return NULL;

    if (auto ast1 = do_eval_ast(args[0]))
    {
        if (auto ast2 = do_eval_ast(args[1]))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            return make_arg<AstInt>(i1 ^ i2);
        }
    }

    return NULL;
}

arg_t EGA_left(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() != 2)
        return NULL;

    if (auto ast1 = do_eval_ast(args[0]))
    {
        if (auto ast2 = do_eval_ast(args[1]))
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
                }
                break;
            default:
                break;
            }
        }
    }

    return NULL;
}

arg_t EGA_right(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() != 2)
        return NULL;

    if (auto ast1 = do_eval_ast(args[0]))
    {
        if (auto ast2 = do_eval_ast(args[1]))
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
                }
                break;
            default:
                break;
            }
        }
    }

    return NULL;
}

arg_t EGA_mid(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() != 3)
        return NULL;

    if (auto ast1 = do_eval_ast(args[0]))
    {
        if (auto ast2 = do_eval_ast(args[1]))
        {
            if (auto ast3 = do_eval_ast(args[2]))
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
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }

    return NULL;
}

bool EGA_init(void)
{
    // assignment
    EGA_add_fn("set", 2, 2, EGA_set);
    EGA_add_fn("=", 2, 2, EGA_set);
    EGA_add_fn(":=", 2, 2, EGA_set);

    // control structure
    EGA_add_fn("if", 2, 3, EGA_if);
    EGA_add_fn("?:", 2, 3, EGA_if);
    EGA_add_fn("for", 4, 4, EGA_for);
    EGA_add_fn("while", 2, 2, EGA_while);

    // comparison
    EGA_add_fn("equal", 2, 2, EGA_equal);
    EGA_add_fn("==", 2, 2, EGA_equal);
    EGA_add_fn("not_equal", 2, 2, EGA_not_equal);
    EGA_add_fn("!=", 2, 2, EGA_not_equal);
    EGA_add_fn("compare", 2, 2, EGA_compare);
    EGA_add_fn("less", 2, 2, EGA_less);
    EGA_add_fn("<", 2, 2, EGA_less);
    EGA_add_fn("less_equal", 2, 2, EGA_less_equal);
    EGA_add_fn("<=", 2, 2, EGA_less_equal);
    EGA_add_fn("greater", 2, 2, EGA_greater);
    EGA_add_fn(">", 2, 2, EGA_greater);
    EGA_add_fn("greater_equal", 2, 2, EGA_greater_equal);
    EGA_add_fn(">=", 2, 2, EGA_greater_equal);

    // print
    EGA_add_fn("print", 0, 16, EGA_print);
    EGA_add_fn("println", 0, 16, EGA_println);
    EGA_add_fn("?", 0, 16, EGA_println);

    // arithmetic
    EGA_add_fn("plus", 2, 2, EGA_plus);
    EGA_add_fn("+", 2, 2, EGA_plus);
    EGA_add_fn("minus", 1, 2, EGA_minus);
    EGA_add_fn("-", 1, 2, EGA_minus);
    EGA_add_fn("mul", 2, 2, EGA_mul);
    EGA_add_fn("*", 2, 2, EGA_mul);
    EGA_add_fn("div", 2, 2, EGA_div);
    EGA_add_fn("/", 2, 2, EGA_div);
    EGA_add_fn("mod", 2, 2, EGA_mod);
    EGA_add_fn("%", 2, 2, EGA_mod);

    // logical
    EGA_add_fn("not", 2, 2, EGA_not);
    EGA_add_fn("!", 2, 2, EGA_not);
    EGA_add_fn("or", 2, 2, EGA_logical_or);
    EGA_add_fn("||", 2, 2, EGA_logical_or);
    EGA_add_fn("and", 2, 2, EGA_logical_and);
    EGA_add_fn("&&", 2, 2, EGA_logical_and);

    // bit operation
    EGA_add_fn("compl", 2, 2, EGA_compl);
    EGA_add_fn("~", 2, 2, EGA_compl);
    EGA_add_fn("bitor", 2, 2, EGA_bitor);
    EGA_add_fn("|", 2, 2, EGA_bitor);
    EGA_add_fn("bitand", 2, 2, EGA_bitand);
    EGA_add_fn("&", 2, 2, EGA_bitand);
    EGA_add_fn("xor", 2, 2, EGA_xor);
    EGA_add_fn("^", 2, 2, EGA_xor);

    // array/string manipulation
    EGA_add_fn("len", 1, 1, EGA_len);
    EGA_add_fn("cat", 0, 15, EGA_cat);
    EGA_add_fn("[]", 2, 2, EGA_at);
    EGA_add_fn("at", 2, 2, EGA_at);
    EGA_add_fn("left", 2, 2, EGA_left);
    EGA_add_fn("right", 2, 2, EGA_right);
    EGA_add_fn("mid", 3, 3, EGA_mid);

    return true;
}

void
EGA_uninit(void)
{
    s_fn_map.clear();

    s_var_map.clear();
}

//////////////////////////////////////////////////////////////////////////////

int
do_interpret_mode(void)
{
    char buf[512];

    printf("Type quit to quit.\n");
    printf("\nEGA> ");
    std::fflush(stdout);

    while (fgets(buf, sizeof(buf), stdin))
    {
        mstr_trim(buf, " \t\r\n\f\v");
        if (strcmp(buf, "quit") == 0)
            break;

        do_eval_text(buf);
        printf("\nEGA> ");
        std::fflush(stdout);
    }

    return 0;
}

bool
do_file_input_mode(const char *filename)
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

        do_eval_text(str.c_str());
        return true;
    }

    printf("ERROR: cannot open file '%s'\n", filename);
    return false;
}

int main(int argc, char **argv)
{
    mstr_unittest();
    EGA_init();

    if (argc <= 1)
    {
        do_interpret_mode();
    }
    else
    {
        do_file_input_mode(argv[1]);
    }

    EGA_uninit();

    assert(s_alive_tokens == 0);
    assert(s_alive_ast == 0);
    return 0;
}
