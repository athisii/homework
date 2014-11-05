/*** PL0 compiler with code generation */

#ifdef CPP_BUILDER

#include <vcl.h>
#pragma hdrstop
#include "Unit1.h"
#pragma package(smart_init)
#pragma resource "*.dfm"

TForm1 *Form1;

#define GET_STRING_LENGTH(s) (s.Length())

#else

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <string>
#include <sstream>

// Mock C++ builder's String.
using std::string;
typedef string String;

#define GET_STRING_LENGTH(s) (s.length())

string IntToStr(int n) {
    std::stringstream ss;
    ss << n;
    return ss.str();
}

#endif  /* #ifdef CPP_BUILDER */


//------------------------------------------------------------------------
// Global Constants
//------------------------------------------------------------------------
#define NORW        19          /* Counts of reserviced keywords. */
#define IDMAX       10          /* Maximum length of identitys. */
#define TXMAX       100         /* Size of symbol table. */
#define LINEMAX     81          /* Maximum length of program line. */
#define AMAX        2047        /* Maximum address. */
#define LEVMAX      3           /* Maximum nesting procedure level. */
#define CXMAX       200         /* Size of code storage. */
#define STACK_MAX   500         /* Size of interpreter stack. */

#define NMAX        10          /* Maximum number of integer digits. */
#define EXIT_FRAMES_SIZE 100    /* Exit frame maxium size. */
#define CONSTANT_TABLE_SIZE 100 /* Maximum size of constant taable. */
#define INTEGER_MAX 2147483647  /* Maximum integer. */
//------------------------------------------------------------------------


//------------------------------------------------------------------------
// Language Components
//------------------------------------------------------------------------
typedef enum {
    // basic symbols
    SYM_NUL, SYM_IDENT, SYM_INTEGER, SYM_CHAR, SYM_FLOAT,

    // operators
    SYM_PLUS, SYM_MINUS, SYM_TIMES, SYM_OVER,
    SYM_ODD, SYM_EQL, SYM_NEQ, SYM_LSS, SYM_LEQ, SYM_GTR, SYM_GEQ,
    SYM_LPAREN, SYM_RPAREN, SYM_COMMA, SYM_SEMICOLON, SYM_PERIOD,
    SYM_ASSIGN, SYM_ADD_ASSIGN, SYM_SUB_ASSIGN, SYM_MUL_ASSIGN, SYM_DIV_ASSIGN,
    SYM_AND, SYM_OR, SYM_NOT,

    // keywords
    SYM_CONST, SYM_VAR, SYM_PROG, SYM_PROC,
    SYM_BEGIN, SYM_END, SYM_RETURN,
    SYM_IF, SYM_THEN, SYM_ELSE,
    SYM_WHILE, SYM_DO, SYM_UNTIL, SYM_FOR, SYM_STEP,
    SYM_WRITE, SYM_READ, SYM_CALL
} SYMBOL;

// symbols in string form.
const char *SYMOUT[] = {
    "NUL", "IDENT", "INTEGER", "CHAR", "FLOAT",

    "PLUS", "MINUS", "TIMES", "OVER",
    "ODD", "EQ", "NEQ", "LSS", "LEQ", "GTR", "GEQ",
    "LPAREN", "RPAREN", "COMMA", "SEMICOLON", "PERIOD",
    "ASSIGN", "ADD_ASSIGN", "SUB_ASSIGN", "MUL_ASSIGN", "DIV_ASSIGN",
    "AND", "OR", "NOT",

    "CONST", "VAR", "PROGRAM", "PROCEDURE",
    "BEGIN", "END", "RETURN",
    "IF", "THEN", "ELSE",
    "WHILE", "DO", "UNTIL", "FOR", "STEP",
    "WRITE", "READ", "CALL"
};

#define SYMBOLS_COUNT ((int) (sizeof(SYMOUT) / sizeof(SYMOUT[0])))

// object type
typedef enum {
    KIND_CONSTANT,
    KIND_VARIABLE,
    KIND_PROCEDURE
} OBJECT_KIND;

typedef enum {
    STATE_RUNNING,
    STATE_STOP
} MACHINE_STATE;

// machine data type
typedef enum {
    TYPE_ADDRESS,
    TYPE_INT,
    TYPE_CHAR,
    TYPE_FLOAT
} DATA_TYPE;


typedef char ALFA[IDMAX+1];         /* identity container */

typedef struct {
    ALFA NAME;
    OBJECT_KIND KIND;
    struct {
        int LEVEL, ADDRESS, SIZE;
    } vp;
} TABLE_ITEM;                       /* symbol table item */

typedef struct {
    DATA_TYPE type;
    union {
        int ival;                    /* address / int / bool */
        char cval;                   /* char */
        float fval;                  /* float */
    };
} DATUM;                            /* machine datum */
//------------------------------------------------------------------------


//------------------------------------------------------------------------
// Machine
//------------------------------------------------------------------------
// instruction function type
typedef enum {      /* TYP L A -- INSTRUCTION FORMAT */
    LIT,            /* LIT 0 A -- LOAD CONTANT FROM ADDRESS A */
    OPR,            /* OPR 0 A -- EXECUTE OPR A */
    LOD,            /* LOD L A -- LOAD VARIABLE L FROM A */
    STO,            /* STO L A -- STORE VARIABLE L TO A */
    MST,            /* MST L 0 -- MARK STACK FRAME */
    CAL,            /* CAL L A -- CALL PROCEDURE A WITH ARGSIZE L */
    INI,            /* INI 0 A -- SET SP WITH MP + A */
    JMP,            /* JMP 0 A -- JUMP TO A */
    JPC,            /* JPC 0 A -- (CONDITIONAL) JUMP TO A */
    HLT,            /* HLT 0 0 -- HALT MACHINE */
    NO_OP           /* MARKER */
} FUNCTION_TYPE;

#define INST_COUNT (NO_OP)

typedef struct {
    FUNCTION_TYPE F;
    int L, A;
} INSTRUCTION;


// Stack Frame
//                                                          (high address)
// +----------------------------------------------------+ <- sp
// |                variables (optional)                |
// +----------------------------------------------------+
// |                constants (optional)                |
// +----------------------------------------------------+
// |                args (optional)                     |
// +----------------------------------------------------+
// |                return address                      |
// +----------------------------------------------------+
// |            dynamic link (to caller's frame)        |
// +----------------------------------------------------+
// |          static link (to parent block's frame)     |
// +----------------------------------------------------+
// |                return value (optional)             |
// +----------------------------------------------------+ <- mp
//                                                          (low address)
#define STACK_FRAME_SIZE 4
//------------------------------------------------------------------------


//------------------------------------------------------------------------
// Global Variables
//------------------------------------------------------------------------
char CH;                        /* last read character */
SYMBOL SYM;                     /* last read symbol */
ALFA ID;                        /* last read identity */
int INTEGER;                    /* last read integer */
char CHAR;                      /* last read char */
float FLOAT;                    /* last read float */

// input buffer state
int CC;                         /* line buffer index */
int LL;                         /* line buffer length */
int LINENO;                     /* current line number */
char LINE[LINEMAX];             /* current line buffer */

int CX;                         /* code storage index */
INSTRUCTION CODE[CXMAX];        /* code storage */

ALFA KW_ALFA[NORW + 1];         /* keywords table, used for ident matching */
SYMBOL KW_SYMBOL[NORW + 1];     /* keyword symbols table, used for ident matching */
SYMBOL ASCII_SYMBOL[128];       /* ascii character symbol table */
ALFA INST_ALFA[INST_COUNT];     /* machine instruction table */

TABLE_ITEM TABLE[TXMAX];        /* symbols table */
DATUM INTER_STACK[STACK_MAX];   /* interpreter stack */

DATUM CONSTANT_TABLE[TXMAX];    /* constant values table */
int CONSTANT_TABLE_INDEX;       /* constant table index */

// TODO exit frame structure doc
int *EXIT_FRAMES[EXIT_FRAMES_SIZE]; /* exit frames list */
int CURRENT_EXIT_FRAME;             /* current exit frame index */


FILE *FIN, *FOUT, *UIN;         /* STDIN/STDOUT/USER STDIN file object */
//------------------------------------------------------------------------


//------------------------------------------------------------------------
// Utilities
//------------------------------------------------------------------------

// Write a line to OUTPUT
//
// Default destination is FOUT,
// If CPP_BUILDER is defined, also write to the FORM.
void _writeln(const char *fmt, va_list args)
{
    vfprintf(FOUT, fmt, args);
    fprintf(FOUT, "\n");

#ifdef CPP_BUILDER
    Form1->vprintfs(fmt, args);
#endif /* #ifdef CPP_BUILDER */
}

