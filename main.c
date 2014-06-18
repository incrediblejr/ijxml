
#define IJXML_AUX_USE_ASSERT
#define IJXML_AUX_IMPLEMENTATION
#include "ijxml_aux.h"

#define IJXML_IMPLEMENTATION
#include "ijxml.h"

#if defined(WIN32)
	#include <windows.h>	/* OutputDebugStringA */
#endif

#include <stdio.h>      /* printf, sprintf */
#include <stdarg.h>     /* va_list, va_start, va_arg, va_end */
#include <stdlib.h>		/* system, free, realloc, ... */
#include <string.h>		/* memcpy */

#include <assert.h>

#if defined(WIN32)
	#define LOG_OUT(s) printf("%s", (s)), OutputDebugStringA((s))
#else
	#define LOG_OUT(s) printf("%s", (s))
#endif

#define VSNPRINTF(s, n, f, v) _vsnprintf_s((s), (n), _TRUNCATE, (f), (v))

#define XML_ENSURE(cond) assert((cond))

#define LOG_TEMP_BUFFER_SIZE (32*1024)
static void test_log(const char *msg_format, ...)
{
	char buffer[LOG_TEMP_BUFFER_SIZE];
	va_list args;

	va_start(args, msg_format);

	VSNPRINTF(buffer, LOG_TEMP_BUFFER_SIZE-1, msg_format, args);

	LOG_OUT(buffer);

	va_end(args);
}
#define TEST_LOG(s, ...) test_log(s "\n", ##__VA_ARGS__)

static char *get_token_string(const char *xml, struct ijxml_token *t)
{
	size_t len = (size_t)(t->end - t->start);
	char *res = malloc(len+1);
	memcpy(res, &xml[t->start], len);
	res[len] = 0;

	return res;
}

static const char *IJXML_TYPE_TO_STRING(int t)
{
	switch (t) {
		case IJXML_TAG_NAME : return "IJXML_TAG_NAME";
		case IJXML_OBJECT : return "IJXML_OBJECT";
		case IJXML_STRING : return "IJXML_STRING";
		case IJXML_ATTRIBUTE_KEY : return "IJXML_ATTRIBUTE_KEY";
		case IJXML_ATTRIBUTE_VALUE : return "IJXML_ATTRIBUTE_VALUE";
		case IJXML_COMMENT : return "IJXML_COMMENT";
		case IJXML_VALUE : return "IJXML_VALUE";

		default	: return "FAIL";
	}
}

static void print_token(const char *xml, struct ijxml_token *t, const char *debug_text)
{
	char *s = get_token_string(xml, t);
	TEST_LOG("[%s] -> [%s] : '%s'. parent [%u]", debug_text, IJXML_TYPE_TO_STRING(t->type), s, t->parent);
	free(s);
}

static void print_tokens(const char *xml, const char *debug_tag, struct ijxml_token *tokens, unsigned num_tokens)
{
	unsigned i;
	char N[64];
	for (i=0; i!=num_tokens; ++i) {
		struct ijxml_token *token = tokens+i;
		sprintf(N, "%u - %s", i, debug_tag);
		print_token(xml, token, N);
	}
}

static const char xml[] = 
	"<object empty_attribute=\"\" class=\"Event\" id  = \"{507f80fe-8832-429b-9951-2b2ee54695c6}\">"
		"<property name=\"name_value\">"
			"<value>empty_event</value>"
		"</property>"
		"<property name=\"name_value2\">"
			"<value>empty_event2</value>"
		"</property>"
	"</object>";


