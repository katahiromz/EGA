// ega.hpp --- The Programming Language EGA
// Copyright (C) 2020 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// This file is public domain software.

#ifndef EGA_HPP_
#define EGA_HPP_    6 // Version 6

#pragma once

#include <vector>
#include <memory>
#include <cstdlib>
#include <cassert>

namespace EGA {

class AstBase;
class AstInt;
class AstStr;
class AstContainer;

//////////////////////////////////////////////////////////////////////////////
// arg_t, args_t, make_arg

typedef std::shared_ptr<AstBase> arg_t;
typedef std::vector<arg_t> args_t;

template <class T>
std::shared_ptr<T> make_arg()
{
    return std::make_shared<T>();
}
template <class T, typename T1>
std::shared_ptr<T> make_arg(const T1& t1)
{
    return std::make_shared<T>(t1);
}
template <class T, typename T1, typename T2>
std::shared_ptr<T> make_arg(const T1& t1, const T2& t2)
{
    return std::make_shared<T>(t1, t2);
}
template <class T, typename T1, typename T2, typename T3>
std::shared_ptr<T> make_arg(const T1& t1, const T2& t2, const T3& t3)
{
    return std::make_shared<T>(t1, t2, t3);
}

//////////////////////////////////////////////////////////////////////////////
// functions

#define EGA_FN

typedef arg_t (*EGA_PROC)(const args_t& args);

struct EGA_FUNCTION
{
    std::string name;
    size_t min_args;
    size_t max_args;
    EGA_PROC proc;
    std::string help;

    EGA_FUNCTION(std::string n, size_t m1, size_t m2, EGA_PROC p, std::string h)
        : name(n)
        , min_args(m1)
        , max_args(m2)
        , proc(p)
        , help(h)
    {
    }
};
typedef std::shared_ptr<EGA_FUNCTION> fn_t;

bool EGA_add_fn(const std::string& name, size_t min_args, size_t max_args, EGA_PROC proc,
                const std::string& help);

//////////////////////////////////////////////////////////////////////////////
// printing

typedef void (*EGA_PRINT_FN)(const char *fmt, va_list va);
void EGA_set_print_fn(EGA_PRINT_FN fn);
void EGA_default_print(const char *fmt, va_list va);
void EGA_do_print(const char *fmt, ...);

//////////////////////////////////////////////////////////////////////////////
// inputing

typedef bool (*EGA_INPUT_FN)(char *buf, size_t buflen);
void EGA_set_input_fn(EGA_INPUT_FN fn);
bool EGA_default_input(char *buf, size_t buflen);
bool EGA_do_input(char *buf, size_t buflen);

//////////////////////////////////////////////////////////////////////////////
// exceptions

#include <stdexcept>

class EGA_exception : public std::runtime_error
{
public:
    EGA_exception(const std::string& what, int lineno = 0)
        : std::runtime_error(what)
        , m_lineno(lineno)
    {
    }

    int get_lineno() const
    {
        return m_lineno;
    }

protected:
    int m_lineno;
};

class EGA_syntax_error : public EGA_exception
{
public:
    EGA_syntax_error(int lineno) : EGA_exception("syntax error", lineno)
    {
    }
};

class EGA_type_mismatch : public EGA_exception
{
public:
    EGA_type_mismatch(int lineno) : EGA_exception("type mismatch", lineno)
    {
    }
};

class EGA_undefined_variable : public EGA_exception
{
public:
    EGA_undefined_variable(const std::string& name, int lineno)
        : EGA_exception((std::string("undefined variable: '") += name) += "'", lineno)
    {
    }
};

class EGA_argument_number_exception : public EGA_exception
{
public:
    EGA_argument_number_exception(int lineno)
        : EGA_exception("argument number mismatch", lineno)
    {
    }
};

class EGA_index_out_of_range : public EGA_exception
{
public:
    EGA_index_out_of_range(int lineno)
        : EGA_exception("index out of range", lineno)
    {
    }
};

class EGA_exit_exception : public EGA_exception
{
public:
    arg_t m_arg;