// Write to OUTPUT
//
// Default destination is FOUT,
// If CPP_BUILDER is defined, also write to the FORM.
void _writeln(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    _writeln(fmt, args);
    va_end(args);
}

// Read from user input.
void _scanf(const char *fmt, ...)
{
    va_list args;

#ifdef CPP_BUILDER
    String input;
    
    input = InputBox("输入", "请键盘输入：", 0);
    va_start(args, fmt);
    vsscanf(input.c_str(), fmt, args);
    va_end(args);
#else

    _writeln("Please input:");
    va_start(args, fmt);
    vfscanf(UIN, fmt, args);
    va_end(args);
#endif /* #ifdef CPP_BUILDER */
}

// Don't panic!
void panic(int errorcode, const char *fmt, ...)
{
    va_list args;

    _writeln("error: %d", errorcode);

    va_start(args, fmt);
    _writeln(fmt, args);
    va_end(args);

#ifndef CPP_BUILDER
    exit(errorcode);
#endif  /* #ifdef CPP_BUILDER */
}

const std::string current_date_time()
{
    time_t now;
    struct tm datetime;
    char buf[80];

    now = time(0);
    datetime = *localtime(&now);
    strftime(buf, sizeof(buf), "%x %X", &datetime);

    return buf;
}

// Print something.
void log(const char *content)
{
    _writeln("//------------------------------------------------------------------------");
    _writeln("// %s", content);
    _writeln("// %s", current_date_time().c_str());
    _writeln("//------------------------------------------------------------------------");
}

// List generated instructions start from CX0.
void ListCode(int CX0)
{
    int i;
    String s;
    INSTRUCTION inst;

    for (i = CX0; i < CX; i++) {
        inst = CODE[i];

        s = IntToStr(i);
        while (GET_STRING_LENGTH(s) < 3)
            s = " " + s;
        s = s + " " + INST_ALFA[inst.F] + " " + IntToStr(inst.L) + " " + IntToStr(inst.A);
        _writeln(s.c_str());
    }
}
//------------------------------------------------------------------------


//------------------------------------------------------------------------
// Lexer
//------------------------------------------------------------------------
void ResetLineBuffer()
{
    LL = 0;
    CC = 0;
    CH = ' ';
}

void ResetLexer()
{
    ResetLineBuffer();

    LINENO = 0;
}

void GetCh()
{
    String s;

    if (CC == LL) {
        if (feof(FIN))
            panic(0, "unexpected EOF, maybe program is incomplete");

        ResetLineBuffer();
        LINENO = LINENO + 1;

        // read a line
        while (!feof(FIN) && CH != '\n') {
            CH = fgetc(FIN);
            LINE[LL++] = CH;
        }
        LINE[LL - 1] = ' '; LINE[LL] = 0;

        // list program.
        s = IntToStr(LINENO);
        while (GET_STRING_LENGTH(s) < 3)
            s = " " + s;
        s = s + " " + LINE;

        _writeln(s.c_str());
    }

    CH = LINE[CC++];
}

void GetSym()
{
    int i;
    float base;
    ALFA ident;

    // skip whitespaces
    while (isspace(CH))
        GetCh();

    if (isalpha(CH)) {  /* ident/keyword */
        i = 0;
        do {
            if (i < IDMAX)
                ident[i++] = CH;
            GetCh();
        } while (isalnum(CH));
        ident[i] = 0;
        strcpy(ID, ident);
        SYM = SYM_IDENT;

        // check if the ident is a keyword
        for (i = 0; i < NORW; i++)
            if (strcasecmp(ID, KW_ALFA[i]) == 0) {
                SYM = KW_SYMBOL[i];
                break;
            }
    } /* ident/keyword */

    else if (isdigit(CH)) {   /* decimal integer / float */
        SYM = SYM_INTEGER;
        i = 0; INTEGER = 0;
        do {
            INTEGER = 10 * INTEGER + (CH - '0');
            i++;
            GetCh();
            if (i > NMAX)
                panic(30, "GetSym: integer too large: %d", INTEGER);
        } while (isdigit(CH));

        if (CH == '.') {
            GetCh();
            if (!isdigit(CH)) {
                panic(0, "GetSym: malform fixed-point float representation");
            }
            FLOAT = INTEGER;
            INTEGER = 0;
            SYM = SYM_FLOAT;
            base = 0.1;
            do {
                FLOAT = FLOAT + base * (CH - '0');
                base = base / 10;
                GetCh();
            } while (isdigit(CH));
        }
    } /* decimal integer */

    else if (CH == '\'') {  /* char */
        GetCh();
        SYM = SYM_CHAR;
        CHAR = CH;
        GetCh();
        if (CH != '\'')
            panic(0, "GetSym: expected ', got: %c", CH);
        GetCh();
    } /* char */

    else if (CH == ':') {   /* assignment */
        SYM = SYM_NUL;
        
        GetCh();
        if (CH == '=') {
            GetCh();
            SYM = SYM_ASSIGN;
        }
    } /* assignment */

    else if (CH == '+') {   /* plus/add assignment */
        SYM = SYM_PLUS;

        GetCh();
        if (CH == '=') {
            GetCh();
            SYM = SYM_ADD_ASSIGN;
        }
    } /* plus/add assignment */

    else if (CH == '-') {   /* minus/sub assignment */
        SYM = SYM_MINUS;

        GetCh();
        if (CH == '=') {
            GetCh();
            SYM = SYM_SUB_ASSIGN;
        }
    } /* minus/sub assignment */

    else if (CH == '*') {   /* times/multi assignment */
        SYM = SYM_TIMES;

        GetCh();
        if (CH == '=') {
            GetCh();
            SYM = SYM_MUL_ASSIGN;
        }
    } /* times/multi assignment */

    else if (CH == '/') {   /* over/div assignment/comment */
        SYM = SYM_OVER;

        GetCh();
        if (CH == '=') {
            GetCh();
            SYM = SYM_DIV_ASSIGN;
        } else if (CH == '*') {
            GetCh();

            // skip comments
            while (1) {
                if (CH == '*') {
                    GetCh();
                    if (CH == '/') {
                        GetCh();
                        break;  /* while (1) */
                    }
                }

                GetCh();
            }

            // Restart tokenizing.
            GetSym();
        }
    } /* over/div assignment */

    else if (CH == '>') {   /* >/>= */
        SYM = SYM_GTR;

        GetCh();
        if (CH == '=') {
            GetCh();
            SYM = SYM_GEQ;
        }
    } /* >/>= */

    else if (CH == '<') {   /* </<=/<> */
        SYM = SYM_LSS;

        GetCh();
        if (CH == '=') {
            GetCh();
            SYM = SYM_LEQ;
        } else if (CH == '>') {
            GetCh();
            SYM = SYM_NEQ;
        }
    } /* </<= */

    else if (CH == '&') {   /* && */
        GetCh();

        if (CH == '&') {
            GetCh();
            SYM = SYM_AND;
        } else {
            SYM = SYM_NUL;
        }
    } /* && */

    else if (CH == '|') {   /* || */
        GetCh();

        if (CH == '|') {
            GetCh();
            SYM = SYM_OR;
        } else {
            SYM = SYM_NUL;
        }
    } /* || */

    else {  /* ascii character symbols */
        SYM = ASCII_SYMBOL[(int) CH];
        GetCh();
    }
}
//------------------------------------------------------------------------


//------------------------------------------------------------------------
// Datum
//------------------------------------------------------------------------
inline void datum_set_value(DATUM &d, float fval)
{
    d.type = TYPE_FLOAT;
    d.fval = fval;
}

inline void datum_set_value(DATUM &d, int ival)
{
    d.type = TYPE_INT;
    d.ival = ival;
}

inline void datum_set_value(DATUM &d, char cval)
{
    d.type = TYPE_CHAR;
    d.cval = cval;
}

inline float datum_cast_float(DATUM d)
{
    switch (d.type) {
        case TYPE_FLOAT:
            return d.fval;
        case TYPE_ADDRESS:
        case TYPE_INT:
            return (float) d.ival;
        case TYPE_CHAR:
            return (float) d.cval;
        default:
            panic(0, "datum_cast_float: unable to cast type: %d to float", d.type);
    }
}

inline int datum_cast_int(DATUM d)
{
    switch (d.type) {
        case TYPE_FLOAT:
            return (int) d.fval;
        case TYPE_ADDRESS:
        case TYPE_INT:
            return d.ival;
        case TYPE_CHAR:
            return (int) d.cval;
        default:
            panic(0, "datum_cast_int: unable to cast type: %d to int", d.type);
    }
}

