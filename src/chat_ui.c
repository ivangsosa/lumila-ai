#include "chat_ui.h"
#include <string.h>
#include <time.h>

static GtkTextTag *user_tag = NULL;
static GtkTextTag *ai_tag = NULL;
static GtkTextTag *background_tag = NULL;
static GtkTextTag *user_name_tag = NULL;
static GtkTextTag *ai_name_tag = NULL;
static GtkTextTag *timestamp_tag = NULL;
static GtkTextTag *code_block_tag = NULL;
static GtkTextTag *code_keyword_tag = NULL;
static GtkTextTag *code_string_tag = NULL;
static GtkTextTag *code_comment_tag = NULL;
static GtkTextTag *code_number_tag = NULL;
static GtkTextTag *code_function_tag = NULL;

void lumila_chat_ui_init(GtkTextView *view)
{
    if (!view) return;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);

    // Create text tags for styling - user message without background
    user_tag = gtk_text_buffer_create_tag(buffer, "user",
        "justification", GTK_JUSTIFY_RIGHT,
        "left-margin", 80,
        "right-margin", 10,
        "pixels-above-lines", 10,
        "pixels-below-lines", 10,
        "foreground", "#82AAFF",  // Azul moderno brillante
        "wrap-mode", GTK_WRAP_WORD,
        NULL);

    ai_tag = gtk_text_buffer_create_tag(buffer, "ai",
        "justification", GTK_JUSTIFY_LEFT,
        "left-margin", 10,
        "right-margin", 80,
        "pixels-above-lines", 10,
        "pixels-below-lines", 10,
        "foreground", "#C8D3F5",  // Blanco azulado suave
        "wrap-mode", GTK_WRAP_WORD,
        NULL);

    // Sender name tags
    user_name_tag = gtk_text_buffer_create_tag(buffer, "user_name",
        "justification", GTK_JUSTIFY_RIGHT,
        "left-margin", 80,
        "right-margin", 10,
        "pixels-above-lines", 14,
        "pixels-below-lines", 4,
        "foreground", "#82AAFF",
        "weight", PANGO_WEIGHT_BOLD,
        NULL);

    ai_name_tag = gtk_text_buffer_create_tag(buffer, "ai_name",
        "justification", GTK_JUSTIFY_LEFT,
        "left-margin", 10,
        "right-margin", 80,
        "pixels-above-lines", 14,
        "pixels-below-lines", 4,
        "foreground", "#C3E88D",  // Verde moderno
        "weight", PANGO_WEIGHT_BOLD,
        NULL);

    // Timestamp tag
    timestamp_tag = gtk_text_buffer_create_tag(buffer, "timestamp",
        "justification", GTK_JUSTIFY_RIGHT,
        "left-margin", 60,
        "right-margin", 10,
        "pixels-above-lines", 2,
        "pixels-below-lines", 8,
        "foreground", "#888888",
        "size-points", 8.0,
        "style", PANGO_STYLE_ITALIC,
        NULL);

    // Code block tag — dark container with visual separation
    code_block_tag = gtk_text_buffer_create_tag(buffer, "code_block",
        "background", "#16162a",
        "foreground", "#D4D4D4",
        "family", "Monospace",
        "wrap-mode", GTK_WRAP_NONE,
        "left-margin", 16,
        "right-margin", 40,
        "pixels-above-lines", 6,
        "pixels-below-lines", 6,
        NULL);

    // Syntax highlighting tags
    code_keyword_tag = gtk_text_buffer_create_tag(buffer, "code_keyword",
        "foreground", "#569CD6",  // Azul para keywords
        "weight", PANGO_WEIGHT_BOLD,
        NULL);

    code_string_tag = gtk_text_buffer_create_tag(buffer, "code_string",
        "foreground", "#CE9178",  // Naranja para strings
        NULL);

    code_comment_tag = gtk_text_buffer_create_tag(buffer, "code_comment",
        "foreground", "#6A9955",  // Verde para comentarios
        "style", PANGO_STYLE_ITALIC,
        NULL);

    code_number_tag = gtk_text_buffer_create_tag(buffer, "code_number",
        "foreground", "#B5CEA8",  // Verde claro para números
        NULL);

    code_function_tag = gtk_text_buffer_create_tag(buffer, "code_function",
        "foreground", "#DCDCAA",  // Amarillo para funciones
        NULL);

    background_tag = gtk_text_buffer_create_tag(buffer, "background",
        "background", "#000000",
        NULL);

    // Apply CSS for modern dark background
    GtkCssProvider *css_provider = gtk_css_provider_new();
    const gchar *css_data =
        "textview {"
        "  background-color: #0f0f23;"
        "  color: #C8D3F5;"
        "}"
        "textview text {"
        "  background-color: #0f0f23;"
        "}";

    gtk_css_provider_load_from_data(css_provider, css_data, -1, NULL);

    GtkStyleContext *context = gtk_widget_get_style_context(GTK_WIDGET(view));
    gtk_style_context_add_provider(context,
                                    GTK_STYLE_PROVIDER(css_provider),
                                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_object_unref(css_provider);
}

