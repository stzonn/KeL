#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"
#include "lexer_error.h"
#include "lexer_utils.h"

/*
 * L stands for "Left"
 * R stands for "Right"
 * it is refered to the position around a colon
 *
 * Q stands for "Qualifier"
 *
 * checking order:
 * 1 - is_command (important to check the L case)
 * 2 - is_qualifier_L
 * 3 - is_L
 * 4 - is_qualifier_R
 * 5 - is_R
 * 6 - is_qualifier_LR
 * 7 - is_LR
 * 8 - is_period_L
 * 9 - is_literal
 * 10 - is_special
 * 11 - is_valid_name
 *
 * qualifier cases are checked first to detect the brackets, so it is easier to detect
 * names (for instance, the name of a L or a R).
*/

// more errors will be implemented in the dedicated function
// this variable manages errors which subsequently may no longer be errors
static bool token_error = false;

static void create_token_special(
const char* restrict code,
long int start,
TokenType type,
Token* restrict token) {
	uint32_t subtype;

	switch(code[start]) {
	case '!': subtype = TokenSubtype_EXCLAMATION_MARK; break;
	case '"': subtype = TokenSubtype_DQUOTES; break;
	case '#': subtype = TokenSubtype_HASH; break;
	case '%': subtype = TokenSubtype_MODULO; break;
	case '&': subtype = TokenSubtype_AMPERSAND; break;
	case '\'': subtype = TokenSubtype_SQUOTE; break;
	case '(': subtype = TokenSubtype_LPARENTHESIS; break;
	case ')': subtype = TokenSubtype_RPARENTHESIS; break;
	case '*': subtype = TokenSubtype_ASTERISK; break;
	case '+': subtype = TokenSubtype_PLUS; break;
	case ',': subtype = TokenSubtype_COMMA; break;
	case '-': subtype = TokenSubtype_MINUS; break;
	case '.': subtype = TokenSubtype_PERIOD; break;
	case '/': subtype = TokenSubtype_DIVIDE; break;
	case ':': subtype = TokenSubtype_COLON; break;
	case ';': subtype = TokenSubtype_SEMICOLON; break;
	case '<': subtype = TokenSubtype_LOBRACKET; break;
	case '=': subtype = TokenSubtype_EQUAL; break;
	case '>': subtype = TokenSubtype_ROBRACKET; break;
	case '?': subtype = TokenSubtype_QUESTION_MARK; break;
	case '@': subtype = TokenSubtype_AT; break;
	case '[': subtype = TokenSubtype_LBRACKET; break;
	case ']': subtype = TokenSubtype_RBRACKET; break;
	case '\\': subtype = TokenSubtype_BACKSLASH; break;
	case '^': subtype = TokenSubtype_CARET; break;
	case '`': subtype = TokenSubtype_GRAVE_ACCENT; break;
	case '{': subtype = TokenSubtype_LCBRACE; break;
	case '|': subtype = TokenSubtype_PIPE; break;
	case '}': subtype = TokenSubtype_RCBRACE; break;
	case '~': subtype = TokenSubtype_TILDE; break;
	default: assert(false); // missing case
	}

	*token = (Token) {
		.type = type,
		.subtype = subtype,
		.start = start,
		.end = start + 1};
}

static void create_token_Q(
TokenType type,
TokenSubtype subtype,
long int start,
long int end,
Token* restrict token) {
	*token = (Token) {
		.type = type,
		.subtype = subtype,
		.start = start,
		.end = end};
}

static void create_token_colon_word(
TokenType type,
long int L_start,
long int L_end,
long int R_start,
long int R_end,
Token* restrict token) {
	*token = (Token) {
		.type = type,
		.subtype = TokenSubtype_NO,
		.L_start = L_start,
		.L_end = L_end,
		.R_start = R_start,
		.R_end = R_end};
}

static void create_token_literal(
TokenSubtype subtype,
long int start,
long int end,
Token* token) {
	*token = (Token) {
		.type = TokenType_LITERAL,
		.subtype = subtype,
		.start = start,
		.end = end};
}

static bool is_valid_QL(
const char* code,
TokenSubtype* subtype,
long int start,
long int end) {
	*subtype = TokenSubtype_NO;
	code = &code[start]; // for strcmp

	if(strncmp(
		"entry",
		code,
		end - start)
	== 0)
		*subtype = TokenSubtype_QL_ENTRY;
	else if(strncmp(
		"mut",
		code,
		end - start)
	== 0)
		*subtype = TokenSubtype_QL_MUT;

	return *subtype != TokenSubtype_NO;
}