inline char datum_cast_char(DATUM d)
{
    switch (d.type) {
        case TYPE_FLOAT:
            return (char) d.fval;
        case TYPE_ADDRESS:
        case TYPE_INT:
            return (char) d.ival;
        case TYPE_CHAR:
            return d.cval;
        default:
            panic(0, "datum_cast_char: unable to cast type: %d to char", d.type);
    }
}

inline char datum_cast_address(DATUM d)
{
    switch (d.type) {
        case TYPE_FLOAT:
            return (int) d.fval;
        case TYPE_ADDRESS:
        case TYPE_INT:
            return d.ival;
        case TYPE_CHAR:
            return (int) d.cval;
        default:
            panic(0, "datum_cast_address: unable to cast type: %d to int", d.type);
    }
}

inline void datum_copy(DATUM from, DATUM &to)
{
    to.type = from.type;
    switch (to.type) {
        case TYPE_FLOAT:
            to.fval = from.fval;
            break;
        case TYPE_INT:
        case TYPE_ADDRESS:
            to.ival = from.ival;
            break;
        case TYPE_CHAR:
            to.cval = from.cval;
            break;
        default:
            panic(0, "datum_copy: unknown datum type: %d", to.type);
    }
}
//------------------------------------------------------------------------

//------------------------------------------------------------------------
// Machine
//------------------------------------------------------------------------
// Generate instruction.
// Returns new instruction filled address.
int GEN(FUNCTION_TYPE F, int L, int A)
{
    int filled_cx;

    if (CX > CXMAX)
        panic(0, "program too long");
    filled_cx = CX;

    CODE[CX].F = F;
    CODE[CX].L = L;
    CODE[CX].A = A;
    CX = CX + 1;

    return filled_cx;
}

// Calculate static link with mp in specify level.
int get_static_link(int level, int mp, DATUM dstore[])
{
    int sl = mp + 1;

    while (level > 0) {
        if (dstore[sl].type != TYPE_ADDRESS)
            panic(0, "get_static_link: datum should be address type");
        sl = dstore[sl].ival;
        level = level - 1;
    }

    return sl;
}

//------------------------------------------------------------------------
// Arithmetic Operations
//
// Promotion Rules:
// 
// 1. If either operand is float, the other is converted to float,
//    target operand is converted to float.
// 2. Otherwise, both operands have type int, target operand
//    is converted to int.
//------------------------------------------------------------------------
// OPR 1 (- A)
inline void inst_inverse(DATUM src, DATUM &dest)
{
    dest.type = src.type;
    switch (src.type) {
        case TYPE_FLOAT:
            dest.type = TYPE_FLOAT;
            datum_set_value(dest, - src.fval);
            break;
        default:
            dest.type = TYPE_INT;
            datum_set_value(dest, - src.ival);
            break;
    }
}

// OPR 2 (A + B)
inline void inst_add(DATUM a, DATUM b, DATUM &c)
{
    if (a.type == TYPE_FLOAT || b.type == TYPE_FLOAT) {
        c.type = TYPE_FLOAT;
        datum_set_value(c, datum_cast_float(a) + datum_cast_float(b));
    } else {
        c.type = TYPE_INT;
        datum_set_value(c, datum_cast_int(a) + datum_cast_int(b));
    }
}

// OPR 3 (A - B)
inline void inst_sub(DATUM a, DATUM b, DATUM &c)
{
    if (a.type == TYPE_FLOAT || b.type == TYPE_FLOAT) {
        c.type = TYPE_FLOAT;
        datum_set_value(c, datum_cast_float(a) - datum_cast_float(b));
    } else {
        c.type = TYPE_INT;
        datum_set_value(c, datum_cast_int(a) - datum_cast_int(b));
    }
}

// OPR 4 (A * B)
inline void inst_mul(DATUM a, DATUM b, DATUM &c)
{
    if (a.type == TYPE_FLOAT || b.type == TYPE_FLOAT) {
        c.type = TYPE_FLOAT;
        datum_set_value(c, datum_cast_float(a) * datum_cast_float(b));
    } else {
        c.type = TYPE_INT;
        datum_set_value(c, datum_cast_int(a) * datum_cast_int(b));
    }
}

// OPR 5 (A / B)
inline void inst_div(DATUM a, DATUM b, DATUM &c)
{
    if (a.type == TYPE_FLOAT || b.type == TYPE_FLOAT) {
        c.type = TYPE_FLOAT;
        datum_set_value(c, datum_cast_float(a) / datum_cast_float(b));
    } else {
        c.type = TYPE_INT;
        datum_set_value(c, datum_cast_int(a) / datum_cast_int(b));
    }
}

// OPR 6 (ODD A)
inline void inst_odd(DATUM src, DATUM &dest)
{
    if (src.type != TYPE_INT)
        panic(0, "inst_odd: opreand should have type int, got: %d", src.type);
    dest.type = TYPE_INT;
    datum_set_value(dest, datum_cast_int(src) % 2 == 1);
}

// OPR 7 (!A)
inline void inst_not(DATUM src, DATUM &dest)
{
    int value;

    dest.type = TYPE_INT;
    switch (src.type) {
        case TYPE_FLOAT:
            value = !src.fval;
            break;
        default:
            value = !src.ival;
    }
    datum_set_value(dest, value);
}

// OPR 8 (A == B)
inline void inst_equ(DATUM a, DATUM b, DATUM &c)
{
    c.type = TYPE_INT;
    if (a.type == TYPE_FLOAT || b.type == TYPE_FLOAT)
        datum_set_value(c, datum_cast_float(a) == datum_cast_float(b));
    else
        datum_set_value(c, datum_cast_int(a) == datum_cast_int(b));
}

// OPR 9 (A != B)
inline void inst_neq(DATUM a, DATUM b, DATUM &c)
{
    c.type = TYPE_INT;
    if (a.type == TYPE_FLOAT || b.type == TYPE_FLOAT)
        datum_set_value(c, datum_cast_float(a) != datum_cast_float(b));
    else
        datum_set_value(c, datum_cast_int(a) != datum_cast_int(b));
}

// OPR 10 (A < B)
inline void inst_lss(DATUM a, DATUM b, DATUM &c)
{
    c.type = TYPE_INT;
    if (a.type == TYPE_FLOAT || b.type == TYPE_FLOAT)
        datum_set_value(c, datum_cast_float(a) < datum_cast_float(b));
    else
        datum_set_value(c, datum_cast_int(a) < datum_cast_int(b));
}

// OPR 11 (A >= B)
inline void inst_geq(DATUM a, DATUM b, DATUM &c)
{
    c.type = TYPE_INT;
    if (a.type == TYPE_FLOAT || b.type == TYPE_FLOAT)
        datum_set_value(c, datum_cast_float(a) >= datum_cast_float(b));
    else
        datum_set_value(c, datum_cast_int(a) >= datum_cast_int(b));
}

// OPR 12 (A <= B)
inline void inst_leq(DATUM a, DATUM b, DATUM &c)
{
    c.type = TYPE_INT;
    if (a.type == TYPE_FLOAT || b.type == TYPE_FLOAT)
        datum_set_value(c, datum_cast_float(a) <= datum_cast_float(b));
    else
        datum_set_value(c, datum_cast_int(a) <= datum_cast_int(b));
}

// OPR 13 (A > B)
inline void inst_gtr(DATUM a, DATUM b, DATUM &c)
{
    c.type = TYPE_INT;
    if (a.type == TYPE_FLOAT || b.type == TYPE_FLOAT)
        datum_set_value(c, datum_cast_float(a) > datum_cast_float(b));
    else
        datum_set_value(c, datum_cast_int(a) > datum_cast_int(b));
}

// OPR 14 (write to stdout with line break)
inline void inst_write_stdout(DATUM a)
{
    switch (a.type) {
        case TYPE_FLOAT:
            _writeln("%f", datum_cast_float(a));
            break;
        case TYPE_INT:
            _writeln("%d", datum_cast_int(a));
            break;
        case TYPE_CHAR:
            _writeln("%c", datum_cast_char(a));
            break;
        default:
            panic(0, "inst_write_stdout: unknow data type: %d", a.type);
    }
}

// OPR 16 (scan input from stdin)
// TODO support float / char...
inline void inst_read_stdin(DATUM &a)
{
    int input;

    _scanf("%d", &input);
    datum_set_value(a, input);
}

// OPR 17 (A && B)
inline void inst_cond_and(DATUM a, DATUM b, DATUM &c)
{
    c.type = TYPE_INT;
    datum_set_value(c, datum_cast_int(a) && datum_cast_int(b));
}

// OPR 18 (A || B)
inline void inst_cond_or(DATUM a, DATUM b, DATUM &c)
{
    c.type = TYPE_INT;
    datum_set_value(c, datum_cast_int(a) || datum_cast_int(b));
}

