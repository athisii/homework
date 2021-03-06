#ifndef TOKEN_H
#define TOKEN_H

#define TOKEN_IDENT_MAX_LENGTH 50

enum token_type {
    /* status tokens */
    END_OF_FILE, ERROR,
    
    /* reserved keywords */
    CONST, VAR, PROCEDURE, BEGIN, END, IF, THEN, WHILE, DO, ODD, CALL,
    WRITE, READ,

    /* operators */
    ASSIGN, EQ, NEQ, LT, LEQ, GT, GEQ, LPAREN, RPAREN, SEMI, PERIOD,
    PLUS, MINUS, TIMES, OVER,

    /* characters */
    IDENT, NUM
};

struct token {
    enum token_type type;
    struct {
        int lines;
        int cols;
    } pos;
    char *value;
};



struct token *token_create(enum token_type, char *, int, int);
/* Create a token from ident, and check if it is a reserved keyword. */
struct token *token_create_from_ident(char *, int, int);
void token_destory(struct token *);
void token_print(struct token *);

#endif /* TOKEN_H */