    EGA_exit_exception(arg_t arg) : EGA_exception("exit exception"), m_arg(arg)
    {
    }
};

class EGA_break_exception : public EGA_exception
{
public:
    EGA_break_exception() : EGA_exception("break exception")
    {
    }
};

class EGA_illegal_operation : public EGA_exception
{
public:
    EGA_illegal_operation(int lineno) : EGA_exception("illegal operation", lineno)
    {
    }
};

//////////////////////////////////////////////////////////////////////////////
// TokenType

enum TokenType
{
    TOK_EOF,
    TOK_INT,
    TOK_STR,
    TOK_IDENT,
    TOK_SYMBOL
};

std::string EGA_dump_token_type(TokenType type);

//////////////////////////////////////////////////////////////////////////////
// AstType

enum AstType
{
    AST_INT,
    AST_STR,
    AST_ARRAY,
    AST_VAR,
    AST_CALL,
    AST_PROGRAM
};

std::string EGA_dump_ast_type(AstType type);

//////////////////////////////////////////////////////////////////////////////
// Token

class Token
{
public:
    static int s_alive_count;
    static void alive_count(bool add);

    Token(TokenType type, size_t lineno, const std::string& str)
        : m_type(type)
        , m_lineno(lineno)
        , m_str(str)
    {
        alive_count(true);

        if (type == TOK_INT)
            m_int = std::atoi(str.c_str());
        else
            m_int = 0;
    }

    virtual ~Token()
    {
        alive_count(false);
    }

    TokenType& get_type()
    {
        return m_type;
    }

    int get_lineno() const
    {
        return m_lineno;
    }

    std::string& get_str()
    {
        return m_str;
    }

    int get_int() const
    {
        return m_int;
    }

    virtual std::string dump() const;

    void print() const;

protected:
    TokenType m_type;
    int m_lineno;
    std::string m_str;
    int m_int;

private:
    // Token is not copyable.
    Token(const Token&);
    Token& operator=(const Token&);
};
typedef std::shared_ptr<Token> token_t;
typedef std::vector<token_t> tokens_t;

//////////////////////////////////////////////////////////////////////////////
// TokenStream

class TokenStream
{
public:
    friend class Token;
    friend class AstBase;

    TokenStream()
        : m_error(0)
        , m_index(0)
    {
    }

    virtual ~TokenStream()
    {
    }

    void add(token_t token)
    {
        m_tokens.push_back(token);
    }

    void add(TokenType type, int line, const std::string& str)
    {
        add(std::make_shared<Token>(type, line, str));
    }

    bool do_lexical(const char *input, int& lineno);

    bool do_lexical(const std::string& input, int& lineno)
    {
        return do_lexical(input.c_str(), lineno);
    }

    int get_error() const
    {
        return m_error;
    }

    arg_t do_parse();

    token_t operator[](size_t index)
    {
        assert(index < size());
        return m_tokens[index];
    }

    const token_t& operator[](size_t index) const
    {
        assert(index < size());
        return m_tokens[index];
    }

    size_t size() const
    {
        return m_tokens.size();
    }

    size_t& get_index()
    {
        return m_index;
    }

    token_t token()
    {
        assert(m_index < size());
        return m_tokens[m_index];
    }

    TokenType& token_type()
    {
        return token()->get_type();
    }

    std::string& token_str()
    {
        return token()->get_str();
    }

    int get_lineno()
    {
        return token()->get_lineno();
    }

    bool go_next()
    {
        if (m_index < size())
        {
            ++m_index;
            return true;
        }
        return false;
    }

    bool go_back()
    {
        if (m_index > 0)
        {
            --m_index;
            return true;
        }
        return false;
    }

    std::string dump() const;

    void print() const;

protected:
    tokens_t m_tokens;
    int m_error;
    size_t m_index;

    arg_t visit_translation_unit();
    arg_t visit_expression();
    arg_t visit_integer_literal();
    arg_t visit_string_literal();
    arg_t visit_array_literal();
    arg_t visit_call(const std::string& name);
    arg_t visit_expression_list(AstType type, const std::string& name = "");
};

//////////////////////////////////////////////////////////////////////////////
// AstBase

class AstBase
{
public:
    static int s_alive_count;
    static void alive_count(bool add);

