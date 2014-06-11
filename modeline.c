// vim: expandtab:ts=8

#include "geanyplugin.h"

#define DEBUG_MODE 1

GeanyPlugin *geany_plugin;
GeanyData *geany_data;
GeanyFunctions *geany_functions;

PLUGIN_VERSION_CHECK(150);
PLUGIN_SET_INFO("Modeline", "Detect modelines for code formatting", "1.0",
                "Matt Hayes <nobomb@gmail.com>");

static void scan_document(GeanyDocument *doc);
static void parse_options(GeanyDocument *doc, gchar *buf);
static void interpret_option(GeanyDocument *doc, gchar *opt);
static void on_document_open(GObject *obj, GeanyDocument *doc, gpointer user_data);
static void on_document_save(GObject *obj, GeanyDocument *doc, gpointer user_data);

static void opt_expand_tab(GeanyDocument *doc, void *arg);
static void opt_tab_stop(GeanyDocument *doc, void *arg);
static void opt_wrap(GeanyDocument *doc, void *arg);

#ifdef DEBUG_MODE
#define debugf(fmt, ...) printf(fmt, ## __VA_ARGS__), fflush(stdout)
#else
#define debugf(fmt, ...)
#endif

/**< Hook into geany */
PluginCallback plugin_callbacks[] = {
        { "document-open", G_CALLBACK(on_document_open), FALSE, NULL },
        { "document-save", G_CALLBACK(on_document_save), FALSE, NULL },
        { NULL, NULL, FALSE, NULL }
};

/**
 * @brief Mode option types
 */
enum mode_opt_arg {
        MODE_OPT_ARG_INT, /**< Argument is an integer */
        MODE_OPT_ARG_TRUE, /**< No argument, true */
        MODE_OPT_ARG_FALSE, /**< No argument, false */
        MODE_OPT_ARG_STR, /**< String argument */
};

/**
 * @brief Mode option structure
 */
struct mode_opt {
        const gchar *name; /**< Full name of option */
        const gchar *alias; /**< Short alias of option */
        enum mode_opt_arg arg_type; /**< Argument type for option */
        void (*cb)(GeanyDocument *, void *); /**< */
};

/**< Define mode options, what type of argument it takes, and the callback */
static struct mode_opt opts[] = {
        { "expandtab",   "et", MODE_OPT_ARG_TRUE,  &opt_expand_tab },
        { "noexpandtab", NULL, MODE_OPT_ARG_FALSE, &opt_expand_tab },
        { "tabstop",     "ts", MODE_OPT_ARG_INT,   &opt_tab_stop },
        { "wrap",        NULL, MODE_OPT_ARG_TRUE,  &opt_wrap },
        { "nowrap",      NULL, MODE_OPT_ARG_FALSE, &opt_wrap },
        { NULL,          NULL, -1,                 NULL }
};

/**< These are prefixes we search for to determine what is a modeline */
static const gchar *mode_pre[] = {
        " geany:",
        " vi:",
        " vim:",
        " ex:",
        NULL
};

/**
 * @brief Whether or not to expand tabs to spaces
 *
 * @param doc Document
 * @param arg 1/0 (gint)
 */
static void opt_expand_tab(GeanyDocument *doc, void *arg)
{
        gint *iarg;

        iarg = arg;

        debugf("opt_expand_tab: %d\n", *iarg);

        editor_set_indent_type(doc->editor, (*iarg) ?
                               GEANY_INDENT_TYPE_SPACES :
                               GEANY_INDENT_TYPE_TABS);
}

/**
 * @brief This sets the indent/tab width
 *
 * @param doc Document
 * @param arg Indent/tab width (gint)
 */
static void opt_tab_stop(GeanyDocument *doc, void *arg)
{
        const GeanyIndentPrefs *prefs;
        gint *iarg;

        iarg = arg;
        prefs = editor_get_indent_prefs(doc->editor);

        debugf("opt_tab_stop: %d\n", *iarg);

        doc->editor->indent_width = *iarg;
        editor_set_indent_type(doc->editor, prefs->type);
}

