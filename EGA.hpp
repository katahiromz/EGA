// EGA.hpp --- The Programming Language EGA
// Copyright (C) 2020 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// This file is public domain software.

#ifndef EGA_HPP_
#define EGA_HPP_    0   // Version 0

#pragma once

#include <vector>
#include <cstdlib>
#include <cctype>
#include <cassert>
#include "mstr.hpp"

class AstBase;
class AstInt;
class AstStr;
class AstContainer;

//////////////////////////////////////////////////////////////////////////////

typedef std::vector<AstBase *> args_t;

typedef AstBase* (*EGA_PROC)(const args_t& args);

struct EGA_FUNCTION
{
    std::string name;
    size_t min_args;
    size_t max_args;
    EGA_PROC proc;

    EGA_FUNCTION(std::string n, size_t m1, size_t m2, EGA_PROC p)
        : name(n)
        , min_args(m1)
        , max_args(m2)
        , proc(p)
    {
    }
};

struct EGA_exception { };
struct EGA_arg_number_exception : EGA_exception { };

EGA_FUNCTION *EGA_get_fn(const std::string& name);
bool EGA_add_fn(const std::string& name, size_t min_args, size_t max_args, EGA_PROC proc);
AstBase * EGA_eval_fn(const std::string& name, const std::vector<AstBase *>& args);
AstBase *EGA_eval_var(const std::string& name);
void EGA_set_var(const std::string& name, const AstBase *ast);

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

std::string dump_token_type(TokenType type);

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

std::string dump_ast_type(AstType type);

//////////////////////////////////////////////////////////////////////////////
// Token

void token_alive_count(bool add);

class Token
{
public:
    Token(TokenType type, size_t lineno, const std::string& str)
        : m_type(type)
        , m_lineno(lineno)
        , m_str(str)
    {
        token_alive_count(true);

        if (type == TOK_INT)
            m_int = std::atoi(str.c_str());
        else
            m_int = 0;
    }

    virtual ~Token()
    {
        token_alive_count(false);
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

    void print() const
    {
        printf("%s", dump().c_str());
        fflush(stdout);
    }

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
        for (size_t i = 0; i < m_tokens.size(); ++i)
        {
            delete m_tokens[i];
        }
    }

    void add(Token *token)
    {
        m_tokens.push_back(token);
    }

    void add(TokenType type, int line, const std::string& str)
    {
        add(new Token(type, line, str));
    }

    bool do_lexical(const char *input);

    bool do_lexical(const std::string& input)
    {
        return do_lexical(input.c_str());
    }

    int get_error() const
    {
        return m_error;
    }

    AstContainer *do_parse();

    Token *operator[](size_t index)
    {
        assert(index < size());
        return m_tokens[index];
    }

    const Token *operator[](size_t index) const
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

    Token *token()
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

    void print() const
    {
        printf("%s", dump().c_str());
        fflush(stdout);
    }

protected:
    std::vector<Token *> m_tokens;
    int m_error;
    size_t m_index;

    AstContainer *visit_translation_unit();
    AstBase *visit_expression();
    AstInt *visit_integer_literal();
    AstStr *visit_string_literal();
    AstContainer *visit_array_literal();
    AstContainer *visit_call(const std::string& name);
    AstContainer *visit_expression_list(AstType type, const std::string& name = "");
};

//////////////////////////////////////////////////////////////////////////////
// AstBase

void ast_alive_count(bool add);

class AstBase
{
public:
    virtual ~AstBase()
    {
        ast_alive_count(false);
    }

    AstType& get_type()
    {
        return m_type;
    }

    virtual std::string dump() const = 0;

    virtual AstBase *clone() const = 0;

    virtual AstBase *eval() const = 0;

    void print() const
    {
        printf("%s", dump().c_str());
        fflush(stdout);
    }

protected:
    AstType m_type;

    AstBase(AstType type) : m_type(type)
    {
        ast_alive_count(true);
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
    AstInt(int value = 0)
        : AstBase(AST_INT)
        , m_value(value)
    {
    }

    int& get_int()
    {
        return m_value;
    }

    virtual std::string dump() const
    {
        return mstr_to_string(m_value);
    }

    virtual AstBase *clone() const
    {
        return new AstInt(m_value);
    }

    virtual AstBase *eval() const
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
    AstStr(const std::string& str)
        : AstBase(AST_STR)
        , m_str(str)
    {
    }

    std::string& get_str()
    {
        return m_str;
    }

    virtual std::string dump() const
    {
        std::string ret = "\"";
        ret += m_str;
        ret += "\"";
        return ret;
    }

    virtual AstBase *clone() const
    {
        return new AstStr(m_str.c_str());
    }

    virtual AstBase *eval() const
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
    AstVar(const std::string& name)
        : AstBase(AST_VAR)
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

    virtual std::string dump() const
    {
        std::string ret = "(AST_VAR, ";
        ret += m_name;
        ret += ")";
        return ret;
    }

    virtual AstBase *clone() const
    {
        return new AstVar(m_name);
    }

    virtual AstBase *eval() const
    {
        return EGA_eval_var(m_name);
    }

protected:
    std::string m_name;
    AstBase *m_value;
};

//////////////////////////////////////////////////////////////////////////////
// AstContainer

AstBase *EGA_eval_fn(const std::string& name, const std::vector<AstBase *>& args);

class AstContainer : public AstBase
{
public:
    AstContainer(AstType type, const std::string& str = "")
        : AstBase(type)
        , m_str(str)
    {
        assert(type == AST_ARRAY || type == AST_CALL || type == AST_PROGRAM);
    }

    ~AstContainer()
    {
        for (size_t i = 0; i < m_children.size(); ++i)
        {
            delete m_children[i];
        }
    }

    AstBase *operator[](size_t index)
    {
        assert(index < size());
        return m_children[index];
    }

    const AstBase *operator[](size_t index) const
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

    void add(AstBase *ast)
    {
        m_children.push_back(ast);
    }

    std::vector<AstBase *>& children()
    {
        return m_children;
    }

    std::string& get_str()
    {
        return m_str;
    }

    virtual std::string dump() const
    {
        std::string ret = "{ ";
        if (size() > 0)
        {
            ret += m_children[0]->dump();
            for (size_t i = 1; i < size(); ++i)
            {
                ret += ", ";
                ret += m_children[i]->dump();
            }
        }
        ret += " }";
        return ret;
    }

    virtual AstBase *clone() const
    {
        AstContainer *ret = new AstContainer(m_type, m_str);
        for (size_t i = 0; i < size(); ++i)
        {
            ret->add(m_children[i]->clone());
        }
        return ret;
    }

    virtual AstBase *eval() const
    {
        switch (m_type)
        {
        case AST_ARRAY:
            if (AstContainer *ret = new AstContainer(AST_ARRAY, "array"))
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
            if (size())
            {
                AstBase *ptr = m_children[0]->eval();
                for (size_t i = 1; i < size(); ++i)
                {
                    delete ptr;
                    ptr = m_children[i]->eval();
                }
                return ptr;
            }
            return NULL;

        default:
            assert(0);
            break;
        }

        return NULL;
    }

protected:
    std::string m_str;
    std::vector<AstBase *> m_children;
};

typedef AstContainer AstCall;
typedef AstContainer AstArray;
typedef AstContainer AstProgram;

//////////////////////////////////////////////////////////////////////////////
// global functions

bool do_lexical(TokenStream& stream, const std::string& input);
AstContainer *do_parse(TokenStream& stream);
AstContainer *do_parse(const std::string& input);

bool EGA_init(void);
void EGA_uninit(void);

#endif  // ndef EGA_HPP_
