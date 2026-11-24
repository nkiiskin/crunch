#ifndef CRUNCH_PARSER_AST_JSON_H
#define CRUNCH_PARSER_AST_JSON_H

#include <stdio.h>
#include "parser/ast.h"

/**
 * @brief Writes the complete, versioned AST interchange format as JSON.
 * @param out FILE stream receiving the JSON document.
 * @param program AST program to serialize.
 * @return Nonzero when the complete document is written; zero on output failure.
 */
int ast_write_json(FILE *out, const AstProgram *program);

#endif