static bool is_valid_QR(
const char* code,
TokenSubtype* subtype,
long int start,
long int end) {
	*subtype = TokenSubtype_NO;
	code = &code[start];

	if(strncmp(
		"default",
		code,
		end - start)
	== 0)
		*subtype = TokenSubtype_QR_DEFAULT;

	return *subtype != TokenSubtype_NO;
}

static bool if_command_create_token(
const char* code,
long int start,
Token* token) {
	if(!is_command(code[start]))
		return false;

	create_token_special(
		code,
		start,
		TokenType_COMMAND,
		token);
	return true;
}

static bool get_QL(
const char* restrict code,
TokenSubtype* subtype,
long int start,
long int* end,
long int* L_start,
long int* L_end) {
	if(code[start] != '[')
		return false;

	long int buffer_end = start + 1;
	*subtype = TokenSubtype_NO;
	*L_start = start + 1;

	do {
		get_next_word(
			code,
			&start,
			&buffer_end);
		TokenSubtype buffer_subtype;

		if(is_valid_QL(
			code,
			&buffer_subtype,
			start,
			buffer_end)
		== false)
			return false;

		*subtype |= buffer_subtype;
		*L_end = buffer_end;
	} while(code[buffer_end] != ']');

	*end = buffer_end + 1;
	return true;
}

static bool if_QL_create_token(
const char* restrict code,
long int start,
long int* end,
Token* token) {
	TokenSubtype subtype;
	long int buffer_end = *end;
	long int L_start;
	long int L_end;

	if(!get_QL(
		code,
		&subtype,
		start,
		&buffer_end,
		&L_start,
		&L_end)
	|| (isgraph(code[buffer_end])
	 && code[buffer_end] != ':')) {
		// better to check this here?
		if(isgraph(code[buffer_end]))
			token_error = true;

		return false;
	}

	if(code[buffer_end] == ':')
		buffer_end += 1;

	if(isgraph(code[buffer_end]))
		return false;

	*end = buffer_end;
	create_token_Q(
		TokenType_QL,
		subtype,
		L_start,
		L_end,
		token);
	return true;
}

static bool if_L_create_token(
bool previous_is_command,
const char* restrict code,
long int start,
long int* end,
Token* token) {
	if(previous_is_command
	|| is_valid_name(
		code,
		start,
		*end)
	== false)
		return false;

	create_token_colon_word(
		TokenType_L,
		start,
		*end,
		*end,
		*end,
		token);
	
	if(code[*end] == ':')
		*end += 1;

	return true;
}

static bool get_QR(
const char* restrict code,
TokenSubtype* subtype,
long int start,
long int* end,
long int* R_start,
long int* R_end) {
	if(code[start] != ':'
	|| code[start + 1] != '[')
		return false;

	start += 1;
	long int buffer_end = start + 1;
	*subtype = TokenSubtype_NO;
	*R_start = start + 1;

	do {
		get_next_word(
			code,
			&start,
			&buffer_end);
		TokenSubtype buffer_subtype;

		if(is_valid_QR(
			code,
			&buffer_subtype,
			start,
			buffer_end)
		== false)
			return false;

		*subtype |= buffer_subtype;
		*R_end = buffer_end;
	} while(code[buffer_end] != ']'); 

	*end = buffer_end + 1;
	return true;
}

static bool if_QR_create_token(
const char* restrict code,
long int start,
long int* end,
Token* token) {
	TokenSubtype subtype;
	long int buffer_end = *end;
	long int R_start;
	long int R_end;

	if(!get_QR(
		code,
		&subtype,
		start,
		&buffer_end,
		&R_start,
		&R_end)
	|| isgraph(code[buffer_end]))
		return false;

	*end = buffer_end;
	create_token_Q(
		TokenType_QR,
		subtype,
		R_start,
		R_end,
		token);
	return true;
}

static bool if_R_create_token(
const char* restrict code,
long int start,
long int* end,
Token* token) {
	if(code[start] != ':')
		return false;

	long int buffer_end = start + 1;
	get_next_word(
		code,
		&start,
		&buffer_end);

	if(is_special(code[*end]))
		return false;

	if(is_valid_name(
		code,
		start,
		buffer_end)
	== false)
		return false; // could be an array

	*end = buffer_end;
	create_token_colon_word(
		TokenType_R,
		start,
		start,
		start,
		*end,
		token);
	return true;
}

