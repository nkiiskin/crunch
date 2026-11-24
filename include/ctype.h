#ifndef CRUNCH_CTYPE_H
#define CRUNCH_CTYPE_H

/**
 * @brief Tests whether a character is alphabetic or a decimal digit.
 * @param character An unsigned-char value or EOF.
 * @return Nonzero if the character is alphanumeric; otherwise zero.
 */
int isalnum(int character);

/**
 * @brief Tests whether a character is alphabetic.
 * @param character An unsigned-char value or EOF.
 * @return Nonzero if the character is alphabetic; otherwise zero.
 */
int isalpha(int character);

/**
 * @brief Tests whether a character is a horizontal blank.
 * @param character An unsigned-char value or EOF.
 * @return Nonzero for space or horizontal tab; otherwise zero.
 */
int isblank(int character);

/**
 * @brief Tests whether a character is a control character.
 * @param character An unsigned-char value or EOF.
 * @return Nonzero if the character is a control character; otherwise zero.
 */
int iscntrl(int character);

/**
 * @brief Tests whether a character is a decimal digit.
 * @param character An unsigned-char value or EOF.
 * @return Nonzero if the character is a decimal digit; otherwise zero.
 */
int isdigit(int character);

/**
 * @brief Tests whether a character is printable other than space.
 * @param character An unsigned-char value or EOF.
 * @return Nonzero if the character is graphical; otherwise zero.
 */
int isgraph(int character);

/**
 * @brief Tests whether a character is lowercase.
 * @param character An unsigned-char value or EOF.
 * @return Nonzero if the character is lowercase; otherwise zero.
 */
int islower(int character);

/**
 * @brief Tests whether a character is printable.
 * @param character An unsigned-char value or EOF.
 * @return Nonzero if the character is printable; otherwise zero.
 */
int isprint(int character);

/**
 * @brief Tests whether a character is printable punctuation.
 * @param character An unsigned-char value or EOF.
 * @return Nonzero if the character is punctuation; otherwise zero.
 */
int ispunct(int character);

/**
 * @brief Tests whether a character is whitespace.
 * @param character An unsigned-char value or EOF.
 * @return Nonzero if the character is whitespace; otherwise zero.
 */
int isspace(int character);

/**
 * @brief Tests whether a character is uppercase.
 * @param character An unsigned-char value or EOF.
 * @return Nonzero if the character is uppercase; otherwise zero.
 */
int isupper(int character);

/**
 * @brief Tests whether a character is a hexadecimal digit.
 * @param character An unsigned-char value or EOF.
 * @return Nonzero if the character is a hexadecimal digit; otherwise zero.
 */
int isxdigit(int character);

/**
 * @brief Converts an uppercase character to lowercase when possible.
 * @param character An unsigned-char value or EOF.
 * @return The lowercase character, or the unchanged input.
 */
int tolower(int character);

/**
 * @brief Converts a lowercase character to uppercase when possible.
 * @param character An unsigned-char value or EOF.
 * @return The uppercase character, or the unchanged input.
 */
int toupper(int character);

#endif /* CRUNCH_CTYPE_H */