    virtual ~AstBase()
    {
        alive_count(false);
    }

    AstType& get_type()
    {
        return m_type;
    }

    virtual std::string dump(bool q) const = 0;

    void print() const;

    virtual arg_t clone() const = 0;

    virtual arg_t eval() const = 0;

    int get_lineno() const
    {
        return m_lineno;
    }

protected:
    AstType m_type;
    int m_lineno;

    AstBase(AstType type, int lineno)
        : m_type(type)
        , m_lineno(lineno)
    {
        alive_count(true);
    }

private:
    // AstBase is not copyable.
    AstBase(const AstBase&);
    AstBase& operator=(const AstBase&);
};

//////////////////////////////////////////////////////////////////////////////
// AstInt

class AstInt : public AstBase
{
public:
    AstInt(int value = 0, int lineno = 0)
        : AstBase(AST_INT, lineno)
        , m_value(value)
    {
    }

    int& get_int()
    {
        return m_value;
    }

    virtual std::string dump(bool q) const;

    virtual arg_t clone() const
    {
        return make_arg<AstInt>(m_value);
    }

    virtual arg_t eval() const
    {
        return clone();
    }

protected:
    int m_value;
};

//////////////////////////////////////////////////////////////////////////////
// AstStr

class AstStr : public AstBase
{
public:
    AstStr(const std::string& str, int lineno = 0)
        : AstBase(AST_STR, lineno)
        , m_str(str)
    {
    }

    std::string& get_str()
    {
        return m_str;
    }

    virtual std::string dump(bool q) const;

    virtual arg_t clone() const
    {
        return make_arg<AstStr>(m_str);
    }

    virtual arg_t eval() const
    {
        return clone();
    }

protected:
    std::string m_str;
};

//////////////////////////////////////////////////////////////////////////////
// AstVar

class AstVar : public AstBase
{
public:
    AstVar(const std::string& name, int lineno = 0)
        : AstBase(AST_VAR, lineno)
        , m_name(name)
    {
    }

    ~AstVar()
    {
    }

    std::string get_name() const
    {
        return m_name;
    }

    virtual std::string dump(bool q) const
    {
        return m_name;
    }

    virtual arg_t clone() const
    {
        return make_arg<AstVar>(m_name, m_lineno);
    }

    virtual arg_t eval() const;

protected:
    std::string m_name;
};

//////////////////////////////////////////////////////////////////////////////
// AstContainer

class AstContainer : public AstBase
{
public:
    AstContainer(AstType type, int lineno = 0, const std::string& str = "")
        : AstBase(type, lineno)
        , m_str(str)
    {
        assert(type == AST_ARRAY || type == AST_CALL || type == AST_PROGRAM);
    }

    ~AstContainer()
    {
    }

    arg_t& operator[](size_t index)
    {
        assert(index < size());
        return m_children[index];
    }

    const arg_t& operator[](size_t index) const
    {
        assert(index < size());
        return m_children[index];
    }

    size_t size() const
    {
        return m_children.size();
    }

    bool empty() const
    {
        return size() == 0;
    }

    void add(arg_t ast)
    {
        m_children.push_back(ast);
    }

    std::vector<arg_t>& children()
    {
        return m_children;
    }

    std::string& get_str()
    {
        return m_str;
    }

    virtual std::string dump(bool q) const;

    virtual arg_t clone() const;

    virtual arg_t eval() const;

protected:
    std::string m_str;
    std::vector<arg_t> m_children;
};

//////////////////////////////////////////////////////////////////////////////
// global functions

bool EGA_init(void);
void EGA_uninit(void);

void EGA_set_var(const std::string& name, arg_t ast);
bool EGA_eval_text_ex(const char *text);

int EGA_interactive(const char *filename = NULL, bool echo = false);
bool EGA_file_input(const char *filename);
arg_t EGA_eval_arg(arg_t ast, bool do_check);
int EGA_get_int(arg_t ast);
std::string EGA_get_str(arg_t ast);

} // namespace EGA

#endif  // ndef EGA_HPP_
