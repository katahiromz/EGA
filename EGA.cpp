// EGA.cpp --- The Programming Language EGA
// Copyright (C) 2020 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// This file is public domain software.

#include "EGA.hpp"
#include <cstdio>
#include <cstring>

static int s_alive_tokens = 0;
static int s_alive_ast = 0;

static std::vector<EGA_FUNCTION> s_functions;

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
    case AST_INVALID: return "AST_INVALID";
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

AstContainer *TokenStream::do_parse()
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

AstContainer *TokenStream::visit_translation_unit()
{
    PARSE_DEBUG();

    AstContainer *call = new AstContainer(AST_PROGRAM, "program");

    for (;;)
    {
        if (token_type() == TOK_EOF)
            return call;

        if (AstBase *expr = visit_expression())
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
        delete call;
        return NULL;
    }
}

AstBase *TokenStream::visit_expression()
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
        if (do_get_fn(name))
        {
            go_next();
            return visit_call(name);
        }
        else
        {
            AstVar *var = new AstVar(name);
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

AstInt *TokenStream::visit_integer_literal()
{
    PARSE_DEBUG();

    if (token_type() != TOK_INT)
        return NULL;
    AstInt *ai = new AstInt(token()->get_int());
    go_next();
    return ai;
}

AstStr *TokenStream::visit_string_literal()
{
    PARSE_DEBUG();

    if (token_type() != TOK_STR)
        return NULL;
    AstStr *as = new AstStr(token()->get_str());
    go_next();
    return as;
}

AstContainer *TokenStream::visit_array_literal()
{
    PARSE_DEBUG();

    if (token_type() != TOK_SYMBOL || token_str() != "{")
        return NULL;

    go_next();

    if (token_type() == TOK_SYMBOL && token_str() == "}")
    {
        go_next();
        return new AstContainer(AST_ARRAY);
    }

    if (AstContainer *list = visit_expression_list(AST_ARRAY, "array"))
    {
        if (token_type() == TOK_SYMBOL && token_str() == "}")
        {
            go_next();
            return list;
        }

        printf("ERROR: unexpected token (3): '%s'\n", token_str().c_str());
        delete list;
    }

    return NULL;
}

AstContainer *TokenStream::visit_call(const std::string& name)
{
    PARSE_DEBUG();

    if (token_type() != TOK_SYMBOL || token_str() != "(")
        return NULL;

    go_next();

    AstContainer *list = new AstContainer(AST_CALL, name);

    if (token_type() == TOK_SYMBOL)
    {
        if (token_str() == ")")
        {
            go_next();
            return list;
        }
    }

    if (AstBase *expr = visit_expression())
    {
        list->add(expr);
    }
    else
    {
        delete list;
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
            delete list;
            return NULL;
        }
        go_next();

        if (AstBase *expr = visit_expression())
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

AstContainer *TokenStream::visit_expression_list(AstType type, const std::string& name)
{
    PARSE_DEBUG();

    size_t index = get_index();

    AstBase *expr = visit_expression();
    if (!expr)
        return NULL;

    AstContainer *list = new AstContainer(type, name);
    list->add(expr);

    for (;;)
    {
        if (token_type() != TOK_SYMBOL)
        {
            delete list;
            get_index() = index;
            return NULL;
        }

        if (token_str() == ",")
        {
            go_next();
            continue;
        }

        if (token_str() == ")")
        {
            go_next();
            break;
        }

        expr = visit_expression();
        if (!expr)
        {
            delete list;
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

EGA_FUNCTION *do_get_fn(const std::string& name)
{
    EVAL_DEBUG();
    for (size_t i = 0; i < s_functions.size(); ++i)
    {
        if (s_functions[i].name == name)
            return &s_functions[i];
    }
    return NULL;
}

bool
do_add_function(const std::string& name, size_t min_args, size_t max_args,
                EGA_PROC proc)
{
    EGA_FUNCTION fn = { name, min_args, max_args, proc };
    s_functions.push_back(fn);
    return true;
}

AstBase *
do_eval_function(const std::string& name, const args_t& args)
{
    EVAL_DEBUG();
    if (EGA_FUNCTION *fn = do_get_fn(name))
    {
        if (fn->min_args <= args.size() && args.size() <= fn->max_args)
            return (*(fn->proc))(args);
    }
    return NULL;
}

AstBase *do_eval_ast(const AstBase *ast)
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
        if (AstContainer *ast = stream.do_parse())
        {
            //ast->print();
            AstBase *evaled = do_eval_ast(ast);
            if (evaled)
            {
                evaled->print();
                delete evaled;
            }
            delete ast;
            return 0;
        }
        printf("do_parse failed\n");
        return -2;
    }
    printf("do_lexical failed\n");
    return -1;
}

int EGA_int(AstBase *ast)
{
    EVAL_DEBUG();
    if (ast->get_type() != AST_INT)
        throw new EGA_exception;
    return static_cast<AstInt *>(ast)->get_int();
}

std::string EGA_str(AstBase *ast)
{
    EVAL_DEBUG();
    if (ast->get_type() != AST_STR)
        throw new EGA_exception;
    return static_cast<AstStr *>(ast)->get_str();
}

AstBase* EGA_less(const args_t& args)
{
    EVAL_DEBUG();
    if (args.size() != 2)
        return NULL;

    if (AstBase *ast1 = do_eval_ast(args[0]))
    {
        if (AstBase *ast2 = do_eval_ast(args[1]))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            delete ast1;
            delete ast2;
            return new AstInt(i1 < i2);
        }
        delete ast1;
    }
    return NULL;
}

AstBase* EGA_greater(const args_t& args)
{
    EVAL_DEBUG();
    if (args.size() != 2)
        return NULL;

    if (AstBase *ast1 = do_eval_ast(args[0]))
    {
        if (AstBase *ast2 = do_eval_ast(args[1]))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            delete ast1;
            delete ast2;
            return new AstInt(i1 > i2);
        }
        delete ast1;
    }
    return NULL;
}

AstBase* EGA_le(const args_t& args)
{
    EVAL_DEBUG();
    if (args.size() != 2)
        return NULL;

    if (AstBase *ast1 = do_eval_ast(args[0]))
    {
        if (AstBase *ast2 = do_eval_ast(args[1]))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            delete ast1;
            delete ast2;
            return new AstInt(i1 <= i2);
        }
        delete ast1;
    }
    return NULL;
}

