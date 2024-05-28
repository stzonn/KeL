#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "lexer.h"
#include "lexer_allocation.h"
#include "lexer_error.h"
#include "lexer_utils.h"

/*
 * L stands for "Left"
 * R stands for "Right"
 * Q stands for "Qualifier"
 * PL stands for "Period Left"
 * It is related to the position around a colon. Everything is an L by default.
 *
 * Checking order:
 * 1 - is command
 * 2 - is QL
 * 3 - is L
 * 4 - is QR
 * 5 - is R
 *     1 - Consumes all following R modifier operators.
 * 6 - is QLR
 * 7 - is LR
 * 8 - is PL
 * 9 - is literal 
 * 10 - is special
 *      1 - Consumes all following R modifier operators
 *      2 - R left parenthesis
 *      3 - R grave accent
 *      4 - Consumes all following L modifier operators
 *      5 - R right parenthesis
 *      6 - Lonely colon
 *      7 - Other special symbols
 * 11 - is valid name
 *
 * Qualifier cases are checked first to detect the brackets, so it is easier to detect
 * names (for instance, the name of an L or a R).
*/

// more errors will be supported in "lexer_error.c".
static int error = 0;

static int set_error(int value) {
	if(value == -1) {
		error = -1;
		return -1;
	}

	return value;
}

static void create_token_null(Token* token) {
	*token = (Token) {
		.type = TokenType_NO,
		.subtype = TokenSubtype_NO,
		.start = 0,
		.end = 0};
}