static gchar *get_current_time_string(void)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    gchar *time_str = g_strdup_printf("%02d:%02d", tm_info->tm_hour, tm_info->tm_min);
    return time_str;
}

/* ============================================================================
 * Language-aware syntax highlighting
 * ============================================================================ */

typedef struct {
    const gchar **keywords;
    const gchar *comment_single;
    const gchar *comment_multi_start;
    const gchar *comment_multi_end;
    gboolean hash_comments;
    gboolean dash_comments;
    gboolean html_tags;
    gboolean css_mode;
    gboolean shell_vars;
    gboolean backtick_strings;
    gboolean triple_strings;
    gboolean template_literals;
} LanguageProfile;

static const gchar *kw_python[] = {
    "False","None","True","and","as","assert","async","await","break","class",
    "continue","def","del","elif","else","except","finally","for","from","global",
    "if","import","in","is","lambda","nonlocal","not","or","pass","raise","return",
    "try","while","with","yield","self","print","len","range","list","dict","set",
    "tuple","str","int","float","bool","open","input","map","filter","zip","enumerate",
    NULL
};

static const gchar *kw_js_ts[] = {
    "break","case","catch","class","const","continue","debugger","default","delete",
    "do","else","export","extends","finally","for","function","if","import","in",
    "instanceof","let","new","return","super","switch","this","throw","try","typeof",
    "var","void","while","with","yield","async","await","interface","type","enum",
    "implements","package","private","protected","public","static","readonly","namespace",
    "declare","abstract","as","any","boolean","constructor","from","get","module",
    "number","of","require","set","string","symbol","undefined","null","true","false",
    "console","document","window","Array","Object","String","Number","Boolean","Promise",
    NULL
};

static const gchar *kw_c_cpp[] = {
    "auto","break","case","char","const","continue","default","do","double","else",
    "enum","extern","float","for","goto","if","inline","int","long","register",
    "restrict","return","short","signed","sizeof","static","struct","switch","typedef",
    "union","unsigned","void","volatile","while","_Alignas","_Alignof","_Atomic",
    "_Bool","_Complex","_Generic","_Imaginary","_Noreturn","_Static_assert","_Thread_local",
    "class","public","private","protected","virtual","explicit","override","final",
    "nullptr","constexpr","decltype","noexcept","template","typename","using","new",
    "delete","try","catch","throw","namespace","operator","friend","mutable",
    "std","cout","cin","endl","printf","scanf","malloc","free","NULL","true","false",
    NULL
};

static const gchar *kw_rust[] = {
    "as","async","await","break","const","continue","crate","dyn","else","enum",
    "extern","false","fn","for","if","impl","in","let","loop","match","mod","move",
    "mut","pub","ref","return","self","Self","static","struct","super","trait",
    "true","type","unsafe","use","where","while","yield","Option","Result","Some",
    "None","Ok","Err","String","Vec","Box","Rc","Arc","println","println!","format!",
    "vec!","panic!","assert!","macro_rules!",
    NULL
};

static const gchar *kw_go[] = {
    "break","case","chan","const","continue","default","defer","else","fallthrough",
    "for","func","go","goto","if","import","interface","map","package","range",
    "return","select","struct","switch","type","var","nil","true","false","make",
    "new","append","copy","delete","len","cap","panic","recover","print","println",
    NULL
};

static const gchar *kw_php[] = {
    "abstract","and","array","as","break","callable","case","catch","class","clone",
    "const","continue","declare","default","die","do","echo","else","elseif","empty",
    "enddeclare","endfor","endforeach","endif","endswitch","endwhile","eval","exit",
    "extends","final","finally","fn","for","foreach","function","global","goto",
    "if","implements","include","include_once","instanceof","insteadof","interface",
    "isset","list","match","namespace","new","or","print","private","protected",
    "public","readonly","require","require_once","return","static","switch","throw",
    "trait","try","unset","use","var","while","xor","yield","yield_from","true",
    "false","null","NULL","TRUE","FALSE","$this","self","parent","static",
    NULL
};