AstBase* EGA_ge(const args_t& args)
{
    EVAL_DEBUG();
    if (args.size() != 2)
        return NULL;

    if (AstBase *ast1 = do_eval_ast(args[0]))
    {
        if (AstBase *ast2 = do_eval_ast(args[1]))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            delete ast1;
            delete ast2;
            return new AstInt(i1 >= i2);
        }
        delete ast1;
    }
    return NULL;
}

AstBase* EGA_equal(const args_t& args)
{
    EVAL_DEBUG();
    if (args.size() != 2)
        return NULL;

    if (AstBase *ast1 = do_eval_ast(args[0]))
    {
        if (AstBase *ast2 = do_eval_ast(args[1]))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            delete ast1;
            delete ast2;
            return new AstInt(i1 == i2);
        }
        delete ast1;
    }
    return NULL;
}

AstBase* EGA_not_equal(const args_t& args)
{
    EVAL_DEBUG();
    if (args.size() != 2)
        return NULL;

    if (AstBase *ast1 = do_eval_ast(args[0]))
    {
        if (AstBase *ast2 = do_eval_ast(args[1]))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            delete ast1;
            delete ast2;
            return new AstInt(i1 != i2);
        }
        delete ast1;
    }
    return NULL;
}

AstBase* EGA_print_int(const args_t& args)
{
    EVAL_DEBUG();
    if (args.size() != 1)
        return NULL;

    if (AstBase *ast1 = do_eval_ast(args[0]))
    {
        int i1 = EGA_int(ast1);
        delete ast1;
        printf("%d\n", i1);
    }
    return NULL;
}

AstBase* EGA_print_str(const args_t& args)
{
    EVAL_DEBUG();
    if (args.size() != 1)
        return NULL;

    if (AstBase *ast1 = do_eval_ast(args[0]))
    {
        std::string str = EGA_str(ast1);
        delete ast1;
        printf("%s\n", str.c_str());
    }
    return NULL;
}

AstBase* EGA_str_len(const args_t& args)
{
    EVAL_DEBUG();
    if (args.size() != 1)
        return NULL;

    if (AstBase *ast1 = do_eval_ast(args[0]))
    {
        std::string str1 = EGA_str(ast1);
        delete ast1;
        return new AstInt((int)str1.size());
    }
    return NULL;
}

AstBase* EGA_str_cat(const args_t& args)
{
    EVAL_DEBUG();
    if (args.size() != 2)
        return NULL;

    if (AstBase *ast1 = do_eval_ast(args[0]))
    {
        if (AstBase *ast2 = do_eval_ast(args[1]))
        {
            std::string str1 = EGA_str(ast1);
            std::string str2 = EGA_str(ast2);
            delete ast1;
            delete ast2;
            return new AstStr(str1 + str2);
        }
        delete ast1;
    }
    return NULL;
}

AstBase* EGA_plus(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() == 1)
    {
        if (AstBase *ast1 = do_eval_ast(args[0]))
        {
            int i1 = EGA_int(ast1);
            delete ast1;
            return new AstInt(i1);
        }
        return NULL;
    }

    if (args.size() == 2)
    {
        if (AstBase *ast1 = do_eval_ast(args[0]))
        {
            if (AstBase *ast2 = do_eval_ast(args[1]))
            {
                int i1 = EGA_int(ast1);
                int i2 = EGA_int(ast2);
                delete ast1;
                delete ast2;
                return new AstInt(i1 + i2);
            }
            delete ast1;
        }
    }
    return NULL;
}