void Interpret(int pc)
{
    int mp, sp;                             /* program registers */
    int callee_mp, sl;
    INSTRUCTION I;
    MACHINE_STATE state;

    state = STATE_RUNNING;
    sp = 0;
    mp = 1;

    do {
        I = CODE[pc++];
        switch (I.F) {
            case LIT:                           /* load constant */
                sp = sp + 1;
                datum_copy(CONSTANT_TABLE[I.A], INTER_STACK[sp]);
                break; /* case LIT */
            
            case OPR:                           /* execute operation */
                switch (I.A) {
                    case 0:                     /* return */
                        callee_mp = mp;
                        pc = datum_cast_address(INTER_STACK[callee_mp + 3]);
                        mp = datum_cast_address(INTER_STACK[callee_mp + 2]);
                        // TODO handle function type & procedure type
                        sp = callee_mp - 1;
                        break;
                    case 1:                     /* -A */
                        inst_inverse(INTER_STACK[sp], INTER_STACK[sp]);
                        break;
                    case 2:                     /* A + B */
                        sp = sp - 1;
                        inst_add(INTER_STACK[sp], INTER_STACK[sp + 1], INTER_STACK[sp]);
                        break;
                    case 3:                     /* A - B */
                        sp = sp - 1;
                        inst_sub(INTER_STACK[sp], INTER_STACK[sp + 1], INTER_STACK[sp]);
                        break;
                    case 4:                     /* A * B */
                        sp = sp - 1;
                        inst_mul(INTER_STACK[sp], INTER_STACK[sp + 1], INTER_STACK[sp]);
                        break;
                    case 5:                     /* A / B */
                        sp = sp - 1;
                        inst_div(INTER_STACK[sp], INTER_STACK[sp + 1], INTER_STACK[sp]);
                        break;
                    case 6:                     /* ODD A */
                        inst_odd(INTER_STACK[sp], INTER_STACK[sp]);
                        break;
                    case 7:                     /* !A */
                        inst_not(INTER_STACK[sp], INTER_STACK[sp]);
                        break;
                    case 8:                     /* A == B */
                        sp = sp - 1;
                        inst_equ(INTER_STACK[sp], INTER_STACK[sp + 1], INTER_STACK[sp]);
                        break;
                    case 9:                     /* A != B */
                        sp = sp - 1;
                        inst_neq(INTER_STACK[sp], INTER_STACK[sp + 1], INTER_STACK[sp]);
                        break;
                    case 10:                    /* A < B */
                        sp = sp - 1;
                        inst_lss(INTER_STACK[sp], INTER_STACK[sp + 1], INTER_STACK[sp]);
                        break;
                    case 11:                    /* >= */
                        sp = sp - 1;
                        inst_geq(INTER_STACK[sp], INTER_STACK[sp + 1], INTER_STACK[sp]);
                        break;
                    case 12:                    /* > */
                        sp = sp - 1;
                        inst_gtr(INTER_STACK[sp], INTER_STACK[sp + 1], INTER_STACK[sp]);
                        break;
                    case 13:                    /* <= */
                        sp = sp - 1;
                        inst_leq(INTER_STACK[sp], INTER_STACK[sp + 1], INTER_STACK[sp]);
                        break;
                    case 14:                    /* write to stdout */
                        inst_write_stdout(INTER_STACK[sp--]);
                        break;
                    case 15:                    /* write '\n' to stdout */
                        _writeln("");
                        break;
                    case 16:                    /* read from stdin */
                        inst_read_stdin(INTER_STACK[++sp]);
                        break;
                    case 17:                    /* A && B */
                        sp = sp - 1;
                        inst_cond_and(INTER_STACK[sp], INTER_STACK[sp + 1], INTER_STACK[sp]);
                        break;
                    case 18:                    /* A || B */
                        sp = sp - 1;
                        inst_cond_or(INTER_STACK[sp], INTER_STACK[sp + 1], INTER_STACK[sp]);
                        break;
                    default:
                        panic(0, "Interpret: unknown op code: %d", I.A);
                } /* switch I.A */
                break; /*case OPR */

            case LOD:                           /* load variable */
                sp = sp + 1;
                sl = get_static_link(I.L, mp, INTER_STACK);
                datum_copy(INTER_STACK[sl + I.A], INTER_STACK[sp]);
                break; /* case LOD */

            case STO:                           /* store variable */
                sl = get_static_link(I.L, mp, INTER_STACK);
                datum_copy(INTER_STACK[sp], INTER_STACK[sl + I.A]);
                sp = sp - 1;
                break; /* case STO */

            case MST:                           /* mark stack frame */
                // return value, set by callee
                datum_set_value(INTER_STACK[++sp], 0);
                INTER_STACK[sp].type = TYPE_INT;

                // static link
                sl = get_static_link(I.L, mp, INTER_STACK);
                datum_set_value(INTER_STACK[++sp], sl);
                INTER_STACK[sp].type = TYPE_ADDRESS;

                // dynamic link
                datum_set_value(INTER_STACK[++sp], mp);
                INTER_STACK[sp].type = TYPE_ADDRESS;

                // return address, set by CAL
                datum_set_value(INTER_STACK[++sp], 0);
                INTER_STACK[sp].type = TYPE_ADDRESS;
                break; /* case MST */

            case CAL:                           /* call procedure */
                mp = sp - STACK_FRAME_SIZE - I.L + 1;
                datum_set_value(INTER_STACK[sp - I.L], pc);
                pc = I.A;
                break; /* case CAL */

            case INI:                           /* increment */
                sp = mp + I.A;
                break; /* case INI */

            case JMP:                           /* unconditional jump */
                pc = I.A;
                break; /* case JMP */

            case JPC:                           /* false jump */
                if (! datum_cast_int(INTER_STACK[sp]))
                    pc = I.A;
                sp = sp - 1;
                break; /* case JPC */

            case HLT:                           /* halt machine */
                state = STATE_STOP;
                break; /* case HLT */

            default:
                panic(0, "Interpret: unknown instruction type: %d", I.F);
        }
    } while (state == STATE_RUNNING);
}
//------------------------------------------------------------------------


//------------------------------------------------------------------------
// Constant Table
//------------------------------------------------------------------------
// Record a integer value into constant table.
int constant_enter(int val)
{
    if (CONSTANT_TABLE_INDEX >= CONSTANT_TABLE_SIZE)
        panic(0, "CONSTANT_ENTER: table overflow");
    CONSTANT_TABLE[++CONSTANT_TABLE_INDEX].type = TYPE_INT;
    CONSTANT_TABLE[CONSTANT_TABLE_INDEX].ival = val;

    return CONSTANT_TABLE_INDEX;
}

int constant_enter(float val)
{
    if (CONSTANT_TABLE_INDEX >= CONSTANT_TABLE_SIZE)
        panic(0, "CONSTANT_ENTER: table overflow");
    CONSTANT_TABLE[++CONSTANT_TABLE_INDEX].type = TYPE_FLOAT;
    CONSTANT_TABLE[CONSTANT_TABLE_INDEX].fval = val;

    return CONSTANT_TABLE_INDEX;
}

int constant_enter(char val)
{
    if (CONSTANT_TABLE_INDEX >= CONSTANT_TABLE_SIZE)
        panic(0, "CONSTANT_ENTER: table overflow");
    CONSTANT_TABLE[++CONSTANT_TABLE_INDEX].type = TYPE_CHAR;
    CONSTANT_TABLE[CONSTANT_TABLE_INDEX].cval = val;

    return CONSTANT_TABLE_INDEX;
}
//------------------------------------------------------------------------


//------------------------------------------------------------------------
// Symbol Table
//------------------------------------------------------------------------
// Record an entity into symbol table.
// TODO DX?
// TODO clean ID.
void ENTER(OBJECT_KIND kind, int level, int &TX, int &DX)
{
    TX = TX + 1;

    strcpy(TABLE[TX].NAME, ID);
    TABLE[TX].KIND = kind;

    switch (kind) {
        case KIND_CONSTANT:
            TABLE[TX].vp.ADDRESS = CONSTANT_TABLE_INDEX;
            break;
        case KIND_VARIABLE:
            TABLE[TX].vp.LEVEL = level;
            TABLE[TX].vp.ADDRESS = DX;
            DX = DX + 1;
            break;
        case KIND_PROCEDURE:
            TABLE[TX].vp.LEVEL = level;
            break;
    }
}

// Find identity's position.
// Return 0 if not found.
int POSITION(ALFA ID, int TX)
{
    int i;

    for (i = TX; i > 0; i--)
        if (strcmp(TABLE[i].NAME, ID) == 0)
            return i;

    return 0;
}