static const gchar *kw_html[] = {
    "!DOCTYPE","a","abbr","address","area","article","aside","audio","b","base",
    "bdi","bdo","blockquote","body","br","button","canvas","caption","cite","code",
    "col","colgroup","data","datalist","dd","del","details","dfn","dialog","div",
    "dl","dt","em","embed","fieldset","figcaption","figure","footer","form","h1",
    "h2","h3","h4","h5","h6","head","header","hr","html","i","iframe","img","input",
    "ins","kbd","label","legend","li","link","main","map","mark","meta","meter",
    "nav","noscript","object","ol","optgroup","option","output","p","param","picture",
    "pre","progress","q","rp","rt","ruby","s","samp","script","section","select",
    "small","source","span","strong","style","sub","summary","sup","table","tbody",
    "td","template","textarea","tfoot","th","thead","time","title","tr","track",
    "u","ul","var","video","wbr",
    NULL
};

static const gchar *kw_css[] = {
    "align-content","align-items","align-self","all","animation","background",
    "border","border-radius","bottom","box-shadow","box-sizing","color","content",
    "cursor","display","flex","flex-direction","float","font","font-family",
    "font-size","font-weight","grid","height","import","justify-content","left",
    "line-height","margin","max-height","max-width","min-height","min-width","opacity",
    "overflow","padding","position","right","text-align","text-decoration","top",
    "transform","transition","visibility","width","z-index","hover","active","focus",
    "important","inherit","initial","unset","relative","absolute","fixed","sticky",
    "block","inline","inline-block","none","hidden","auto","center","space-between",
    "space-around","stretch","row","column","wrap","nowrap","solid","dashed","dotted",
    NULL
};

static const gchar *kw_bash[] = {
    "alias","bg","bind","break","builtin","case","cd","command","continue","declare",
    "dirs","disown","do","done","echo","elif","else","enable","esac","eval","exec",
    "exit","export","false","fc","fg","fi","for","function","getopts","hash","help",
    "history","if","in","jobs","kill","let","local","logout","mapfile","popd",
    "printf","pushd","pwd","read","readonly","return","select","set","shift","shopt",
    "source","suspend","test","then","time","times","trap","true","type","typeset",
    "ulimit","umask","unalias","unset","until","wait","while","bash","sh","zsh",
    NULL
};

static const gchar *kw_sql[] = {
    "ADD","ALL","ALTER","AND","ANY","AS","ASC","AUTHORIZATION","BACKUP","BEGIN",
    "BETWEEN","BREAK","BROWSE","BULK","BY","CASCADE","CASE","CHECK","CHECKPOINT",
    "CLOSE","CLUSTERED","COALESCE","COLLATE","COLUMN","COMMIT","COMPUTE","CONNECT",
    "CONSTRAINT","CONTAINS","CONTINUE","CONVERT","CREATE","CROSS","CURRENT",
    "CURRENT_DATE","CURRENT_TIME","CURRENT_TIMESTAMP","CURRENT_USER","CURSOR",
    "DATABASE","DBCC","DEALLOCATE","DECLARE","DEFAULT","DELETE","DENY","DESC",
    "DISK","DISTINCT","DISTRIBUTED","DOUBLE","DROP","DUMP","ELSE","END","ERRLVL",
    "ESCAPE","EXCEPT","EXEC","EXECUTE","EXISTS","EXIT","EXTERNAL","FETCH","FILE",
    "FILLFACTOR","FOR","FOREIGN","FREETEXT","FREETEXTTABLE","FROM","FULL","FUNCTION",
    "GOTO","GRANT","GROUP","HAVING","HOLDLOCK","IDENTITY","IDENTITY_INSERT","IDENTITYCOL",
    "IF","IN","INDEX","INNER","INSERT","INTERSECT","INTO","IS","JOIN","KEY","KILL",
    "LEFT","LIKE","LINENO","LOAD","MERGE","NATIONAL","NOCHECK","NONCLUSTERED","NOT",
    "NULL","NULLIF","OF","OFF","OFFSETS","ON","OPEN","OPENDATASOURCE","OPENQUERY",
    "OPENROWSET","OPENXML","OPTION","OR","ORDER","OUTER","OVER","PERCENT","PIVOT",
    "PLAN","PRECISION","PRIMARY","PRINT","PROC","PROCEDURE","PUBLIC","RAISERROR",
    "READ","READTEXT","RECONFIGURE","REFERENCES","REPLICATION","RESTORE","RESTRICT",
    "RETURN","REVERT","REVOKE","RIGHT","ROLLBACK","ROWCOUNT","ROWGUIDCOL","RULE",
    "SAVE","SCHEMA","SECURITYAUDIT","SELECT","SEMANTICKEYPHRASETABLE","SESSION_USER",
    "SET","SETUSER","SHUTDOWN","SOME","STATISTICS","SYSTEM_USER","TABLE","TABLESAMPLE",
    "TEXTSIZE","THEN","TO","TOP","TRAN","TRANSACTION","TRIGGER","TRUNCATE","TRY_CONVERT",
    "TSEQUAL","UNION","UNIQUE","UNPIVOT","UPDATE","UPDATETEXT","USE","USER","VALUES",
    "VARYING","VIEW","WAITFOR","WHERE","WHILE","WITH","WRITETEXT",
    NULL
};