static void create_token_special(
const char* restrict code,
long int start,
TokenType type,
Token* restrict token) {
	*token = (Token) {
		.type = type,
		.subtype = lexer_character_to_subtype(code[start]),
		.start = start,
		.end = start + 1};
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

static bool if_command_create_token(
const char* code,
long int start,
Token* token) {
	if(!lexer_is_command(code[start]))
		return false;

	create_token_special(
		code,
		start,
		TokenType_COMMAND,
		token);
	return true;
}

static int get_QL(
TokenType type,
TokenSubtype subtype,
long int* restrict start,
long int* restrict end,
long int* restrict i,
Lexer* lexer) {
	const char* code = lexer->source->content;
	Token* tokens = lexer->tokens;

	do {
		if(lexer_allocate_chunk(
			*i + 1,
			lexer)
		== false)
			return -1;

		lexer_get_next_word(
			code,
			start,
			end);
		tokens[*i] = (Token) {
			.type = type,
			.subtype = subtype,
			.L_start = *start,
			.L_end = *end,
			.R_start = *end,
			.R_end = *end};
		*i += 1;
	} while(code[*end] != ']');

	return 1;
}

static int if_QL_create_token(
long int* restrict start,
long int* restrict end,
long int* restrict i,
Lexer* lexer) {
	const char* code = lexer->source->content;
	Token* tokens = lexer->tokens;
	long int buffer_start = *start;
	long int buffer_end = *end;
	long int buffer_i = *i;

	if(isgraph(code[buffer_start - 1])
	|| code[buffer_start] != '[')
		return 0;

	switch(get_QL(
		TokenType_QL,
		TokenType_QL,
		&buffer_start,
		&buffer_end,
		&buffer_i,
		lexer)) {
	case -1: return -1;
	case 1: /* fall through */
	}

	buffer_end += 1;

	if(code[buffer_end] == ':'
	// QR possibility
	&& !isgraph(code[buffer_end + 1]))
		buffer_end += 1;

	if(isgraph(code[buffer_end])) // performance?
		return 0;

	*start = buffer_start;
	*end = buffer_end;
	*i = buffer_i - 1;
	return 1;
}

static bool if_L_create_token(
long int start,
long int* end,
long int i,
Lexer* lexer) {
	const char* code = lexer->source->content;
	Token* tokens = lexer->tokens;
	const bool previous_is_command = tokens[i - 1].type == TokenType_COMMAND;
	const bool previous_is_operator_modifier = lexer_is_operator_modifier(code[tokens[i - 1].L_start]);

	if(previous_is_command
	|| previous_is_operator_modifier
	|| code[start - 1] == ':'
	|| (code[*end] == ':'
	 && isalpha(code[*end + 1]))
	|| lexer_is_valid_name(
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
		&tokens[i]);
	
	if(code[*end] == ':'
	// R possibility
	&& !isgraph(code[*end + 1]))
		*end += 1;

	return true;
}

static int get_QR(
TokenType type,
TokenSubtype subtype,
long int* restrict start,
long int* restrict end,
long int* restrict i,
Lexer* lexer) {
	const char* code = lexer->source->content;
	Token* tokens = lexer->tokens;

	do {
		if(lexer_allocate_chunk(
			*i + 1,
			lexer)
		== false)
			return -1;

		lexer_get_next_word(
			code,
			start,
			end);
		tokens[*i] = (Token) {
			.type = type,
			.subtype = subtype,
			.L_start = *start,
			.L_end = *start,
			.R_start = *start,
			.R_end = *end};
		*i += 1;
	} while(code[*end] != ']');

	return 1;
}

static int if_QR_create_token(
long int* restrict start,
long int* restrict end,
long int* restrict i,
Lexer* restrict lexer) {
	const char* code = lexer->source->content;
	Token* tokens = lexer->tokens;
	long int buffer_start = *start;
	long int buffer_end = *end;
	long int buffer_i = *i;

	if(code[buffer_start] != ':'
	|| code[buffer_start + 1] != '[')
		return 0;

	buffer_end += 1;

	switch(get_QR(
		TokenType_QR,
		TokenType_QR,
		&buffer_start,
		&buffer_end,
		&buffer_i,
		lexer)) {
	case -1: return -1;
	case 1: /* fall through */;
	}

	buffer_end += 1;

	if(isgraph(code[buffer_end])) // performance?
		return 0;

	*start = buffer_start;
	*end = buffer_end;
	*i = buffer_i - 1;
	return 1;
}

static bool if_R_create_token(
long int start,
long int* end,
long int i,
Lexer* lexer) {
	const char* code = lexer->source->content;
	long int buffer_end = start + 1;
	Token* tokens = lexer->tokens;
	const bool previous_is_operator_modifier = lexer_is_operator_modifier(code[tokens[i - 1].L_start]);

	if((code[start] != ':'
	 && !previous_is_operator_modifier)
	|| !isalpha(code[start + 1]))
		return false;

	if(!previous_is_operator_modifier) {
		lexer_get_next_word(
			code,
			&start,
			&buffer_end);
	}

	if(lexer_is_valid_name(
		code,
		start,
		buffer_end)
	== false)
		return false; // could be an array

	if(!previous_is_operator_modifier)
		*end = buffer_end;

	create_token_colon_word(
		TokenType_R,
		start,
		start,
		start,
		*end,
		&tokens[i]);
	return true;
}

static bool if_QLR_create_token(
long int* restrict start,
long int* restrict end,
long int* restrict i,
Lexer* lexer) {
	const char* code = lexer->source->content;
	long int buffer_start = *start;
	long int buffer_end = *end;
	long int buffer_i = *i;

	if(isgraph(code[buffer_start - 1])
	|| code[buffer_start] != '[')
		return 0;

	switch(get_QL(
		TokenType_QLR,
		TokenType_QL,
		&buffer_start,
		&buffer_end,
		&buffer_i,
		lexer)) {
	case -1: return -1;
	case 1: /* fall through */;
	}

	buffer_end += 1;

	if(code[buffer_end] != ':'
	&& code[buffer_end + 1] != '[')
		return false;

	buffer_end += 2;

	switch(get_QR(
		TokenType_QLR,
		TokenType_QR,
		&buffer_start,
		&buffer_end,
		&buffer_i,
		lexer)) {
	case -1: return -1;
	case 1: /* fall through */
	}

	buffer_end += 1;

	if(isgraph(code[buffer_end])) // performance?
		return 0;

	*start = buffer_start;
	*end = buffer_end;
	*i = buffer_i - 1;
	return 1;
}

static bool if_LR_create_token(
long int start,
long int* end,
long int i,
Lexer* lexer) {
	const char* code = lexer->source->content;
	Token* tokens = lexer->tokens;

	if(tokens[i - 1].type == TokenType_COMMAND
	|| !lexer_is_valid_name(
		code,
		start,
		*end)
	|| code[*end] != ':')
		return false;

	long int R_start = *end + 1;
	long int R_end = *end + 1;
	lexer_get_next_word(
		code,
		&R_start,
		&R_end);

	if(lexer_is_valid_name(
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
		&tokens[i]);
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
				case 'B': break;
				case 'o': break;
				case 'x': break;
				default: set_error(-1); return false; // unknown base
			}

			buffer_end += 1;

			if(!isXdigit(code[buffer_end])) {
				set_error(-1);
				return false;
			}
		}

		while(code[buffer_end] != '\0'
		   && (isXdigit(code[buffer_end])
		    || code[buffer_end] == '`')) buffer_end += 1;
		// a number cannot be followed by '`' and must be followed by a blank or a special symbole
		if(code[buffer_end - 1] == '`'
		|| (isgraph(code[buffer_end])
		 && !lexer_is_special(code[buffer_end]))) {
			set_error(-1);
			return false;
		}

		subtype = TokenSubtype_LITERAL_NUMBER;
	} else if(code[start] == '\'') {
		while(code[buffer_end] != '\0'
		   && code[buffer_end] != '\'') buffer_end += 1;

		if(code[buffer_end] != '\'') {
			set_error(-1);
			return false;
		}

		start += 1;
		buffer_end += 1;
		subtype = TokenSubtype_LITERAL_CHARACTER;
	} else if(code[start] == '`') {
		while(code[buffer_end] != '\0'
		   && code[buffer_end] != '`') buffer_end += 1;

		if(code[buffer_end] != '`') {
			set_error(-1);
			return false;
		}

		start += 1;
		buffer_end += 1;
		subtype = TokenSubtype_LITERAL_STRING;
	} else {
		return false;
	}

	*token = (Token) {
		.type = TokenType_LITERAL,
		.subtype = subtype,
		.start = start,
		.end = buffer_end - (subtype != TokenSubtype_LITERAL_NUMBER ? 1 : 0)};
	*end = buffer_end;
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
	lexer_get_next_word(
		code,
		&start,
		&buffer_end);

	if(lexer_is_valid_name(
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
	if(!lexer_is_special(code[start]))
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
	if(lexer_is_valid_name(
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
	long int count_L_parenthesis_nest = 0; // to get a good match with R parenthesis
	long int start = 0;
	long int end = 1;
	long int i = 1;

	if(lexer_scan_errors(
		source,
		allocator)
	== false)
		return false;

	if(lexer_allocate_chunk(
		1,
		lexer)
	== false)
		return false;

	create_token_null(&lexer->tokens[0]);

	while(lexer_get_next_word(
		code,
		&start,
		&end)
	== true) {
		while(lexer_skip_comment(
			code,
			&start,
			&end));

		if(code[end] == '\0')
			break;
		// allocation
		if(lexer_allocate_chunk(
			i,
			lexer)
		== false) {
			destroy_lexer(lexer);
			return false;
		}
		// create tokens
		Token* token = &lexer->tokens[i];

		if(if_command_create_token(
			code,
			start,
			token)
		== true) {
			// OK
		} else if(set_error(
			if_QL_create_token(
				&start,
				&end,
				&i,
				lexer))
		== 1) {
			// OK
		} else if(if_L_create_token(
			start,
			&end,
			i,
			lexer)
		== true)  {
			// OK
		} else if(set_error(
			if_QR_create_token(
				&start,
				&end,
				&i,
				lexer))
		== 1) {
			// OK
		} else if(if_R_create_token(
			start,
			&end,
			i,
			lexer)
		== true) {
			long int buffer_end = end;
			lexer_get_next_word(
				code,
				&start,
				&buffer_end);

			if(lexer_is_operator_modifier(code[start])) {
				Token* tokens = lexer->tokens;

				do {					
					i += 1;
					tokens[i] = (Token) {
						.type = TokenType_R,
						.subtype = lexer_character_to_subtype(code[start]),
						.L_start = start,
						.L_end = start,
						.R_start = start,
						.R_end = buffer_end};
					
					if(lexer_allocate_chunk(
						i,
						lexer)
					== false) {
						destroy_lexer(lexer);
						return false;
					}

					lexer_get_next_word(
						code,
						&start,
						&buffer_end);
				} while(lexer_is_operator_modifier(code[start])); 

				end = start;
			}
		} else if(set_error(
			if_QLR_create_token(
				&start,
				&end,
				&i,
				lexer))
		== 1) {
			// OK
		} else if(if_LR_create_token(
			start,
			&end,
			i,
			lexer)
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
		} else if(lexer_is_special(code[start])) {
			Token* tokens = lexer->tokens;
			long int buffer_end = end;
			// right case
			if(code[start] == ':'
			&& (lexer_is_operator_leveling(code[buffer_end])
			 || code[buffer_end] == '[')) {
				// start at the first leveling operator, or open bracket
				start += 1;
				buffer_end += 1;

				do {
					tokens[i] = (Token) {
						.type = TokenType_R,
						.subtype = lexer_character_to_subtype(code[start]),
						.L_start = start,
						.L_end = start,
						.R_start = start,
						.R_end = buffer_end};
					i += 1;
					// it must not be EOF (KEY_MODIFIER_EOF)
					lexer_get_next_word(
						code,
						&start,
						&buffer_end);

					if(lexer_allocate_chunk(
						i,
						lexer)
					== false) {
						destroy_lexer(lexer);
						return false;
					}
				} while(lexer_is_operator_modifier(code[start]));

				end = start;
				i -= 1; // `i` is incremented at the end of the loop
			} else if(code[start] == ':'
			       && code[buffer_end] == '(') {
				// it is the only special symbol in this case
				tokens[i] = (Token) {
					.type = TokenType_R,
					.subtype = TokenSubtype_LPARENTHESIS,
					.L_start = start,
					.L_end = start,
					.R_start = start + 1,
					.R_end = start + 2};
				end += 1;
			} else if(code[start] == ':'
			       && code[buffer_end] == '`') {
				tokens[i] = (Token) {
					.type = TokenType_R,
					.subtype = TokenSubtype_GRAVE_ACCENT,
					.L_start = start,
					.L_end = start,
					.R_start = start + 1,
					.R_end = start + 2};
				end += 1;
			// left case
			} else if(lexer_is_operator_leveling(code[start])
			       || code[start] == '[') {
				long int buffer_start = start;

				while(lexer_is_operator_modifier(code[buffer_end])) {
					lexer_get_next_word(
						code,
						&buffer_start,
						&buffer_end);
				}

				if(code[buffer_end] == ':'
				&& lexer_is_operator_modifier(code[start])) {
					do {
						tokens[i] = (Token) {
							.type = TokenType_L,
							.subtype = lexer_character_to_subtype(code[start]),
							.L_start = start,
							.L_end = end,
							.R_start = end,
							.R_end = end};
						i += 1;
						lexer_get_next_word(
							code,
							&start,
							&end);

						if(lexer_allocate_chunk(
							i,
							lexer)
						== false) {
							destroy_lexer(lexer);
							return false;
						}
					} while(code[start] != ':');

					end -= 1;
					i -= 1; // `i` is incremented at the end of the loop
				} else
					goto TOKEN_SPECIAL;
			} else if(code[start] == ')'
			       && count_L_parenthesis_nest == 0) {
				tokens[i] = (Token) {
					.type = TokenType_R,
					.subtype = TokenSubtype_RPARENTHESIS,
					.L_start = start,
					.L_end = start,
					.R_start = start,
					.R_end = start + 1};
			} else {
TOKEN_SPECIAL:
				// to process R parenthesis
				if(code[start] == '(')
					count_L_parenthesis_nest += 1;
				else if(code[start] == ')')
					count_L_parenthesis_nest -= 1;
				
				if(code[start] == ':') {
					create_token_special(
						code,
						start,
						TokenType_COLON_LONELY,
						token);
				} else {
					create_token_special(
						code,
						start,
						TokenType_SPECIAL,
						token);
				}
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

		if(error == -1) {
			destroy_lexer(lexer);
			return false;
		}

		i += 1;
	}

	if(i == 1) {
		destroy_lexer(lexer);
		return false;
	}

	lexer->count = i;
	Token* tokens_realloc = realloc(
		lexer->tokens,
		(i + 1) * sizeof(Token));

	if(tokens_realloc == NULL) {
		destroy_lexer(lexer);
		return false;
	}

	lexer->tokens = tokens_realloc;
	create_token_null(&lexer->tokens[i]);
	return true;
}

void destroy_lexer(
Lexer* restrict lexer) {
	lexer->source = NULL;
	free(lexer->tokens);
	lexer->count = 0;
}
