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

/* Count ops of a given kind in a diff result. */
static int diff_count(GArray *ops, LumilaDiffKind kind)
{
    int n = 0;
    for (guint i = 0; i < ops->len; i++) {
        if (g_array_index(ops, LumilaDiffOp, i).kind == kind) n++;
    }
    return n;
}

static void test_diff_identical(void)
{
    GArray *ops = lumila_utils_diff_lines("a\nb\nc\n", "a\nb\nc\n");
    assert(ops->len == 3);
    assert(diff_count(ops, LUMILA_DIFF_KEEP) == 3);
    assert(diff_count(ops, LUMILA_DIFF_DEL) == 0);
    assert(diff_count(ops, LUMILA_DIFF_INS) == 0);
    lumila_utils_diff_ops_free(ops);
    printf("test_diff_identical: PASS\n");
}

static void test_diff_append(void)
{
    GArray *ops = lumila_utils_diff_lines("a\nb\n", "a\nb\nc\nd\n");
    assert(diff_count(ops, LUMILA_DIFF_KEEP) == 2);
    assert(diff_count(ops, LUMILA_DIFF_INS) == 2);
    assert(diff_count(ops, LUMILA_DIFF_DEL) == 0);
    /* Ins lines must carry their new-content line index. */
    LumilaDiffOp *last = &g_array_index(ops, LumilaDiffOp, ops->len - 1);
    assert(last->kind == LUMILA_DIFF_INS && last->new_line == 3);
    lumila_utils_diff_ops_free(ops);
    printf("test_diff_append: PASS\n");
}

static void test_diff_delete(void)
{
    GArray *ops = lumila_utils_diff_lines("a\nb\nc\nd\n", "a\nd\n");
    assert(diff_count(ops, LUMILA_DIFF_KEEP) == 2);
    assert(diff_count(ops, LUMILA_DIFF_DEL) == 2);
    assert(diff_count(ops, LUMILA_DIFF_INS) == 0);
    lumila_utils_diff_ops_free(ops);
    printf("test_diff_delete: PASS\n");
}

static void test_diff_modify(void)
{
    GArray *ops = lumila_utils_diff_lines("a\nold\nc\n", "a\nnew\nc\n");
    assert(diff_count(ops, LUMILA_DIFF_KEEP) == 2);
    assert(diff_count(ops, LUMILA_DIFF_DEL) == 1);
    assert(diff_count(ops, LUMILA_DIFF_INS) == 1);
    /* DEL comes before INS for a modified line. */
    int del_idx = -1, ins_idx = -1;
    for (guint i = 0; i < ops->len; i++) {
        LumilaDiffOp *op = &g_array_index(ops, LumilaDiffOp, i);
        if (op->kind == LUMILA_DIFF_DEL) { del_idx = (int)i; assert(strcmp(op->text, "old") == 0); }
        if (op->kind == LUMILA_DIFF_INS) { ins_idx = (int)i; assert(strcmp(op->text, "new") == 0); assert(op->new_line == 1); }
    }
    assert(del_idx >= 0 && ins_idx > del_idx);
    lumila_utils_diff_ops_free(ops);
    printf("test_diff_modify: PASS\n");
}

static void test_diff_common_prefix_suffix(void)
{
    const char *old = "h1\nh2\nmid_old1\nmid_old2\nt1\nt2\n";
    const char *new = "h1\nh2\nmid_new\nt1\nt2\n";
    GArray *ops = lumila_utils_diff_lines(old, new);
    assert(diff_count(ops, LUMILA_DIFF_KEEP) == 4);
    assert(diff_count(ops, LUMILA_DIFF_DEL) == 2);
    assert(diff_count(ops, LUMILA_DIFF_INS) == 1);
    lumila_utils_diff_ops_free(ops);
    printf("test_diff_common_prefix_suffix: PASS\n");
}

static void test_diff_empty(void)
{
    /* Old empty -> everything is INS; new empty -> everything is DEL. */
    GArray *ops = lumila_utils_diff_lines("", "a\nb\n");
    assert(diff_count(ops, LUMILA_DIFF_INS) == 2);
    assert(diff_count(ops, LUMILA_DIFF_KEEP) == 0);
    lumila_utils_diff_ops_free(ops);

    ops = lumila_utils_diff_lines("a\nb\n", "");
    assert(diff_count(ops, LUMILA_DIFF_DEL) == 2);
    lumila_utils_diff_ops_free(ops);
    printf("test_diff_empty: PASS\n");
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
    test_diff_identical();
    test_diff_append();
    test_diff_delete();
    test_diff_modify();
    test_diff_common_prefix_suffix();
    test_diff_empty();
    printf("=== All tests passed ===\n");
    return 0;
}