static const gchar *kw_java[] = {
    "abstract","assert","boolean","break","byte","case","catch","char","class",
    "const","continue","default","do","double","else","enum","extends","final",
    "finally","float","for","goto","if","implements","import","instanceof","int",
    "interface","long","native","new","package","private","protected","public",
    "return","short","static","strictfp","super","switch","synchronized","this",
    "throw","throws","transient","try","void","volatile","while","true","false",
    "null","String","System","Object","Integer","Double","Boolean","List","Map",
    "ArrayList","HashMap","out","println","print",
    NULL
};

static const gchar *kw_ruby[] = {
    "BEGIN","END","alias","and","begin","break","case","class","def","defined?",
    "do","else","elsif","end","ensure","false","for","if","in","module","next","nil",
    "not","or","redo","rescue","retry","return","self","super","then","true","undef",
    "unless","until","when","while","yield","puts","print","require","include","extend",
    NULL
};

static const gchar *kw_json[] = {
    "true","false","null",
    NULL
};

static const gchar *kw_markdown[] = {
    NULL
};

static const LanguageProfile profile_python = {
    kw_python, NULL, NULL, NULL, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE
};
static const LanguageProfile profile_js      = {
    kw_js_ts, "//", "/*", "*/", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE
};
static const LanguageProfile profile_ts      = {
    kw_js_ts, "//", "/*", "*/", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE
};
static const LanguageProfile profile_c       = {
    kw_c_cpp, "//", "/*", "*/", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE
};
static const LanguageProfile profile_cpp     = {
    kw_c_cpp, "//", "/*", "*/", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE
};
static const LanguageProfile profile_rust    = {
    kw_rust, "//", "/*", "*/", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE
};
static const LanguageProfile profile_go      = {
    kw_go, "//", "/*", "*/", FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE
};
static const LanguageProfile profile_php     = {
    kw_php, "//", "/*", "*/", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE
};
static const LanguageProfile profile_html    = {
    kw_html, NULL, "<!--", "-->", FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE
};
static const LanguageProfile profile_css     = {
    kw_css, "//", "/*", "*/", FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE
};
static const LanguageProfile profile_bash    = {
    kw_bash, NULL, NULL, NULL, TRUE, FALSE, FALSE, FALSE, TRUE, TRUE, FALSE, FALSE
};
static const LanguageProfile profile_sql     = {
    kw_sql, "--", "/*", "*/", FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE
};
static const LanguageProfile profile_java    = {
    kw_java, "//", "/*", "*/", FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE
};
static const LanguageProfile profile_ruby    = {
    kw_ruby, NULL, "=begin", "=end", TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE
};
static const LanguageProfile profile_json    = {
    kw_json, NULL, NULL, NULL, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE
};
static const LanguageProfile profile_md      = {
    kw_markdown, NULL, NULL, NULL, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE
};

typedef struct {
    const gchar *name;
    const LanguageProfile *profile;
} LangMap;