/**
 * @brief Whether or not to wrap lines
 *
 * @param doc Document
 * @param arg 1/0 (gint)
 */
static void opt_wrap(GeanyDocument *doc, void *arg)
{
        gint *iarg;

        iarg = arg;

        debugf("opt_wrap: %d\n", *iarg);

        doc->editor->line_wrapping = *iarg;
        scintilla_send_message(doc->editor->sci, SCI_SETWRAPMODE,
                               (*iarg) ? SC_WRAP_WORD : SC_WRAP_NONE, 0);
}

/**
 * @brief Scan a document, line by line, looking for modelines.
 *
 * @param doc Document
 */
static void scan_document(GeanyDocument *doc)
{
        guint lines, line, i;
        gchar *buf, *ptr;

        if (!doc->is_valid)
                return;

        lines = sci_get_line_count(doc->editor->sci);
        for (line = 0; line < lines; line++) {
                buf = g_strstrip(sci_get_line(doc->editor->sci, line));

                for (i = 0; mode_pre[i] != NULL; i++) {
                        if ((ptr = g_strstr_len(buf, -1, mode_pre[i]))) {
                                parse_options(doc, buf);
                                return;
                        }
                }
        }
}

/**
 * @brief Parse out each key/value pair from a modeline, then send the pair out
 *        to the option interpreter.
 *
 * @param doc Document
 * @param buf Modeline
 */
static void parse_options(GeanyDocument *doc, gchar *buf)
{
        gchar **tok, **sp, *str;
        guint i, j;

        debugf("modeline [%s]\n", buf);

        tok = g_strsplit_set(buf, ": ", 0);
        for (i = 1; tok[i]; i++) {
                str = g_strstrip(tok[i]);

                // Not a 'set' list, throw the option to the option interpreter
                if (g_ascii_strncasecmp(str, "set", 3)) {
                        interpret_option(doc, str);
                        continue;
                }

                // This is a 'set' list, split by spaces!
                sp = g_strsplit(str, " ", 0);
                for (j = 1; sp[j]; j++) {
                        str = g_strstrip(sp[j]);
                        interpret_option(doc, str);
                }
                g_strfreev(sp);
        }
        g_strfreev(tok);
}

/**
 * @brief Interpret an option and set it.
 *
 * @param doc Document
 * @param opt Key/value pair
 */
static void interpret_option(GeanyDocument *doc, gchar *opt)
{
        gchar **kv, *key;
        guint i;
        gint iarg;

        debugf("interpret [%s]\n", opt);

        if (!(kv = g_strsplit(opt, "=", 0)))
                return;

        key = g_strstrip(kv[0]);
        for (i = 0; opts[i].name; i++) {
                if (g_ascii_strcasecmp(opts[i].name, key)) {
                        if (!opts[i].alias)
                                continue;
                        if (g_ascii_strcasecmp(opts[i].alias, key))
                                continue;
                }

                switch (opts[i].arg_type) {
                case MODE_OPT_ARG_TRUE:
                        iarg = 1;
                        break;
                case MODE_OPT_ARG_FALSE:
                        iarg = 0;
                        break;
                case MODE_OPT_ARG_INT:
                        if (!kv[1])
                                return;
                        iarg = g_ascii_strtoull(g_strstrip(kv[1]), NULL, 10);
                        break;
                case MODE_OPT_ARG_STR:
                        if (!kv[1])
                                return;
                        opts[i].cb(doc, kv[1]);
                        return;
                default:
                        return;
                }

                opts[i].cb(doc, &iarg);
                return;
        }
}

/**
 * @brief Document open hook
 *
 * @param obj
 * @param doc Document
 * @param user_data
 */
static void on_document_open(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
        debugf("on_document_open\n");
        scan_document(doc);
}

/**
 * @brief Document save hook
 *
 * @param obj
 * @param doc Document
 * @param user_data
 */
static void on_document_save(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
        debugf("on_document_save\n");
        scan_document(doc);
}

/**
 * @brief Plugin initialization
 *
 * @param
 */
void plugin_init(GeanyData *data)
{
}

/**
 * @brief Plugin cleanup
 */
void plugin_cleanup(void)
{
}