static void test_reallocation(void)
{
	struct ijxml_parser xml_parser_static, xml_parser_realloc;

	struct ijxml_token static_tokens[24];
	struct ijxml_token *realloc_tokens = 0;
	unsigned num_tokens = 0;

	struct ijxml_parse_result res;

	ijxml_parser_init(&xml_parser_static);
	res = ijxml_parse(&xml_parser_static, xml, (unsigned)strlen(xml), static_tokens, sizeof(static_tokens)/ sizeof(static_tokens[0]));
	XML_ENSURE(res.error == 0);

	ijxml_parser_init(&xml_parser_realloc);
	do {
		num_tokens = (num_tokens == 0 ? 1 : num_tokens + 1);
		realloc_tokens = (struct ijxml_token*)realloc(realloc_tokens, num_tokens * sizeof(struct ijxml_token));
		if (num_tokens == 16)
			num_tokens = 16;
		res = ijxml_parse(&xml_parser_realloc, xml, (unsigned)strlen(xml), realloc_tokens, num_tokens);
	} while(res.error == 1);

	XML_ENSURE(xml_parser_realloc.toknext == xml_parser_static.toknext);
	XML_ENSURE(memcmp(static_tokens, realloc_tokens, sizeof(struct ijxml_token)*(xml_parser_realloc.toknext-1)) == 0);

	free(realloc_tokens);
}

static void test_reallocation_parsing(void)
{
	struct ijxml_parser xml_parser;
	struct ijxml_parser *parser = &xml_parser;

	struct ijxml_token *realloc_tokens = 0;
	unsigned num_tokens = 0;

	struct ijxml_parse_result res;

	ijxml_parser_init(parser);

	do {
		num_tokens = (num_tokens == 0 ? 1 : num_tokens + 1);
		realloc_tokens = (struct ijxml_token*)realloc(realloc_tokens, num_tokens * sizeof(struct ijxml_token));
		if (num_tokens == 16)
			num_tokens = 16;
		res = ijxml_parse(parser, xml, (unsigned)strlen(xml), realloc_tokens, num_tokens);
	} while(res.error == 1);

	print_tokens(xml, "REALLOC_TEST", realloc_tokens, parser->toknext);
	{
		unsigned i, n;
		struct ijxml_aux_context con;
		struct ijxml_aux_context *ctx = &con;
		struct ijxml_token *token;

		unsigned token_index;

		ijxml_aux_init(ctx, xml, realloc_tokens, parser->toknext);

		for (i=0, n=realloc_tokens->size; i!=n; ++i) {
			token_index = ijxml_aux_object_at(ctx, 0, i);
			XML_ENSURE(token_index != IJXML_AUX_INVALID_TOKEN_OFFSET);
			print_token(xml, ijxml_aux_token(ctx, ijxml_aux_tag(ctx, token_index)), "");
		}

		token_index = ijxml_aux_object_attribute(ctx, 0, "id");
		XML_ENSURE(token_index != IJXML_AUX_INVALID_TOKEN_OFFSET);
		XML_ENSURE(ijxml_aux_token_equals(ctx, token_index, "{507f80fe-8832-429b-9951-2b2ee54695c6}"));

		token_index = ijxml_aux_object_attribute(ctx, 0, "class");
		XML_ENSURE(token_index != IJXML_AUX_INVALID_TOKEN_OFFSET);
		XML_ENSURE(ijxml_aux_token_equals(ctx, token_index, "Event"));
		token = realloc_tokens+token_index;
		print_token(xml, token, "class attribute");

		token_index = ijxml_aux_object_attribute(ctx, 0, "clas");
		XML_ENSURE(token_index == IJXML_AUX_INVALID_TOKEN_OFFSET);

		token_index = ijxml_aux_object_by_tag(ctx, 0, "property");
		XML_ENSURE(token_index != IJXML_AUX_INVALID_TOKEN_OFFSET);

		token_index = ijxml_aux_object_attribute(ctx, token_index, "name");
		XML_ENSURE(token_index != IJXML_AUX_INVALID_TOKEN_OFFSET);
		XML_ENSURE(ijxml_aux_token_equals(ctx, token_index, "name_value"));
		token = realloc_tokens+token_index;
		print_token(xml, token, "property attribute");
		i = 0;
	}
}

int main(int args, char **argv)
{
	(void)args;
	(void)argv;

	test_reallocation_parsing();

	test_reallocation();
	//system("pause");

	return 0;
}