static const LangMap lang_map[] = {
    {"python",   &profile_python},
    {"py",       &profile_python},
    {"javascript", &profile_js},
    {"js",       &profile_js},
    {"typescript", &profile_ts},
    {"ts",       &profile_ts},
    {"c",        &profile_c},
    {"cpp",      &profile_cpp},
    {"c++",      &profile_cpp},
    {"cxx",      &profile_cpp},
    {"rust",     &profile_rust},
    {"rs",       &profile_rust},
    {"go",       &profile_go},
    {"golang",   &profile_go},
    {"php",      &profile_php},
    {"html",     &profile_html},
    {"htm",      &profile_html},
    {"css",      &profile_css},
    {"bash",     &profile_bash},
    {"sh",       &profile_bash},
    {"shell",    &profile_bash},
    {"zsh",      &profile_bash},
    {"sql",      &profile_sql},
    {"java",     &profile_java},
    {"ruby",     &profile_ruby},
    {"rb",       &profile_ruby},
    {"json",     &profile_json},
    {"markdown", &profile_md},
    {"md",       &profile_md},
    {"yaml",     &profile_python},  // reuse generic
    {"yml",      &profile_python},
    {"toml",     &profile_python},
    {"xml",      &profile_html},    // reuse html tag logic
    {"dockerfile", &profile_bash},
    {"makefile", &profile_bash},
    {NULL, NULL}
};

static const LanguageProfile *lookup_language_profile(const gchar *lang)
{
    if (!lang || !*lang) return NULL;
    for (gint i = 0; lang_map[i].name != NULL; i++) {
        if (g_ascii_strcasecmp(lang, lang_map[i].name) == 0) {
            return lang_map[i].profile;
        }
    }
    return NULL;
}