// Get an identity.
// Panic if not found.
void GET_IDENT(ALFA id, int TX, TABLE_ITEM *item)
{
    int pos;

    pos = POSITION(id, TX);
    if (pos == 0)
        panic(0, "unable to find identity: %s", id);

    *item = TABLE[pos];
}
//------------------------------------------------------------------------


//------------------------------------------------------------------------
// Parser
//------------------------------------------------------------------------
int parse_program(int, int &);
int parse_block(int, int &);
void parse_const(int, int &, int &);
void parse_var(int, int &, int &);
void parse_procedure(int, int &, int &);
void parse_statement(int, int &);
void parse_assignment(int, int &);
void parse_if(int, int &);
void parse_while(int, int &);
void parse_for(int, int &);
// TODO make it as expression.
void parse_call(int, int &);
void parse_read(int, int &);
void parse_write(int, int &);
void parse_return(int, int &);

// expression groups
void parse_expression(int, int &);
void parse_or_cond(int, int &);
void parse_and_cond(int, int &);
void parse_relational(int, int &);
void parse_additive(int, int &);
void parse_multive(int, int &);
void parse_unary(int, int &);
void parse_factor(int, int &);


// exit frame utilites
void exit_frame_begin();
void exit_frame_add(int);
void exit_frame_end(int);


/*
 * Grammar:
 *
 *  PROGRAM-BLOCK ::= PROGRAM IDENT; BLOCK "."
 *
 * Instructions Layout:
 * +----------------------------------------------------+ <- program_body_start
 * |                                                    |
 * |                    program body                    |
 * |                                                    |
 * +----------------------------------------------------+ <- program_start (execute starts here)
 * |                    MST 0 0                         |
 * +----------------------------------------------------+
 * |                CALL program_body_start             |
 * +----------------------------------------------------+
 * |                        HLT                         |
 * +----------------------------------------------------+
 */
int parse_program(int level, int &TX)
{
    int program_block_start_cx, program_start_cx;

    if (SYM != SYM_PROG)
        panic(0, "PROGRAM-BLOCK: expect PROGRAM, got: %s", SYMOUT[SYM]);
    GetSym();

    if (SYM != SYM_IDENT)
        panic(0, "PROGRAM-BLOCK: expect program name, got: %s", SYMOUT[SYM]);
    GetSym();

    if (SYM != SYM_SEMICOLON)
        panic(5, "PROGRAM-BLOCK: expect ';', got: %s", SYMOUT[SYM]);
    GetSym();

    program_block_start_cx = parse_block(level, TX);

    if (SYM != SYM_PERIOD)
        panic(9, "PROGRAM-BLOCK: expect '.', got: %s", SYMOUT[SYM]);

    program_start_cx = GEN(MST, 0, 0);
    GEN(CAL, 0, program_block_start_cx);
    GEN(HLT, 0, 0);

    return program_start_cx;
}

/*
 * Grammar:
 *
 *   BLOCK ::= {CONST-BLOCK} {VAR-BLOCK} [PROCEDURE-BLOCK] STATEMENT
 *
 * Instructions Layout:
 * +----------------------------------------------------+
 * |                constant declarations               | (optional)
 * +----------------------------------------------------+
 * |                variable declarations               | (optional)
 * +----------------------------------------------------+
 * |                procedure declarations              | (optional)
 * +----------------------------------------------------+ <- body_start
 * |                                                    |
 * |                    block body                      |
 * |                                                    |
 * +----------------------------------------------------+
 * |        return statement (JMP body_end)             | (optional)
 * +----------------------------------------------------+
 * |        return statement (JMP body_end)             | (optional)
 * +----------------------------------------------------+ <- body_end
 * |                    body return                     |
 * +----------------------------------------------------+
 */
int parse_block(int level, int &TX)
{
    int body_start_cx, body_end_cx;
    int vairables_size;
    int DX;

    if (level > LEVMAX)
        panic(32, "BLOCK: level too deep: %d", level);

    exit_frame_begin();

    DX = 0;
    if (SYM == SYM_CONST)
        parse_const(level, TX, DX);
    if (SYM == SYM_VAR)
        parse_var(level, TX, DX);
    while (SYM == SYM_PROC)
        parse_procedure(level, TX, DX);

    // TODO calculate arg size.
    body_start_cx = GEN(INI, 0, DX + STACK_FRAME_SIZE);

    parse_statement(level, TX);                 /* block body */

    body_end_cx = GEN(OPR, 0, 0);               /* return */
    exit_frame_end(body_end_cx);

    return body_start_cx;
}

/*
 * Grammar:
 *
 *  CONST-BLOCK ::= CONST IDENT "=" INTEGER ["," IDENT "=" INTEGER] ";"
 */
void parse_const(int level, int &TX, int &DX)
{
    if (SYM != SYM_CONST)
        panic(0, "CONST-BLOCK: expect CONST, got: %s", SYMOUT[SYM]);
    GetSym();

    do {
        if (SYM != SYM_IDENT)
            panic(4, "CONST-BLOCK: expect IDENT, got: %s", SYMOUT[SYM]);
        GetSym();

        if (SYM != SYM_EQL)
            panic(4, "CONST-BLOCK: expect '=', got: %s", SYMOUT[SYM]);
        GetSym();

        if (SYM != SYM_INTEGER)
            panic(3, "CONST-BLOCK: expect INTEGER , got: %s", SYMOUT[SYM]);
        constant_enter(INTEGER);
        ENTER(KIND_CONSTANT, level, TX, DX);
        GetSym();

        if (SYM != SYM_COMMA)
            break;
        GetSym();
    } while (SYM != SYM_SEMICOLON);

    if (SYM != SYM_SEMICOLON)
        panic(0, "CONST-BLOCK: expect ';', got: %s", SYMOUT[SYM]);
    GetSym();
}

/*
 * Grammar:
 *
 *  VAR-BLOCK ::= VAR IDENT ["," IDENT] ";"
 */
void parse_var(int level, int &TX, int &DX)
{
    if (SYM != SYM_VAR)
        panic(0, "VAR-BLOCK: expect VAR, got: %s", SYMOUT[SYM]);
    GetSym();

    do {
        if (SYM != SYM_IDENT)
            panic(4, "VAR-BLOCK: expect IDENT, got: %s", SYMOUT[SYM]);
        ENTER(KIND_VARIABLE, level, TX, DX);
        GetSym();

        if (SYM != SYM_COMMA)
            break;
        GetSym();
    } while (SYM != SYM_SEMICOLON);

    if (SYM != SYM_SEMICOLON)
        panic(0, "VAR-BLOCK: expect ';', got: %s", SYMOUT[SYM]);
    GetSym();
}

/*
 * Grammar:
 *
 *  PROCEDURE-BLOCK ::= PROCEDURE IDENT ";" BLOCK ";"
 */
void parse_procedure(int level, int &TX, int &DX)
{
    if (SYM != SYM_PROC)
        panic(0, "PROCEDURE-BLOCK: expect PROCEDURE, got: %s", SYMOUT[SYM]);
    GetSym();

    if (SYM != SYM_IDENT)
        panic(4, "PROCEDURE-BLOCK: expect identity, got: %s", SYMOUT[SYM]);
    ENTER(KIND_PROCEDURE, level, TX, DX);
    GetSym();

    if (SYM != SYM_SEMICOLON)
        panic(5, "PROCEDURE-BLOCK: expect ';', got: %s", SYMOUT[SYM]);
    GetSym();

    parse_block(level + 1, TX);

    if (SYM != SYM_SEMICOLON)
        panic(5, "PROCEDURE-BLOCK: expect ';', got: %s", SYMOUT[SYM]);
    GetSym();
}

/*
 * Grammar:
 *
 *  STATEMENT ::= BEGIN STATEMENT [";" STATEMENT] END
 *              | ASSIGN-STMT
 *              | CALL-STMT
 *              | IF-STMT
 *              | WHILE-STMT
 *              | FOR-STMT
 *              | CALL-STMT
 *              | READ-STMT
 *              | WRITE-STMT
 *              | RETURN-STMT
 *              | NULL-STMT
 */
void parse_statement(int level, int &TX)
{
    switch (SYM) {
        case SYM_IDENT:
            parse_assignment(level, TX);
            break;
        case SYM_IF:
            parse_if(level, TX);
            break;
        case SYM_WHILE:
            parse_while(level, TX);
            break;
        case SYM_FOR:
            parse_for(level, TX);
            break;
        case SYM_CALL:
            parse_call(level, TX);
            break;
        case SYM_READ:
            parse_read(level, TX);
            break;
        case SYM_WRITE:
            parse_write(level, TX);
            break;
        case SYM_RETURN:
            parse_return(level, TX);
            break;
        case SYM_BEGIN:
            GetSym();

            parse_statement(level, TX);

            while (SYM == SYM_SEMICOLON) {
                GetSym();
                if (SYM != SYM_END)     /* null statement */
                    parse_statement(level, TX);
            }

            if (SYM != SYM_END)
                panic(17, "STATEMENT: expect END, got: %s", SYMOUT[SYM]);
            GetSym();

            break;
        default:
            panic(0, "STATEMENT: unexpected token: %s", SYMOUT[SYM]);
    }
}

