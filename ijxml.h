#ifndef _IJXML_H_
#define _IJXML_H_

typedef enum {
	IJXML_OBJECT,
	IJXML_STRING,
	IJXML_TAG_NAME,
	IJXML_ATTRIBUTE_KEY,
	IJXML_ATTRIBUTE_VALUE,
	IJXML_COMMENT,
	IJXML_VALUE,

	IJXML_TYPE_FORCEINT = 65536    /* Makes sure this enum is signed 32bit. */
} ijxmltype_t;

typedef struct ijxml_token {
	ijxmltype_t type;
	unsigned start;
	unsigned end;
	unsigned size;
	unsigned parent;
} ijxml_token;

typedef struct ijxml_parse_result {
	int error;
} ijxml_parse_result;

typedef struct ijxml_parser {
	unsigned pos;
	unsigned toknext;
	unsigned toksuper;
} ijxml_parser;

void ijxml_parser_init(struct ijxml_parser *parser);
struct ijxml_parse_result ijxml_parse(struct ijxml_parser *parser, const char *xml, unsigned xml_len, struct ijxml_token *tokens, unsigned num_tokens);

#endif // _IJXML_H_

#if defined(IJXML_IMPLEMENTATION)

#define IJXML__NO_TOKEN_SUPER			((unsigned)-1)
#define IJXML__TOKEN_INVALID_OFFSET		((unsigned)-1)

void ijxml_parser_init(struct ijxml_parser *parser)
{
	parser->pos = parser->toknext = 0u;
	parser->toksuper = IJXML__NO_TOKEN_SUPER;
}

static struct ijxml_token *ijxml__allocate_token(struct ijxml_parser *parser, struct ijxml_token *tokens, unsigned num_tokens)
{
	struct ijxml_token *tok;

	if (parser->toknext >= num_tokens)
		return 0;

	tok = &tokens[parser->toknext++];
	tok->start = tok->end = IJXML__TOKEN_INVALID_OFFSET;
	tok->size = 0u;
	tok->parent = IJXML__NO_TOKEN_SUPER;

	return tok;
}

static void ijxml__token_fill(struct ijxml_token *tok, unsigned start_pos, unsigned end_pos, ijxmltype_t xml_type)
{
	tok->start = start_pos;
	tok->end = end_pos;
	tok->type = xml_type;
	tok->size = 0u;
}

static unsigned ijxml__string_len(const char *s)
{
	const char *eos = s;
	while(*eos)
		++eos;

	return (unsigned)(eos - s);
}

static int ijxml__string_starts_with(const char *s, const char *substring, const char *end)
{
	while (s < end) {
		if (*substring != *s)
			return 0;

		++s, ++substring;

		if (*substring == 0)
			return 1;
	}

	return 0;
}

static void ijxml__find_substring(struct ijxml_parser *parser, const char *xml, const char *substring, unsigned xml_len, struct ijxml_parse_result *res)
{
	unsigned start_parser_pos = parser->pos;
	unsigned substring_len = ijxml__string_len(substring);
	const char *xml_end = xml+xml_len;

	if (parser->pos + substring_len > xml_len) {
		res->error = 1;
		return;
	}

	while ((parser->pos+substring_len) <= xml_len) {
		const char *s = &xml[parser->pos++];

		if (ijxml__string_starts_with(s, substring, xml_end) == 1)
			return;

		if (*s == 0)
			break;
	}

	parser->pos = start_parser_pos;
	res->error = 1;
}

static void ijxml__skip_to_white(struct ijxml_parser *parser, const char *xml, unsigned xml_len, struct ijxml_parse_result *res)
{
	unsigned parser_start_pos = parser->pos;

	while (parser->pos != xml_len) {
		char c = xml[parser->pos++]; 
		switch (c)
		{
			case '\t' : case '\r' : case '\n' : case ' ': 
				return;
			default:
				break;
		}
	}

	if (parser->pos == xml_len) {
		parser->pos = parser_start_pos;
		res->error = 1;
	}
}