static gboolean is_keyword_in_profile(const gchar *word, const LanguageProfile *prof)
{
    if (!prof || !prof->keywords || !word || !*word) return FALSE;
    for (gint i = 0; prof->keywords[i] != NULL; i++) {
        if (g_strcmp0(word, prof->keywords[i]) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

static gchar *extract_language_from_fence(const gchar *line)
{
    if (!g_str_has_prefix(line, "```")) return NULL;
    const gchar *lang = line + 3;
    while (*lang && g_ascii_isspace(*lang)) lang++;
    if (!*lang) return NULL;
    GString *s = g_string_new("");
    while (*lang && !g_ascii_isspace(*lang)) {
        g_string_append_c(s, *lang++);
    }
    gchar *result = g_string_free(s, FALSE);
    if (!*result) {
        g_free(result);
        return NULL;
    }
    return result;
}

static void insert_code_with_highlighting(GtkTextBuffer *buffer, GtkTextIter *iter,
                                          const gchar *line, const LanguageProfile *prof,
                                          gboolean *in_multi_comment)
{
    const gchar *p = line;
    GString *token = g_string_new("");

    // If we are inside a multi-line comment, consume until end marker
    if (in_multi_comment && *in_multi_comment && prof && prof->comment_multi_end) {
        const gchar *end = strstr(p, prof->comment_multi_end);
        if (end) {
            gsize len = end - p + strlen(prof->comment_multi_end);
            gtk_text_buffer_insert_with_tags(buffer, iter, p, len, code_block_tag, code_comment_tag, NULL);
            p += len;
            *in_multi_comment = FALSE;
        } else {
            gtk_text_buffer_insert_with_tags(buffer, iter, p, -1, code_block_tag, code_comment_tag, NULL);
            g_string_free(token, TRUE);
            return;
        }
    }

    while (*p) {
        // Skip whitespace
        if (g_ascii_isspace(*p)) {
            if (token->len > 0) {
                gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_block_tag, NULL);
                g_string_truncate(token, 0);
            }
            gchar spaces[2] = {*p, '\0'};
            gtk_text_buffer_insert_with_tags(buffer, iter, spaces, -1, code_block_tag, NULL);
            p++;
            continue;
        }

        // Multi-line comment start
        if (prof && prof->comment_multi_start && prof->comment_multi_end) {
            gsize slen = strlen(prof->comment_multi_start);
            if (strncmp(p, prof->comment_multi_start, slen) == 0) {
                if (token->len > 0) {
                    gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_block_tag, NULL);
                    g_string_truncate(token, 0);
                }
                const gchar *end = strstr(p + slen, prof->comment_multi_end);
                if (end) {
                    gsize len = end - p + strlen(prof->comment_multi_end);
                    gtk_text_buffer_insert_with_tags(buffer, iter, p, len, code_block_tag, code_comment_tag, NULL);
                    p += len;
                } else {
                    gtk_text_buffer_insert_with_tags(buffer, iter, p, -1, code_block_tag, code_comment_tag, NULL);
                    if (in_multi_comment) *in_multi_comment = TRUE;
                    g_string_free(token, TRUE);
                    return;
                }
                continue;
            }
        }

        // Single-line comments
        if (prof && prof->comment_single) {
            gsize slen = strlen(prof->comment_single);
            if (strncmp(p, prof->comment_single, slen) == 0) {
                if (token->len > 0) {
                    gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_block_tag, NULL);
                    g_string_truncate(token, 0);
                }
                gtk_text_buffer_insert_with_tags(buffer, iter, p, -1, code_block_tag, code_comment_tag, NULL);
                break;
            }
        }
        if (prof && prof->hash_comments && *p == '#') {
            if (token->len > 0) {
                gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_block_tag, NULL);
                g_string_truncate(token, 0);
            }
            gtk_text_buffer_insert_with_tags(buffer, iter, p, -1, code_block_tag, code_comment_tag, NULL);
            break;
        }
        if (prof && prof->dash_comments && *p == '-' && *(p+1) == '-') {
            if (token->len > 0) {
                gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_block_tag, NULL);
                g_string_truncate(token, 0);
            }
            gtk_text_buffer_insert_with_tags(buffer, iter, p, -1, code_block_tag, code_comment_tag, NULL);
            break;
        }

        // HTML/XML tags
        if (prof && prof->html_tags && *p == '<') {
            if (token->len > 0) {
                gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_block_tag, NULL);
                g_string_truncate(token, 0);
            }
            gchar delimiter = '>';
            g_string_append_c(token, *p++);
            while (*p && *p != delimiter) {
                g_string_append_c(token, *p++);
            }
            if (*p == delimiter) {
                g_string_append_c(token, *p++);
            }
            gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_block_tag, code_keyword_tag, NULL);
            g_string_truncate(token, 0);
            continue;
        }

        // Shell variables $VAR or ${VAR}
        if (prof && prof->shell_vars && *p == '$') {
            if (token->len > 0) {
                gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_block_tag, NULL);
                g_string_truncate(token, 0);
            }
            g_string_append_c(token, *p++);
            if (*p == '{') {
                g_string_append_c(token, *p++);
                while (*p && *p != '}') {
                    g_string_append_c(token, *p++);
                }
                if (*p == '}') g_string_append_c(token, *p++);
            } else {
                while (*p && (g_ascii_isalnum(*p) || *p == '_')) {
                    g_string_append_c(token, *p++);
                }
            }
            gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_block_tag, code_number_tag, NULL);
            g_string_truncate(token, 0);
            continue;
        }

        // Template literals / backtick strings
        if (prof && prof->template_literals && *p == '`') {
            if (token->len > 0) {
                gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_block_tag, NULL);
                g_string_truncate(token, 0);
            }
            g_string_append_c(token, *p++);
            while (*p && *p != '`') {
                if (*p == '\\' && *(p+1)) {
                    g_string_append_c(token, *p++);
                }
                g_string_append_c(token, *p++);
            }
            if (*p == '`') g_string_append_c(token, *p++);
            gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_block_tag, code_string_tag, NULL);
            g_string_truncate(token, 0);
            continue;
        }

        // Go raw string literals `...`
        if (prof && prof->backtick_strings && *p == '`') {
            if (token->len > 0) {
                gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_block_tag, NULL);
                g_string_truncate(token, 0);
            }
            g_string_append_c(token, *p++);
            while (*p && *p != '`') {
                g_string_append_c(token, *p++);
            }
            if (*p == '`') g_string_append_c(token, *p++);
            gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_block_tag, code_string_tag, NULL);
            g_string_truncate(token, 0);
            continue;
        }

        // Regular strings "..." and '...'
        if (*p == '"' || *p == '\'') {
            if (token->len > 0) {
                gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_block_tag, NULL);
                g_string_truncate(token, 0);
            }
            gchar delimiter = *p;
            g_string_append_c(token, *p++);
            while (*p && *p != delimiter) {
                if (*p == '\\' && *(p+1)) {
                    g_string_append_c(token, *p++);
                }
                g_string_append_c(token, *p++);
            }
            if (*p == delimiter) g_string_append_c(token, *p++);
            gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_block_tag, code_string_tag, NULL);
            g_string_truncate(token, 0);
            continue;
        }

        // Numbers: hex 0x..., oct 0o..., bin 0b..., floats, scientific
        if (g_ascii_isdigit(*p) || (*p == '.' && g_ascii_isdigit(*(p+1)))) {
            if (token->len > 0) {
                gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_block_tag, NULL);
                g_string_truncate(token, 0);
            }
            if (*p == '0' && (*(p+1) == 'x' || *(p+1) == 'X')) {
                g_string_append_c(token, *p++);
                g_string_append_c(token, *p++);
                while (*p && g_ascii_isxdigit(*p)) g_string_append_c(token, *p++);
            } else if (*p == '0' && (*(p+1) == 'o' || *(p+1) == 'O')) {
                g_string_append_c(token, *p++);
                g_string_append_c(token, *p++);
                while (*p && (*p >= '0' && *p <= '7')) g_string_append_c(token, *p++);
            } else if (*p == '0' && (*(p+1) == 'b' || *(p+1) == 'B')) {
                g_string_append_c(token, *p++);
                g_string_append_c(token, *p++);
                while (*p && (*p == '0' || *p == '1')) g_string_append_c(token, *p++);
            } else {
                while (*p && (g_ascii_isdigit(*p) || *p == '.')) g_string_append_c(token, *p++);
                if (*p == 'e' || *p == 'E') {
                    g_string_append_c(token, *p++);
                    if (*p == '+' || *p == '-') g_string_append_c(token, *p++);
                    while (*p && g_ascii_isdigit(*p)) g_string_append_c(token, *p++);
                }
            }
            gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_block_tag, code_number_tag, NULL);
            g_string_truncate(token, 0);
            continue;
        }

        // CSS mode: property: value; (very basic detection)
        if (prof && prof->css_mode) {
            if (g_ascii_isalpha(*p) || *p == '-') {
                g_string_append_c(token, *p++);
                while (*p && (g_ascii_isalnum(*p) || *p == '-' || *p == '_')) {
                    g_string_append_c(token, *p++);
                }
                while (*p && g_ascii_isspace(*p)) p++;
                if (*p == ':') {
                    gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_block_tag, code_keyword_tag, NULL);
                    g_string_truncate(token, 0);
                    gtk_text_buffer_insert_with_tags(buffer, iter, ":", 1, code_block_tag, NULL);
                    p++;
                    continue;
                } else {
                    gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_block_tag, NULL);
                    g_string_truncate(token, 0);
                    continue;
                }
            }
        }

        // Build word token
        if (g_ascii_isalnum(*p) || *p == '_') {
            g_string_append_c(token, *p++);
        } else {
            if (token->len > 0) {
                if (is_keyword_in_profile(token->str, prof)) {
                    gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_block_tag, code_keyword_tag, NULL);
                } else {
                    gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_block_tag, NULL);
                }
                g_string_truncate(token, 0);
            }
            gchar symbol[2] = {*p, '\0'};
            gtk_text_buffer_insert_with_tags(buffer, iter, symbol, -1, code_block_tag, NULL);
            p++;
        }
    }

    // Handle remaining token
    if (token->len > 0) {
        if (is_keyword_in_profile(token->str, prof)) {
            gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_block_tag, code_keyword_tag, NULL);
        } else {
            gtk_text_buffer_insert_with_tags(buffer, iter, token->str, -1, code_block_tag, NULL);
        }
    }

    g_string_free(token, TRUE);
}