/*
 * Grammar:
 *
 *  ASSIGNMENT ::= IDENT ASSIGNOP EXPRESSION
 *  ASSIGNOP ::= ":=" | "+=" | "-=" | "*=" | "/="
 */
void parse_assignment(int level, int &TX)
{
    TABLE_ITEM ident;
    SYMBOL assign_op;
    int op_code;
    
    if (SYM != SYM_IDENT)
        panic(0, "ASSIGNMENT: expected IDENT, got: %s", SYMOUT[SYM]);
    GET_IDENT(ID, TX, &ident);
    if (ident.KIND != KIND_VARIABLE)
        panic(12, "ASSIGNMENT: cannot assign to non-variable %s", ID);
    GetSym();

    if (SYM != SYM_ASSIGN && SYM != SYM_ADD_ASSIGN && SYM != SYM_SUB_ASSIGN
        && SYM != SYM_MUL_ASSIGN && SYM != SYM_DIV_ASSIGN)
        panic(13, "ASSIGNMENT: unexpected token: %s", SYMOUT[SYM]);
    assign_op = SYM;
    GetSym();

    if (assign_op == SYM_ASSIGN) {
        // calculate right side value
        parse_expression(level, TX);
    } else {
        // load variable
        GEN(LOD, level - ident.vp.LEVEL, ident.vp.ADDRESS);

        // calculate right side value
        parse_expression(level, TX);

        // calculate new value
        switch (assign_op) {
            case SYM_ADD_ASSIGN: op_code = 2; break;
            case SYM_SUB_ASSIGN: op_code = 3; break;
            case SYM_MUL_ASSIGN: op_code = 4; break;
            case SYM_DIV_ASSIGN: op_code = 5; break;
            default:
                panic(0, "ASSIGNMENT: unsupported operator: %s", SYMOUT[assign_op]);
        }
        GEN(OPR, 0, op_code);
    }

    // store value
    GEN(STO, level - ident.vp.LEVEL, ident.vp.ADDRESS);
}

/*
 * Grammar:
 *
 *  IF-STMT ::= IF EXPRESSION THEN STATEMENT
 *            | IF EXPRESSION THEN STATEMENT ELSE STATEMENT
 *
 * Instructions Layout:
 * +----------------------------------------------------+
 * |                                                    |
 * |                    conditions                      |
 * |                                                    |
 * +----------------------------------------------------+
 * |                JPC 0 THEN_PC                       |
 * +----------------------------------------------------+
 * |                                                    |
 * |                then-stmt                           |
 * |                                                    |
 * +----------------------------------------------------+ <- THEN_PC
 * |                JMP 0 ELSE_PC (if ELSE exists)      |
 * +----------------------------------------------------+
 * |                                                    |
 * |                else-stmt (if ELSE exists)          |
 * |                                                    |
 * +----------------------------------------------------+ <- ELSE_PC (*)
 *
 * * If ELSE is not presented, THEN_PC should be exactly ELSE_PC.
 */
void parse_if(int level, int &TX)
{
    int cond_jmp_cx, then_jmp_cx;

    if (SYM != SYM_IF)
        panic(0, "IF-STMT: expected IF, got: %s", SYMOUT[SYM]);
    GetSym();

    // parse condition
    parse_expression(level, TX);
    cond_jmp_cx = GEN(JPC, 0, 0);

    // parse then part
    if (SYM != SYM_THEN)
        panic(16, "IF-STMT: expected THEN, got: %s", SYMOUT[SYM]);
    GetSym();
    parse_statement(level, TX);

    // parse else part if exists
    if (SYM == SYM_ELSE) {
        GetSym();

        then_jmp_cx = GEN(JMP, 0, 0);
        CODE[cond_jmp_cx].A = CX;
        parse_statement(level, TX);
        CODE[then_jmp_cx].A = CX;
    } else {
        CODE[cond_jmp_cx].A = CX;
    }
}

/*
 * Grammar:
 *
 *  WHILE-STMT ::= WHILE EXPRESSION DO STATEMENT
 *
 * Instructions Layout:
 * +----------------------------------------------------+ <- WHILE_CX
 * |                                                    |
 * |                    conditions                      |
 * |                                                    |
 * +----------------------------------------------------+
 * |                    JPC 0 OUT_CX                    |
 * +----------------------------------------------------+
 * |                                                    |
 * |                    loop body                       |
 * |                                                    |
 * +----------------------------------------------------+
 * |                    JMP WHILE_CX                    |
 * +----------------------------------------------------+ <- OUT_CX
 */
void parse_while(int level, int &TX)
{
    int while_cx, cond_jmp_cx;

    if (SYM != SYM_WHILE)
        panic(0, "WHILE-STMT: expect WHILE, got: %s", SYMOUT[SYM]);
    GetSym();

    while_cx = CX;

    // parse expression
    parse_expression(level, TX);
    cond_jmp_cx = GEN(JPC, 0, 0);
    
    if (SYM != SYM_DO)
        panic(18, "WHILE-STMT: expect DO, got: %s", SYMOUT[SYM]);
    GetSym();

    // parse body
    parse_statement(level, TX);
    GEN(JMP, 0, while_cx);
    
    // back patch
    CODE[cond_jmp_cx].A = CX;
}

/*
 * Description:
 *
 *  FOR-STMT in PL/0:
 *  
 *      FOR I := 0 STEP 1 UNTIL 5 DO WRITE(I)
 *
 *  is equal to this C form:
 *
 *      for (i = 0; i <= 5; i++)
 *          write(i);
 *
 * Grammar:
 *
 *  FOR-STMT ::= FOR IDENT ":=" EXPRESSION
 *                  STEP EXPRESSION
 *                  UNTIL EXPRESSION
 *                  DO STATEMENT
 *
 * Instructions Layout:
 * +----------------------------------------------------+
 * |                                                    |
 * |                ident := expression                 |
 * |                                                    |
 * +----------------------------------------------------+
 * |                    JMP UNTIL_CX                    |
 * +----------------------------------------------------+ <- FOR_CX
 * |                                                    |
 * |                    step part                       |
 * |                                                    |
 * +----------------------------------------------------+ <- UNTIL_CX
 * |                                                    |
 * |                    until part                      |
 * |                                                    |
 * +----------------------------------------------------+
 * |                    JPC 0 OUT_CX                    |
 * +----------------------------------------------------+
 * |                                                    |
 * |                      do part                       |
 * |                                                    |
 * +----------------------------------------------------+
 * |                    JMP FOR_CX                      |
 * +----------------------------------------------------+ <- OUT_CX
 */
void parse_for(int level, int &TX)
{
    int for_cx, until_cx, jmp_until_cx, jmp_out_cx;
    TABLE_ITEM ident;

    if (SYM != SYM_FOR)
        panic(0, "FOR-STMT: expect FOR, got: %s", SYMOUT[SYM]);
    GetSym();

    // parse assign expression
    if (SYM != SYM_IDENT)
        panic(0, "FOR-STMT: expect IDENT, got: %s", SYMOUT[SYM]);
    GET_IDENT(ID, TX, &ident);
    if (ident.KIND != KIND_VARIABLE)
        panic(12, "FOR-STMT: cannot assign to non-variable %s", ID);
    GetSym();

    if (SYM != SYM_ASSIGN)
        panic(0, "FOR-STMT: expect ':=', got: %s", SYMOUT[SYM]);
    GetSym();

    // store initial value to loop variable
    parse_expression(level, TX);
    GEN(STO, level - ident.vp.LEVEL, ident.vp.ADDRESS);
    jmp_until_cx = GEN(JMP, 0, 0);

    // parse step part
    for_cx = CX;
    if (SYM != SYM_STEP)
        panic(0, "FOR-STMT: expect STEP, got: %s", SYMOUT[SYM]);
    GetSym();

    // update loop variable
    GEN(LOD, level - ident.vp.LEVEL, ident.vp.ADDRESS);
    parse_expression(level, TX);
    GEN(OPR, 0, 2);
    GEN(STO, level - ident.vp.LEVEL, ident.vp.ADDRESS);
    
    // parse until part
    until_cx = CX;
    if (SYM != SYM_UNTIL)
        panic(0, "FOR-STMT: expect UNTIL, got: %s", SYMOUT[SYM]);
    GetSym();

    // compare loop variable with upper bound
    GEN(LOD, level - ident.vp.LEVEL, ident.vp.ADDRESS);
    parse_expression(level, TX);
    GEN(OPR, 0, 13);
    jmp_out_cx = GEN(JPC, 0, 0);

    // parse do part
    if (SYM != SYM_DO)
        panic(0, "FOR-STMT: expect DO, got: %s", SYMOUT[SYM]);
    GetSym();
    parse_statement(level, TX);
    GEN(JMP, 0, for_cx);


    // back patch address
    CODE[jmp_until_cx].A = until_cx;
    CODE[jmp_out_cx].A = CX;
}