AstBase* EGA_minus(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() == 1)
    {
        if (AstBase *ast1 = do_eval_ast(args[0]))
        {
            int i1 = EGA_int(ast1);
            delete ast1;
            return new AstInt(-i1);
        }
        return NULL;
    }

    if (args.size() == 2)
    {
        if (AstBase *ast1 = do_eval_ast(args[0]))
        {
            if (AstBase *ast2 = do_eval_ast(args[1]))
            {
                int i1 = EGA_int(ast1);
                int i2 = EGA_int(ast2);
                delete ast1;
                delete ast2;
                return new AstInt(i1 - i2);
            }
            delete ast1;
        }
    }
    return NULL;
}

AstBase* EGA_mul(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() != 2)
        return NULL;

    if (AstBase *ast1 = do_eval_ast(args[0]))
    {
        if (AstBase *ast2 = do_eval_ast(args[1]))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            delete ast1;
            delete ast2;
            return new AstInt(i1 * i2);
        }
        delete ast1;
    }

    return NULL;
}

AstBase* EGA_div(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() != 2)
        return NULL;

    if (AstBase *ast1 = do_eval_ast(args[0]))
    {
        if (AstBase *ast2 = do_eval_ast(args[1]))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            delete ast1;
            delete ast2;
            return new AstInt(i1 / i2);
        }
        delete ast1;
    }

    return NULL;
}

AstBase* EGA_mod(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() != 2)
        return NULL;

    if (AstBase *ast1 = do_eval_ast(args[0]))
    {
        if (AstBase *ast2 = do_eval_ast(args[1]))
        {
            int i1 = EGA_int(ast1);
            int i2 = EGA_int(ast2);
            delete ast1;
            delete ast2;
            return new AstInt(i1 % i2);
        }
        delete ast1;
    }

    return NULL;
}

AstBase* EGA_if(const args_t& args)
{
    EVAL_DEBUG();

    if (args.size() != 2 && args.size() != 3)
        return NULL;

    if (AstBase *ast1 = do_eval_ast(args[0]))
    {
        int i1 = EGA_int(ast1);
        delete ast1;
        if (i1)
        {
            if (AstBase *ast2 = do_eval_ast(args[1]))
            {
                return ast2;
            }
        }
        else if (args.size() == 3)
        {
            if (AstBase *ast3 = do_eval_ast(args[2]))
            {
                return ast3;
            }
        }
    }

    return NULL;
}

bool
do_add_basic_functions(void)
{
    do_add_function("equal", 2, 2, EGA_equal);
    do_add_function("==", 2, 2, EGA_equal);
    do_add_function("not_equal", 2, 2, EGA_not_equal);
    do_add_function("!=", 2, 2, EGA_not_equal);
    do_add_function("less", 2, 2, EGA_less);
    do_add_function("<", 2, 2, EGA_less);
    do_add_function("le", 2, 2, EGA_le);
    do_add_function("<=", 2, 2, EGA_le);
    do_add_function("greater", 2, 2, EGA_greater);
    do_add_function(">", 2, 2, EGA_greater);
    do_add_function("ge", 2, 2, EGA_ge);
    do_add_function(">=", 2, 2, EGA_ge);
    do_add_function("print_str", 1, 1, EGA_print_str);
    do_add_function("print_int", 1, 1, EGA_print_int);
    do_add_function("str_len", 1, 1, EGA_str_len);
    do_add_function("str_cat", 2, 2, EGA_str_cat);
    do_add_function("plus", 1, 2, EGA_plus);
    do_add_function("+", 1, 2, EGA_plus);
    do_add_function("minus", 1, 2, EGA_minus);
    do_add_function("-", 1, 2, EGA_minus);
    do_add_function("mul", 2, 2, EGA_mul);
    do_add_function("*", 2, 2, EGA_mul);
    do_add_function("div", 2, 2, EGA_div);
    do_add_function("/", 2, 2, EGA_div);
    do_add_function("mod", 2, 2, EGA_mod);
    do_add_function("%", 2, 2, EGA_mod);
    do_add_function("if", 2, 3, EGA_if);
    do_add_function("?:", 2, 3, EGA_if);
    return true;
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

        return do_eval_text(str.c_str());
    }

    printf("ERROR: cannot open file '%s'\n", filename);
    return -1;
}

int main(int argc, char **argv)
{
    mstr_unittest();
    do_add_basic_functions();

    if (argc <= 1)
    {
        do_interpret_mode();
    }
    else
    {
        do_file_input_mode(argv[1]);
    }

    assert(s_alive_tokens == 0);
    assert(s_alive_ast == 0);
    return 0;
}