void lumila_chat_ui_append_user_message(GtkTextView *view, const gchar *message)
{
    if (!view || !message) return;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);

    // Add spacing between messages
    if (gtk_text_buffer_get_char_count(buffer) > 0) {
        gtk_text_buffer_insert(buffer, &end, "\n", -1);
    }

    // Insert sender name
    gtk_text_buffer_insert_with_tags(buffer, &end, "You", -1, user_name_tag, NULL);
    gtk_text_buffer_insert(buffer, &end, "\n", -1);

    // Insert message content with user bubble styling
    gtk_text_buffer_insert_with_tags(buffer, &end, message, -1, user_tag, NULL);
    gtk_text_buffer_insert(buffer, &end, "\n", -1);

    // Insert timestamp
    // gchar *time_str = get_current_time_string();
    // gtk_text_buffer_insert_with_tags(buffer, &end, time_str, -1, timestamp_tag, NULL);
    // g_free(time_str);
    // gtk_text_buffer_insert(buffer, &end, "\n", -1);

    // Scroll to end
    GtkTextMark *mark = gtk_text_buffer_get_insert(buffer);
    gtk_text_view_scroll_to_mark(view, mark, 0.0, FALSE, 0.0, 0.0);
}

void lumila_chat_ui_append_ai_message(GtkTextView *view, const gchar *message)
{
    if (!view || !message) return;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);

    // Add spacing between messages
    if (gtk_text_buffer_get_char_count(buffer) > 0) {
        gtk_text_buffer_insert(buffer, &end, "\n", -1);
    }

    // Insert sender name
    gtk_text_buffer_insert_with_tags(buffer, &end, "Lumila", -1, ai_name_tag, NULL);
    gtk_text_buffer_insert(buffer, &end, "\n", -1);

    // Check for code blocks and format them
    gchar **lines = g_strsplit(message, "\n", -1);
    gboolean in_code_block = FALSE;
    const LanguageProfile *current_profile = NULL;
    gboolean in_multi_comment = FALSE;

    for (gint i = 0; lines[i] != NULL; i++) {
        gchar *line = lines[i];

        // Check for code block markers
        if (g_str_has_prefix(line, "```")) {
            if (!in_code_block) {
                gchar *lang = extract_language_from_fence(line);
                current_profile = lookup_language_profile(lang);
                g_free(lang);
                in_code_block = TRUE;
                in_multi_comment = FALSE;
            } else {
                in_code_block = FALSE;
                current_profile = NULL;
                in_multi_comment = FALSE;
                gtk_text_buffer_insert(buffer, &end, "\n", -1);
            }
            continue;
        }

        if (in_code_block) {
            insert_code_with_highlighting(buffer, &end, line, current_profile, &in_multi_comment);
            gtk_text_buffer_insert(buffer, &end, "\n", -1);
        } else {
            // Insert regular message with AI styling
            gtk_text_buffer_insert_with_tags(buffer, &end, line, -1, ai_tag, NULL);
            if (lines[i+1] != NULL) {
                gtk_text_buffer_insert(buffer, &end, "\n", -1);
            }
        }
    }

    g_strfreev(lines);
    gtk_text_buffer_insert(buffer, &end, "\n", -1);

    // Insert timestamp
    // gchar *time_str = get_current_time_string();
    // gtk_text_buffer_insert_with_tags(buffer, &end, time_str, -1, timestamp_tag, NULL);
    // g_free(time_str);
    // gtk_text_buffer_insert(buffer, &end, "\n", -1);

    // Scroll to end
    GtkTextMark *mark = gtk_text_buffer_get_insert(buffer);
    gtk_text_view_scroll_to_mark(view, mark, 0.0, FALSE, 0.0, 0.0);
}