static bool if_QLR_create_token(
const char* restrict code,
long int start,
long int* end,
Token* token) {
	TokenSubtype subtype1;
	TokenSubtype subtype2;
	long int buffer_end = *end;
	long int L_start;
	long int L_end;
	long int R_start;
	long int R_end;
	
	if(get_QL(
		code,
		&subtype1,
		start,
		&buffer_end,
		&L_start,
		&L_end)
	== false)
		return false;

	start = buffer_end;

	if(get_QR(
		code,
		&subtype2,
		start,
		&buffer_end,
		&R_start,
		&R_end)
	== false)
		return false;

	if(isgraph(code[buffer_end]))
		return false;

	*end = buffer_end;
	*token = (Token) {
		.type = TokenType_QLR,
		.subtype = subtype1 | subtype2,
		.L_start = L_start,
		.L_end = L_end,
		.R_start = R_start,
		.R_end = R_end};
	return true;
}

static bool if_LR_create_token(
bool previous_is_command,
const char* restrict code,
long int start,
long int* end,
Token* token) {
	if(previous_is_command
	|| !is_valid_name(
		code,
		start,
		*end)
	|| code[*end] != ':')
		return false;

	long int R_start = *end + 1;
	long int R_end = *end + 1;
	get_next_word(
		code,
		&R_start,
		&R_end);

	if(is_valid_name(
		code,
		R_start,
		R_end)
	== false)
		return false;

	create_token_colon_word(
		TokenType_LR,
		start,
		*end,
		R_start,
		R_end,
		token);
	*end = R_end;
	return true;
}

static bool if_literal_create_token(
const char* restrict code,
long int start,
long int* end,
Token* token) {
	TokenSubtype subtype;
	long int buffer_end = start + 1;

	if(isdigit(code[start])) {
		// base check
		if(code[start] == '0'
		&& !isdigit(code[buffer_end])) {
			switch(code[buffer_end]) {
				case 'b': break;
				case 'o': break;
				case 'x': break;
				default: token_error = true; return false; // unknown base
			}

			buffer_end += 1;

			if(!isXdigit(code[buffer_end])) {
				token_error = true;
				return false;
			}
		}

		while(code[buffer_end] != '\0'
		   && (isXdigit(code[buffer_end])
		    || code[buffer_end] == '`')) buffer_end += 1;
		// a number cannot be followed by '`' and must be followed by a blank or a special symbole
		if(code[buffer_end - 1] == '`'
		|| (isgraph(code[buffer_end])
		 && !is_special(code[buffer_end]))) {
			token_error = true;
			return false;
		}

		subtype = TokenSubtype_LITERAL_NUMBER;
	} else if(code[start] == '`') {
		while(code[buffer_end] != '\0'
		   && code[buffer_end] != '`') buffer_end += 1;

		if(code[buffer_end] != '`') {
			token_error = true;
			return false;
		}

		start += 1;
		buffer_end += 1;
		subtype = TokenSubtype_LITERAL_STRING;
	} else if(code[start] == '\\') {
		start += 1;
		buffer_end += 1;
		subtype = TokenSubtype_LITERAL_ASCII;
	} else
		return false;

	*end = buffer_end;
	*token = (Token) {
		.type = TokenType_LITERAL,
		.subtype = subtype,
		.start = start,
		.end = *end - (subtype == TokenSubtype_LITERAL_STRING ? 1 : 0)};
	return true;
}

static bool if_PL_create_token(
const char* code,
long int start,
long int* end,
Token* token) {
	if(code[start] != '.'
	|| !isgraph(code[start + 1]))
		return false;

	long int buffer_end = start + 1;
	get_next_word(
		code,
		&start,
		&buffer_end);

	if(is_valid_name(
		code,
		start,
		buffer_end)
	== false)
		return false;

	*end = buffer_end;
	create_token_colon_word(
		TokenType_PL,
		start,
		buffer_end,
		buffer_end,
		buffer_end,
		token);
	return true;
}

static bool if_special_create_token(
const char* code,
long int start,
Token* token) {
	if(!is_special(code[start]))
		return false;

	create_token_special(
		code,
		start,
		TokenType_SPECIAL,
		token);
	return true;
}