/*
 * Grammar:
 *
 *  CALL-STMT ::= CALL IDENT
 */
void parse_call(int level, int &TX)
{
    TABLE_ITEM ident;

    if (SYM != SYM_CALL)
        panic(0, "CALL: expected CALL, got: %s", SYMOUT[SYM]);
    GetSym();

    if (SYM != SYM_IDENT)
        panic(14, "CALL-STMT: expected procedure name, got: %s", SYMOUT[SYM]);
    
    GET_IDENT(ID, TX, &ident);
    if (ident.KIND != KIND_PROCEDURE)
        panic(15, "CALL-STMT: %s should be a procedure", ID);

    GEN(MST, level - ident.vp.LEVEL, 0);
    // TODO parse parameters.
    GEN(CAL, 0, ident.vp.ADDRESS);
    GetSym();
}

/*
 * Grammar:
 *
 *  READ-STMT ::= READ "(" IDENT, ["," IDENT] ")"
 */
void parse_read(int level, int &TX)
{
    TABLE_ITEM ident;

    if (SYM != SYM_READ)
        panic(0, "READ: expected READ, got: %s", SYMOUT[SYM]);
    GetSym();

    if (SYM != SYM_LPAREN)
        panic(34, "READ-STMT: expect '(', got: %s", SYMOUT[SYM]);

    do {
        GetSym();
        if (SYM != SYM_IDENT)
            panic(35, "READ-STMT: expect identity, got: %s", SYMOUT[SYM]);

        GET_IDENT(ID, TX, &ident);
        if (ident.KIND != KIND_VARIABLE)
            panic(35, "READ-STMT: identity %s should be variable", ID);
        GEN(OPR, 0, 16);
        GEN(STO, level - ident.vp.LEVEL, ident.vp.ADDRESS);
        
        GetSym();
    } while (SYM == SYM_COMMA);

    if (SYM != SYM_RPAREN)
        panic(33, "READ-STMT: expected ')', got: %s", SYMOUT[SYM]);
    GetSym();
}

/*
 * Grammar:
 *
 *  WRITE-STMT ::= WRITE "(" EXPRESSION,  ["," EXPRESSION] ")"
 *               | WRITE
 */
void parse_write(int level, int &TX)
{
    if (SYM != SYM_WRITE)
        panic(0, "WRITE-STMT: expected WRITE, got: %s", SYMOUT[SYM]);
    GetSym();

    if (SYM == SYM_LPAREN) {
        GetSym();

        parse_expression(level, TX);
        GEN(OPR, 0, 14);
        while (SYM == SYM_COMMA) {
            GetSym();

            parse_expression(level, TX);
            GEN(OPR, 0, 14);
        }

        if (SYM != SYM_RPAREN)
            panic(33, "WEITE-STMT: expected ')', got: %s", SYMOUT[SYM]);
        GetSym();
    } /* SYM == SYM_LPAREN */
    
    else {
        GEN(OPR, 0, 15);
    }
}

/*
 * Grammar:
 *
 *  TODO support function
 *  RETURN-STMT ::= RETURN
 */
void parse_return(int level, int &TX)
{
    int jmp_out_cx;

    if (SYM != SYM_RETURN)
        panic(0, "RETURN-STMT: expected RETURN, got: %s", SYMOUT[SYM]);
    GetSym();

    jmp_out_cx = GEN(JMP, 0, 0);
    exit_frame_add(jmp_out_cx);
}

void parse_expression(int level, int &TX)
{
    parse_or_cond(level, TX);
}

/*
 * Grammar:
 *
 *  OR_COND ::= AND_COND ["||" OR_COND]
 */
void parse_or_cond(int level, int &TX)
{
    parse_and_cond(level, TX);

    while (SYM == SYM_OR) {
        GetSym();

        parse_and_cond(level, TX);
        GEN(OPR, 0, 18);
    }
}

/*
 * Grammar:
 *
 *  AND_COND ::= RELATIONAL ["&&" RELATIONAL]
 */
void parse_and_cond(int level, int &TX)
{
    parse_relational(level, TX);

    while (SYM == SYM_AND) {
        GetSym();

        parse_relational(level, TX);
        GEN(OPR, 0, 17);
    }
}

/*
 * Grammar:
 *
 *  RELATIONAL ::= ADDITIVE [RELOP ADDITIVE]
 *               | ODD ADDITIVE
 *  RELOP ::= "=" / "<" / "<=" / ">" / ">=" / "<>"
 */
void parse_relational(int level, int &TX)
{
    SYMBOL rel_symbol;
    int op_code;

    if (SYM == SYM_ODD) {
        GetSym();

        parse_additive(level, TX);
        GEN(OPR, 0, 6);
    } /* SYM == SYM_ODD */

    else {
        parse_additive(level, TX);
        while (SYM == SYM_EQL || SYM == SYM_NEQ
               || SYM == SYM_LSS || SYM == SYM_LEQ
               || SYM == SYM_GTR || SYM == SYM_GEQ) {
            rel_symbol = SYM;
            GetSym();
            parse_additive(level, TX);

            switch (rel_symbol) {
                case SYM_EQL: op_code = 8;  break;
                case SYM_NEQ: op_code = 9;  break;
                case SYM_LSS: op_code = 10; break;
                case SYM_GEQ: op_code = 11; break;
                case SYM_GTR: op_code = 12; break;
                case SYM_LEQ: op_code = 13; break;
                default:
                    panic(0, "RELATIONAL: unknown operator: %s", SYMOUT[SYM]);
            }
            GEN(OPR, 0, op_code);
        }
    }
}

/*
 * Grammar:
 *
 *  ADDITIVE ::= MULTIVE ["+" / "-" MULTIVE]
 */
void parse_additive(int level, int &TX)
{
    SYMBOL op_symbol;

    parse_multive(level, TX);
    while (SYM == SYM_PLUS || SYM == SYM_MINUS) {
        op_symbol = SYM;
        GetSym();

        parse_multive(level, TX);

        if (op_symbol == SYM_PLUS)
            GEN(OPR, 0, 2);
        else
            GEN(OPR, 0, 3);
    }
}

/*
 * Grammar:
 *
 *  MULTIVE ::= UNARY ["*" / "/" UNARY]
 */
void parse_multive(int level, int &TX)
{
    SYMBOL op_symbol;

    parse_unary(level, TX);
    while (SYM == SYM_TIMES || SYM == SYM_OVER) {
        op_symbol = SYM;
        GetSym();

        parse_unary(level, TX);

        if (op_symbol == SYM_TIMES)
            GEN(OPR, 0, 4);
        else
            GEN(OPR, 0, 5);
    }
}

/*
 * Grammar:
 *
 *  UNARY ::= ["-" / "+" / "!"] UNARY
 *          | FACTOR
 */
void parse_unary(int level, int &TX)
{
    if (SYM == SYM_MINUS) {
        GetSym();
        parse_unary(level, TX);
        GEN(OPR, 0, 1);
    } /* SYM == SYM_MINUS */
    
    else if (SYM == SYM_PLUS) {
        GetSym();
        parse_unary(level, TX);
    } /* SYM == SYM_PLUS */

    else if (SYM == SYM_NOT) {
        GetSym();
        parse_unary(level, TX);
        GEN(OPR, 0, 7);
    } /* SYM == SYM_NOT */

    else {
        parse_factor(level, TX);
    }
}

/*
 * Grammar:
 *
 *  FACTOR ::= IDENT
 *           | INTEGER
 *           | FLOAT
 *           | CHAR
 *           | "(" EXPRESSION ")"
 *
 *  INTEGER ::= [0-9]+  range: [-2147483648, 2147483647]
 *  FLOAT ::= [0-9]+"."[0-9]+
 *  CHAR ::= "'" [a-zA-Z] "'"
 */