static void ijxml__skip_whitespaces(struct ijxml_parser *parser, const char *xml, unsigned xml_len, struct ijxml_parse_result *res)
{
	unsigned parser_start_pos = parser->pos;

	while (parser->pos != xml_len) {
		char c = xml[parser->pos]; 
		switch (c)
		{
		case '\t' : case '\r' : case '\n' : case ' ': 
			++parser->pos;
			break;
		default:
			return;
		}
	}

	if (parser->pos == xml_len) {
		parser->pos = parser_start_pos;
		res->error = 1;
	}
}

static void ijxml__parse_string(struct ijxml_parser *parser, const char *xml, unsigned xml_len, struct ijxml_token *tokens, unsigned num_tokens, ijxmltype_t xml_type, struct ijxml_parse_result *res)
{
	struct ijxml_token *token;
	unsigned start = parser->pos++; // skip "

	for (; parser->pos < xml_len && xml[parser->pos] != '\0'; parser->pos++) {
		char c = xml[parser->pos];

		if (c == '\"') {
			token = ijxml__allocate_token(parser, tokens, num_tokens);
			if (!token) {
				goto string_parse_fail;
			}

			ijxml__token_fill(token, start+1, parser->pos++, xml_type);

			token->parent = parser->toksuper;

			return;
		}

		if (c == '\\') {
			parser->pos++;
			switch (xml[parser->pos])
			{
				case '\"': case '/' : case '\\' : case 'b' :
				case 'f' : case 'r' : case 'n'  : case 't' :
					break;
				default:
					goto string_parse_fail;
			}
		}
	}

string_parse_fail:
	parser->pos = start;
	res->error = 1;
}

static void ijxml__parse_key(struct ijxml_parser *parser, const char *xml, unsigned xml_len, struct ijxml_token *tokens, unsigned num_tokens, ijxmltype_t xml_type, struct ijxml_parse_result *res)
{
	struct ijxml_token *token;
	unsigned start = parser->pos;

	for (; parser->pos < xml_len && xml[parser->pos] != '\0'; parser->pos++) {
		switch (xml[parser->pos]) {
			case '\t' : case '\r' : case '\n' : case ' ' :
			case '\\' : case '>' : case '<' : case '=' :
				goto found;
		}

		if (xml[parser->pos] < 32 || xml[parser->pos] >= 127) {
			parser->pos = start;
			res->error = 1;
			return;
		}
	}

found:
	token = ijxml__allocate_token(parser, tokens, num_tokens);
	if (!token) {
		parser->pos = start;
		res->error = 1;
		return;
	}

	ijxml__token_fill(token, start, parser->pos, xml_type);

	token->parent = parser->toksuper;
}

static void ijxml__parse_attributes(struct ijxml_parser *parser, const char *xml, unsigned xml_len, struct ijxml_token *tokens, unsigned num_tokens, struct ijxml_parse_result *res)
{
	while ((xml[parser->pos] != '\0') && (parser->pos < xml_len) && (res->error == 0)) {
		char c;

		ijxml__skip_whitespaces(parser, xml, xml_len, res);

		if (res->error != 0)
			break;

		c = xml[parser->pos];
		switch (c)
		{
			case '/' : case '>' :
				return;
			
			default : {
				ijxml__parse_key(parser, xml, xml_len, tokens, num_tokens, IJXML_ATTRIBUTE_KEY, res);
				
				if (res->error != 0)
					break;

				ijxml__find_substring(parser, xml, "=", xml_len, res);
				if (res->error != 0)
					break;
				
				ijxml__skip_whitespaces(parser, xml, xml_len, res);
				if (res->error != 0)
					break;

				ijxml__parse_string(parser, xml, xml_len, tokens, num_tokens, IJXML_ATTRIBUTE_VALUE, res);
			}
		}
	}
}

static struct ijxml_parse_result ijxml__parse(struct ijxml_parser *parser, const char *xml, unsigned xml_len, struct ijxml_token *tokens, unsigned num_tokens)
{
	struct ijxml_parse_result result;

	result.error = 0;