static bool if_valid_name_create_token(
const char* code,
long int start,
long int end,
Token* token) {
	if(is_valid_name(
		code,
		start,
		end)
	== false)
		return false;

	*token = (Token) {
		.type = TokenType_IDENTIFIER,
		.subtype = TokenSubtype_NO,
		.start = start,
		.end = end};
	return true;
}

bool create_lexer(
const Source* source,
Allocator* restrict allocator,
Lexer* restrict lexer) {
	lexer->source = source;
	lexer->tokens = NULL;
	lexer->count = 0;

	const char* code = source->content;
	bool previous_is_command = false; // checked for keys only
	long int start = 0;
	long int end = 0;
	long int i = 0;

	if(lexer_scan_errors(
		source,
		allocator)
	== false)
		return false;
#define TOKENS_CHUNK 4096
	while(get_next_word(
		code,
		&start,
		&end)
	== true) {
		while(skip_comment(
			code,
			&start,
			&end));
		// allocation
		if(lexer->count <= i) {
			Token* tokens_realloc = realloc(
				lexer->tokens,
				(lexer->count + TOKENS_CHUNK) * sizeof(Token));

			if(tokens_realloc == NULL) {
				destroy_lexer(lexer);
				return false;
			}
			
			lexer->tokens = tokens_realloc;
			lexer->count += TOKENS_CHUNK;
		}
		// create tokens
		Token* token = &lexer->tokens[i];

		if(if_command_create_token(
			code,
			start,
			token)
		== true) {
			previous_is_command = true;
		} else if(if_QL_create_token(
			code,
			start,
			&end,
			token)
		== true) {
			// OK
		} else if(if_L_create_token(
			previous_is_command,
			code,
			start,
			&end,
			token)
		== true)  {
			// OK
		} else if(if_QR_create_token(
			code,
			start,
			&end,
			token)
		== true) {
			// OK
		} else if(if_R_create_token(
			code,
			start,
			&end,
			token)
		== true) {
			// OK
		} else if(if_QLR_create_token(
			code,
			start,
			&end,
			token)
		== true) {
			// OK
		} else if(if_LR_create_token(
			previous_is_command,
			code,
			start,
			&end,
			token)
		== true) {
			// OK
		} else if(if_PL_create_token(
			code,
			start,
			&end,
			token)
		== true) {
			// OK
		} else if(if_literal_create_token(
			code,
			start,
			&end,
			token)
		== true) {
			// OK
		} else if(is_special(code[start])) {
			long int buffer_end = end;
			// right case
			if(code[start] == ':'
			&& (is_operator_leveling(code[start + 1])
			 || code[start + 1] == '[')) {
				while(is_operator_leveling(code[buffer_end])
				   || is_bracket(code[buffer_end])) buffer_end += 1;
				create_token_colon_word(
					TokenType_R,
					start,
					start,
					start + 1, // ignore colon
					buffer_end,
					token);
				end = buffer_end;
			// left case
			} else if(is_operator_leveling(code[start])
			       || code[start] == '[') {
				while(is_operator_leveling(code[buffer_end])
				   || is_bracket(code[buffer_end])) buffer_end += 1;

				if(code[buffer_end] != ':')
					goto SPECIAL;

				create_token_colon_word(
					TokenType_L,
					start,
					buffer_end,
					buffer_end,
					buffer_end,
					token);
				end = buffer_end;
			} else {
SPECIAL:
				create_token_special(
					code,
					start,
					TokenType_SPECIAL,
					token);
			}
		} else if(if_valid_name_create_token(
			code,
			start,
			end,
			token)
		== true) {
			// OK
		} else {
			destroy_lexer(lexer);
			return false;
		}

		previous_is_command = is_command(code[start]);
		start = end;
		i += 1;
	}

	lexer->count = i + 1; // null token
	Token* tokens_realloc = realloc(
		lexer->tokens,
		(lexer->count + 1) * sizeof(Token));
#undef TOKENS_CHUNK // no more allocation reminder
	if(tokens_realloc == NULL) {
		destroy_lexer(lexer);
		return false;
	}

	lexer->tokens = tokens_realloc;
	lexer->tokens[i] = (Token) {
		.type = TokenType_NO,
		.subtype = TokenSubtype_NO};
	return true;
}

void destroy_lexer(
Lexer* restrict lexer) {
	lexer->source = NULL;
	free(lexer->tokens);
	lexer->count = 0;
}
