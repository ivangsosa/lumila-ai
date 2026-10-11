/*
 * Unit tests for utils.c (file-edit parser, error classification).
 * No Geany dependency needed.
 */
#include "../src/utils.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_parse_file_edit_simple(void)
{
    const char *resp =
        "Here is the fix:\n"
        "```file:foo.c\n"
        "int main(void) { return 0; }\n"
        "```\n"
        "Done.";
    gchar *cleaned = NULL;
    GList *edits = lumila_utils_parse_file_edits(resp, &cleaned);
    assert(edits != NULL && edits->next == NULL);
    LumilaFileEdit *e = edits->data;
    assert(strcmp(e->file_name, "foo.c") == 0);
    assert(strcmp(e->content, "int main(void) { return 0; }") == 0);
    assert(cleaned != NULL);
    assert(strstr(cleaned, "Here is the fix:") != NULL);
    assert(strstr(cleaned, "Done.") != NULL);
    assert(strstr(cleaned, "file:") == NULL);
    lumila_utils_file_edits_free(edits);
    g_free(cleaned);
    printf("test_parse_file_edit_simple: PASS\n");
}

static void test_parse_file_edit_nested_fences(void)
{
    /* A markdown file whose content contains its own code fences must
     * not be truncated at the first "```" line. */
    const char *resp =
        "```file:README.md\n"
        "# Title\n"
        "\n"
        "Example:\n"
        "```python\n"
        "print(1)\n"
        "```\n"
        "\n"
        "End of file.\n"
        "```\n";
    GList *edits = lumila_utils_parse_file_edits(resp, NULL);
    assert(edits != NULL && edits->next == NULL);
    LumilaFileEdit *e = edits->data;
    assert(strcmp(e->file_name, "README.md") == 0);
    assert(strstr(e->content, "# Title") != NULL);
    assert(strstr(e->content, "```python") != NULL);
    assert(strstr(e->content, "End of file.") != NULL);
    lumila_utils_file_edits_free(edits);
    printf("test_parse_file_edit_nested_fences: PASS\n");
}

static void test_parse_file_edit_path(void)
{
    const char *resp =
        "```file:src/providers/openai.c\n"
        "/* content */\n"
        "```\n";
    GList *edits = lumila_utils_parse_file_edits(resp, NULL);
    assert(edits != NULL);
    LumilaFileEdit *e = edits->data;
    /* The full path must be preserved, not just the basename. */
    assert(strcmp(e->file_name, "src/providers/openai.c") == 0);
    lumila_utils_file_edits_free(edits);
    printf("test_parse_file_edit_path: PASS\n");
}

static void test_parse_file_edit_multiple(void)
{
    const char *resp =
        "```file:a.c\nint a;\n```\n"
        "between\n"
        "```file:b.h\nint b;\n```\n";
    GList *edits = lumila_utils_parse_file_edits(resp, NULL);
    assert(edits != NULL && edits->next != NULL && edits->next->next == NULL);
    assert(strcmp(((LumilaFileEdit *)edits->data)->file_name, "a.c") == 0);
    assert(strcmp(((LumilaFileEdit *)edits->next->data)->file_name, "b.h") == 0);
    lumila_utils_file_edits_free(edits);
    printf("test_parse_file_edit_multiple: PASS\n");
}

static void test_parse_no_edits(void)
{
    const char *resp = "Just a normal answer.\n```c\ncode\n```\n";
    gchar *cleaned = NULL;
    GList *edits = lumila_utils_parse_file_edits(resp, &cleaned);
    assert(edits == NULL);
    assert(cleaned != NULL && strstr(cleaned, "```c") != NULL);
    g_free(cleaned);
    printf("test_parse_no_edits: PASS\n");
}

static void test_error_classification(void)
{
    assert(lumila_utils_is_error_response("Error: something failed"));
    assert(lumila_utils_is_error_response("HTTP Error 401: Unauthorized"));
    assert(lumila_utils_is_error_response("API Error: bad request"));
    assert(lumila_utils_is_error_response("JSON Parse Error: x"));
    assert(!lumila_utils_is_error_response("Here is your code:"));
    assert(lumila_utils_is_error_response(NULL));

    assert(lumila_utils_is_transient_response("Transient HTTP Error 429"));
    assert(lumila_utils_is_transient_response("Error: Connection timed out"));
    assert(lumila_utils_is_transient_response("API Error: overloaded_error: Overloaded"));
    assert(lumila_utils_is_transient_response("API Error: rate limit exceeded"));
    assert(!lumila_utils_is_transient_response("API Error: invalid api key"));
    assert(!lumila_utils_is_transient_response("HTTP Error 401: Unauthorized"));
    assert(!lumila_utils_is_transient_response("Normal answer"));
    printf("test_error_classification: PASS\n");
}

int main(void)
{
    printf("=== Utils Tests ===\n");
    test_parse_file_edit_simple();
    test_parse_file_edit_nested_fences();
    test_parse_file_edit_path();
    test_parse_file_edit_multiple();
    test_parse_no_edits();
    test_error_classification();
    printf("=== All tests passed ===\n");
    return 0;
}