	while ((xml[parser->pos] != '\0') && (parser->pos < xml_len) && (result.error == 0)) {
		char c = xml[parser->pos];

		switch (c) 
		{
			case '<' : {
				char c2;
				if (parser->pos+1 >= xml_len) {
					result.error = 1;
					break;
				}
				c2 = xml[++parser->pos];

				switch (c2) {
					case '?' : {
						ijxml__find_substring(parser, xml, "?>", xml_len, &result);
					} break;

					case '!' : {
						char c3;
						if (parser->pos+1 >= xml_len) {
							result.error = 1;
							break;
						}
						c3 = xml[++parser->pos];
						if (c3 == '-')
							ijxml__find_substring(parser, xml, "-->", xml_len, &result);
						else
							ijxml__find_substring(parser, xml, ">", xml_len, &result);
					} break;

					case '/' :
						break;

					default: {
						struct ijxml_token *object_token;
						// allocate token for 'object'
						object_token = ijxml__allocate_token(parser, tokens, num_tokens);

						if (!object_token) {
							--parser->pos;		// back up to '<'
							result.error = 1;
							break;
						}

						if (parser->toksuper != IJXML__NO_TOKEN_SUPER) {
							++tokens[parser->toksuper].size;
							object_token->parent = parser->toksuper;
						}

						object_token->type = IJXML_OBJECT;
						object_token->start = parser->pos - 1;
						parser->toksuper = parser->toknext - 1;

						ijxml__parse_key(parser, xml, xml_len, tokens, num_tokens, IJXML_TAG_NAME, &result);
						if (result.error != 0)
							break;

						ijxml__parse_attributes(parser, xml, xml_len, tokens, num_tokens, &result);
					} // default
				} // switch c2
					
			} break; // case '<'

			case '/' : {
				char c2;
				if (parser->toknext < 1) {
					result.error = 1;
					break;
				}

				if (parser->pos+1 >= xml_len) {
					result.error = 1;
					break;
				}
					
				c2 = xml[++parser->pos];

				if (c2 != '>') {
					ijxml__find_substring(parser, xml, ">", xml_len, &result);

					if (result.error != 0)
						break;
				}

				{
					struct ijxml_token *token = &tokens[parser->toknext - 1];
					for (;;) {
						if (token->start != IJXML__TOKEN_INVALID_OFFSET && token->end == IJXML__TOKEN_INVALID_OFFSET) {
							if (token->type != IJXML_OBJECT) {
								result.error = 1;
								break;
							}
							token->end = parser->pos;
							parser->toksuper = token->parent;
							break;
						}

						if (token->parent == IJXML__NO_TOKEN_SUPER)
							break;

						token = &tokens[token->parent];
					}
				}
			} break; /* case '/' */
				
			case '"' : {
				ijxml__parse_string(parser, xml, xml_len, tokens, num_tokens, IJXML_STRING, &result);
			} break;

			case '\t' : case '\r' : case '\n' : case ' ' : case '=' : case '>' : {
				++parser->pos;
			} break;

			default: {
				ijxml__parse_key(parser, xml, xml_len, tokens, num_tokens, IJXML_VALUE, &result);
			}
		}
	}

	return result;
}

struct ijxml_parse_result ijxml_parse(struct ijxml_parser *parser, const char *xml, unsigned xml_len, struct ijxml_token *tokens, unsigned num_tokens)
{
	struct ijxml_token *current_token;
	unsigned num_parsed_tokens = parser->toknext;

	while (num_parsed_tokens) {
		current_token = &tokens[--num_parsed_tokens];
		if (current_token->type == IJXML_OBJECT) {
			if (current_token->end == IJXML__TOKEN_INVALID_OFFSET) {
				parser->pos = current_token->start;
				parser->toknext = num_parsed_tokens;
				parser->toksuper = current_token->parent;

				if (current_token->parent != IJXML__NO_TOKEN_SUPER)
					--tokens[current_token->parent].size;
			} else {
				parser->toksuper = parser->toksuper;
			}

			break;
		}
	}

	return ijxml__parse(parser, xml, xml_len, tokens, num_tokens);
}

#endif