void lumila_chat_ui_clear(GtkTextView *view)
{
    if (!view) return;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gtk_text_buffer_delete(buffer, &start, &end);
}

/* Streaming UI implementation */
static GtkTextMark *stream_mark = NULL;

void lumila_chat_ui_stream_start(GtkTextView *view)
{
    if (!view) return;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);

    if (gtk_text_buffer_get_char_count(buffer) > 0) {
        gtk_text_buffer_insert(buffer, &end, "\n", -1);
    }

    gtk_text_buffer_insert_with_tags(buffer, &end, "Lumila", -1, ai_name_tag, NULL);
    gtk_text_buffer_insert(buffer, &end, "\n", -1);

    stream_mark = gtk_text_buffer_create_mark(buffer, "stream_mark", &end, FALSE);
}

void lumila_chat_ui_stream_append(GtkTextView *view, const gchar *chunk)
{
    if (!view || !chunk || !stream_mark) return;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(buffer, &iter, stream_mark);
    gtk_text_buffer_insert_with_tags(buffer, &iter, chunk, -1, ai_tag, NULL);

    // Scroll to end
    GtkTextMark *mark = gtk_text_buffer_get_insert(buffer);
    gtk_text_view_scroll_to_mark(view, mark, 0.0, FALSE, 0.0, 0.0);
}

void lumila_chat_ui_stream_end(GtkTextView *view)
{
    if (!view) return;

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(view);
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    if (stream_mark) {
        gtk_text_buffer_delete_mark(buffer, stream_mark);
        stream_mark = NULL;
    }
    gtk_text_buffer_insert(buffer, &end, "\n", -1);
}