void parse_factor(int level, int &TX)
{
    TABLE_ITEM ident;

    if (SYM == SYM_IDENT) {
        GET_IDENT(ID, TX, &ident);
        switch (ident.KIND) {
            case KIND_CONSTANT:
                GEN(LIT, 0, ident.vp.ADDRESS);
                break;
            case KIND_VARIABLE:
                // TODO address calculating
                GEN(LOD, level - ident.vp.LEVEL, ident.vp.ADDRESS);
                break;
            default:
                panic(21, "FACTOR: identity %s type should be CONST or VARIABLE", ID);
        }
        GetSym();
    } /* SYM == SYM_IDENT */

    else if (SYM == SYM_INTEGER) {
        if (INTEGER > INTEGER_MAX) {
            panic(31, "FACTOR: integer too large: %d", INTEGER);
            INTEGER = 0;
        }

        GEN(LIT, 0, constant_enter(INTEGER));
        GetSym();
    } /* SYM == SYM_INTEGER */

    else if (SYM == SYM_CHAR) {
        GEN(LIT, 0, constant_enter(CHAR));
        GetSym();
    } /* SYM == SYM_CHAR */

    else if (SYM == SYM_FLOAT) {
        GEN(LIT, 0, constant_enter(FLOAT));
        GetSym();
    } /* SYM == SYM_FLOAT */

    else if (SYM == SYM_LPAREN) {
        GetSym();

        parse_expression(level, TX);

        if (SYM != SYM_RPAREN)
            panic(22, "missing `)`");
        GetSym();
    } /* SYM == SYM_LPAREN */

    else {
        panic(0, "FACTOR: unexpected symbol: %s", SYMOUT[SYM]);
    }
}

// Start a new exit frame.
void exit_frame_begin()
{
    int i;

    CURRENT_EXIT_FRAME = CURRENT_EXIT_FRAME + 1;
    EXIT_FRAMES[CURRENT_EXIT_FRAME] = (int *) malloc(sizeof(int) * EXIT_FRAMES_SIZE);
    if (EXIT_FRAMES[CURRENT_EXIT_FRAME] == NULL)
        panic(0, "exit_frame_begin: unable to alloc memory for exit frame");
    for (i = 0; i < EXIT_FRAMES_SIZE; i++)
        EXIT_FRAMES[CURRENT_EXIT_FRAME][i] = 0;
}

// Add a return jump statement to exit frame.
void exit_frame_add(int return_stmt_cx)
{
    int len;

    if (CURRENT_EXIT_FRAME < 0)
        panic(0, "exit_frame_add: should start a new exit frame first");
    len = EXIT_FRAMES[CURRENT_EXIT_FRAME][0];
    if (len >= EXIT_FRAMES_SIZE)
        panic(0, "exit_frame_add: exit frame too large");
    EXIT_FRAMES[CURRENT_EXIT_FRAME][++len] = return_stmt_cx;
    EXIT_FRAMES[CURRENT_EXIT_FRAME][0] = len;
}

// End an exit frame & backpatch return address.
void exit_frame_end(int body_end_cx)
{
    int i, len, return_stmt_cx;

    if (CURRENT_EXIT_FRAME < 0)
        panic(0, "exit_frame_end: no active exit frame");

    // backpath return address
    len = EXIT_FRAMES[CURRENT_EXIT_FRAME][0];
    for (i = 1; i <= len; i++) {
        return_stmt_cx = EXIT_FRAMES[CURRENT_EXIT_FRAME][i];
        CODE[return_stmt_cx].A = body_end_cx;
    }

    free(EXIT_FRAMES[CURRENT_EXIT_FRAME]);
    CURRENT_EXIT_FRAME = CURRENT_EXIT_FRAME - 1;
}
//------------------------------------------------------------------------


//------------------------------------------------------------------------
// Main Program
//------------------------------------------------------------------------

// Setup language components.
void SetupLanguage()
{
    int i;

    for (i = 0; i < 128; i++)
        ASCII_SYMBOL[i] = SYM_NUL;
    ASCII_SYMBOL['+'] = SYM_PLUS;       ASCII_SYMBOL['-'] = SYM_MINUS;
    ASCII_SYMBOL['*'] = SYM_TIMES;      ASCII_SYMBOL['/'] = SYM_OVER;
    ASCII_SYMBOL['('] = SYM_LPAREN;     ASCII_SYMBOL[')'] = SYM_RPAREN;
    ASCII_SYMBOL['='] = SYM_EQL;        ASCII_SYMBOL[','] = SYM_COMMA;
    ASCII_SYMBOL['.'] = SYM_PERIOD;     ASCII_SYMBOL[';'] = SYM_SEMICOLON;
    ASCII_SYMBOL['!'] = SYM_NOT;

    i = -1;
    strcpy(KW_ALFA[++i], "BEGIN");
    strcpy(KW_ALFA[++i], "CALL");
    strcpy(KW_ALFA[++i], "CONST");
    strcpy(KW_ALFA[++i], "DO");
    strcpy(KW_ALFA[++i], "ELSE");
    strcpy(KW_ALFA[++i], "END");
    strcpy(KW_ALFA[++i], "FOR");
    strcpy(KW_ALFA[++i], "IF");
    strcpy(KW_ALFA[++i], "ODD");
    strcpy(KW_ALFA[++i], "PROCEDURE");
    strcpy(KW_ALFA[++i], "PROGRAM");
    strcpy(KW_ALFA[++i], "READ");
    strcpy(KW_ALFA[++i], "RETURN");
    strcpy(KW_ALFA[++i], "STEP");
    strcpy(KW_ALFA[++i], "THEN");
    strcpy(KW_ALFA[++i], "UNTIL");
    strcpy(KW_ALFA[++i], "VAR");
    strcpy(KW_ALFA[++i], "WHILE");
    strcpy(KW_ALFA[++i], "WRITE");

    i = -1;
    KW_SYMBOL[++i] = SYM_BEGIN;
    KW_SYMBOL[++i] = SYM_CALL;
    KW_SYMBOL[++i] = SYM_CONST;
    KW_SYMBOL[++i] = SYM_DO;
    KW_SYMBOL[++i] = SYM_ELSE;
    KW_SYMBOL[++i] = SYM_END;
    KW_SYMBOL[++i] = SYM_FOR;
    KW_SYMBOL[++i] = SYM_IF;
    KW_SYMBOL[++i] = SYM_ODD;
    KW_SYMBOL[++i] = SYM_PROC;
    KW_SYMBOL[++i] = SYM_PROG;
    KW_SYMBOL[++i] = SYM_READ;
    KW_SYMBOL[++i] = SYM_RETURN;
    KW_SYMBOL[++i] = SYM_STEP;
    KW_SYMBOL[++i] = SYM_THEN;
    KW_SYMBOL[++i] = SYM_UNTIL;
    KW_SYMBOL[++i] = SYM_VAR;
    KW_SYMBOL[++i] = SYM_WHILE;
    KW_SYMBOL[++i] = SYM_WRITE;
  
    strcpy(INST_ALFA[LIT], "LIT");   strcpy(INST_ALFA[OPR], "OPR");
    strcpy(INST_ALFA[LOD], "LOD");   strcpy(INST_ALFA[STO], "STO");
    strcpy(INST_ALFA[CAL], "CAL");   strcpy(INST_ALFA[INI], "INI");
    strcpy(INST_ALFA[JMP], "JMP");   strcpy(INST_ALFA[JPC], "JPC");
    strcpy(INST_ALFA[HLT], "HLT");   strcpy(INST_ALFA[MST], "MST");

    // Setup constant table.
    CONSTANT_TABLE_INDEX = -1;           /* current constant address */

    // Setup exit frames list.
    CURRENT_EXIT_FRAME = -1;
}

// Initialize program.
void Init()
{
    SetupLanguage();
    ResetLexer();
}

void Main()
{
    int TX, pc;

    Init();

    log("Start compiling program.");
    GetSym();
    TX = 0;
    pc = parse_program(0, TX);
    ListCode(0);
    log("Compile finish.");

    log("Start executing program.");
    Interpret(pc);
    log("Execute finish.");
}

#ifdef CPP_BUILDER
void __fastcall TForm1::ButtonRunClick(TObject *Sender)
{
    if ((FIN = fopen((Form1->EditName->Text + ".PL0").c_str(), "r"))) {
        FOUT = fopen((Form1->EditName->Text + ".COD").c_str(), "w");
    }

    Main();

    fclose(FIN);
    fclose(FOUT);
}

#else

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "./main input.pl0");
        exit(1);
    }

    FOUT = stderr; UIN = stdin;
    if ((FIN = fopen(argv[1], "r")) == NULL)
        panic(0, "cannot open source file: %s", argv[1]);

    Main();

    return 0;
}

#endif /* #ifdef CPP_BUILDER */
//------------------------------------------------------------------------
