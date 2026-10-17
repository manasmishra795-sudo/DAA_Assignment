/*
 * 0/1 KNAPSACK  -  Dynamic Programming with a LIVE ASCII visualization
 * ---------------------------------------------------------------------
 * Input file format (test cases separated by lines starting with "#Test"):
 *
 *      #Test
 *      n W              <- number of items, knapsack capacity
 *      w1 v1            <- weight and value of item 1
 *      ...
 *      wn vn
 *
 * What the program does
 *   1. Reads every test case from the input file.
 *   2. Solves each one with the 0/1 knapsack DP and draws the DP table
 *      live in the terminal, one cell at a time (then the traceback).
 *   3. Prints the result of each test case.
 *   4. Saves every result in ONE file          :  result.txt
 *      and ALSO one folder per test case       :  test_1/result.txt
 *                                                 test_2/result.txt ...
 *
 * Result line format:   #Test <case no> input (<n> <W>) result <max value>
 *
 * Build :  gcc -O2 -Wall -o knapsack knapsack.c
 * Run   :  ./knapsack [input_file] [delay_ms]
 *          defaults:   knapsack_data.txt   40
 *          delay_ms = 0 shows the table without animation delay.
 */

#if !defined(_WIN32)
#  ifndef _DEFAULT_SOURCE
#    define _DEFAULT_SOURCE 1
#  endif
#  ifndef _POSIX_C_SOURCE
#    define _POSIX_C_SOURCE 200809L
#  endif
#  ifndef _DARWIN_C_SOURCE
#    define _DARWIN_C_SOURCE 1
#  endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>

#ifdef _WIN32
#  include <windows.h>
#  include <direct.h>
#  define MKDIR(p) _mkdir(p)
static void sleep_ms(int ms) { Sleep((DWORD)ms); }
static void enable_ansi(void)
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode))
        SetConsoleMode(h, mode | 0x0004); /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */
}
static int term_cols(void)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
        return info.srWindow.Right - info.srWindow.Left + 1;
    return 120;
}
#else
#  include <time.h>
#  include <unistd.h>
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <sys/ioctl.h>
#  define MKDIR(p) mkdir((p), 0755)
static void sleep_ms(int ms)
{
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}
static void enable_ansi(void) { }
static int term_cols(void)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return ws.ws_col;
    return 120;
}
#endif

/* ---------- ANSI colours ---------- */
#define C_RESET "\033[0m"
#define C_BOLD  "\033[1m"
#define C_DIM   "\033[90m"
#define C_CUR   "\033[30;43m"   /* black on yellow  : cell being computed   */
#define C_A     "\033[30;46m"   /* black on cyan    : "skip" source cell    */
#define C_B     "\033[30;45m"   /* black on magenta : "take" source cell    */
#define C_PATH  "\033[30;42m"   /* black on green   : optimal path          */

#define LABEL_W 22              /* width of the row-label column            */

/* ---------- data types ---------- */
typedef struct {
    int  n, W;
    int *wt, *val;
} TestCase;

typedef struct { char l[4][240]; } Msg;      /* 4 message lines under table */

typedef struct {
    const TestCase *tc;
    int   tno;
    int **dp;
    char *st;      /* (n+1)*(W+1): 0 = empty, 1 = computed, 2 = on path     */
    int   numw;    /* digits needed for the biggest possible value          */
    int   c0;      /* first visible column (horizontal scrolling)           */
} View;

/* =====================================================================
 *  Input
 * ===================================================================== */

/* read next non-blank line; returns 1 on success */
static int next_data_line(FILE *fp, char *buf, size_t size)
{
    while (fgets(buf, (int)size, fp)) {
        const char *p = buf;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p) return 1;
    }
    return 0;
}

static TestCase *read_tests(const char *fname, int *count)
{
    FILE *fp = fopen(fname, "r");
    if (!fp) { fprintf(stderr, "Cannot open '%s': %s\n", fname, strerror(errno)); return NULL; }

    TestCase *tests = NULL;
    int cnt = 0, cap = 0;
    char line[512];

    while (fgets(line, sizeof line, fp)) {
        if (!strstr(line, "#Test")) continue;          /* separator line   */

        int n, W;
        if (!next_data_line(fp, line, sizeof line) || sscanf(line, "%d %d", &n, &W) != 2
            || n < 0 || W < 0) {
            fprintf(stderr, "Test %d: bad 'n W' line.\n", cnt + 1);
            break;
        }

        TestCase tc;
        tc.n = n; tc.W = W;
        tc.wt  = (int *)malloc((size_t)(n > 0 ? n : 1) * sizeof(int));
        tc.val = (int *)malloc((size_t)(n > 0 ? n : 1) * sizeof(int));
        if (!tc.wt || !tc.val) { fprintf(stderr, "Out of memory\n"); exit(1); }

        int ok = 1;
        for (int i = 0; i < n; i++) {
            if (!next_data_line(fp, line, sizeof line)
                || sscanf(line, "%d %d", &tc.wt[i], &tc.val[i]) != 2
                || tc.wt[i] < 0 || tc.val[i] < 0) {
                fprintf(stderr, "Test %d: bad item line %d.\n", cnt + 1, i + 1);
                ok = 0;
                break;
            }
        }
        if (!ok) { free(tc.wt); free(tc.val); break; }

        if (cnt == cap) {
            cap = cap ? cap * 2 : 8;
            tests = (TestCase *)realloc(tests, (size_t)cap * sizeof(TestCase));
            if (!tests) { fprintf(stderr, "Out of memory\n"); exit(1); }
        }
        tests[cnt++] = tc;
    }
    fclose(fp);
    *count = cnt;
    return tests;
}

/* =====================================================================
 *  Live ASCII rendering
 * ===================================================================== */

static int digits(long v)
{
    int d = 1;
    while (v >= 10) { v /= 10; d++; }
    return d;
}

/*
 * Redraws the whole DP table in place (cursor-home, no flicker).
 * cur = cell being computed, a = "skip" source, b = "take" source
 * (-1 = none).  If the table is wider than the terminal, a window of
 * columns that follows the current cell is shown.
 */
static void render(View *v, int cur_i, int cur_w,
                   int a_i, int a_w, int b_i, int b_w, const Msg *m)
{
    const TestCase *tc = v->tc;
    int n = tc->n, W = tc->W;
    int total = W + 1;
    int cellw = v->numw + 1;

    int maxcols = (term_cols() - LABEL_W - 2) / cellw;
    if (maxcols < 4) maxcols = 4;
    int shown = total < maxcols ? total : maxcols;

    if (cur_w >= 0) {
        if (cur_w < v->c0 || cur_w >= v->c0 + shown) v->c0 = cur_w - shown / 2;
    } else {
        v->c0 = total - shown;                 /* final view: show dp[n][W] */
    }
    if (v->c0 > total - shown) v->c0 = total - shown;
    if (v->c0 < 0) v->c0 = 0;

    printf("\033[H");
    printf(C_BOLD "0/1 KNAPSACK  -  Test %d   (n = %d, W = %d)" C_RESET "\033[K\n",
           v->tno, n, W);
    printf("Legend: " C_CUR " now " C_RESET " " C_A " A " C_RESET "=skip dp[i-1][w]  "
           C_B " B " C_RESET "=take v+dp[i-1][w-wt]  " C_PATH " path " C_RESET "\033[K\n");
    if (shown < total)
        printf(C_DIM "Showing columns %d-%d of 0-%d (widen the terminal to see all)"
               C_RESET "\033[K\n", v->c0, v->c0 + shown - 1, W);
    else
        printf("\033[K\n");

    /* header row: capacities */
    printf("%-*s|", LABEL_W, "        capacity w ->");
    for (int w = v->c0; w < v->c0 + shown; w++) printf(C_BOLD " %*d" C_RESET, v->numw, w);
    printf("\033[K\n");

    /* table rows */
    for (int i = 0; i <= n; i++) {
        char label[64];
        char mark = (i == cur_i) ? '>' : ' ';
        if (i == 0) snprintf(label, sizeof label, "%c i=0 (no items)", mark);
        else        snprintf(label, sizeof label, "%c i=%d (w=%d, v=%d)", mark, i,
                             tc->wt[i - 1], tc->val[i - 1]);
        printf("%-*s|", LABEL_W, label);

        for (int w = v->c0; w < v->c0 + shown; w++) {
            char s = v->st[i * total + w];
            const char *col = "";
            if      (i == cur_i && w == cur_w) col = C_CUR;
            else if (i == a_i   && w == a_w)   col = C_A;
            else if (i == b_i   && w == b_w)   col = C_B;
            else if (s == 2)                   col = C_PATH;
            else if (s == 0)                   col = C_DIM;

            if (s == 0) printf("%s %*s" C_RESET, col, v->numw, ".");
            else        printf("%s %*d" C_RESET, col, v->numw, v->dp[i][w]);
        }
        printf("\033[K\n");
    }

    for (int k = 0; k < 4; k++) printf("%s\033[K\n", m->l[k]);
    printf("\033[J");
    fflush(stdout);
}

/* =====================================================================
 *  Solve one test case (DP + visualization + files)
 * ===================================================================== */

static void make_line(char *buf, size_t size, int tno, const TestCase *tc, int best)
{
    snprintf(buf, size, "#Test %d input (%d %d) result %d", tno, tc->n, tc->W, best);
}

static void make_dir(const char *dir)
{
    if (MKDIR(dir) != 0 && errno != EEXIST)
        fprintf(stderr, "Cannot create folder '%s': %s\n", dir, strerror(errno));
}

static int process_test(const TestCase *tc, int tno, int delay_ms,
                        FILE *all, char *line_out, size_t line_size)
{
    int n = tc->n, W = tc->W, total = W + 1;

    /* ----- allocate table ----- */
    int **dp = (int **)malloc((size_t)(n + 1) * sizeof(int *));
    char *st = (char *)calloc((size_t)(n + 1) * (size_t)total, 1);
    if (!dp || !st) { fprintf(stderr, "Out of memory\n"); exit(1); }
    for (int i = 0; i <= n; i++) {
        dp[i] = (int *)calloc((size_t)total, sizeof(int));
        if (!dp[i]) { fprintf(stderr, "Out of memory\n"); exit(1); }
    }

    long sumv = 0;
    for (int i = 0; i < n; i++) sumv += tc->val[i];

    View v;
    v.tc = tc; v.tno = tno; v.dp = dp; v.st = st; v.c0 = 0;
    v.numw = digits(sumv);

    Msg m;

    /* ----- base cases: dp[0][w] = 0 and dp[i][0] = 0 ----- */
    for (int w = 0; w <= W; w++) st[w] = 1;
    for (int i = 0; i <= n; i++) st[i * total] = 1;

    memset(&m, 0, sizeof m);
    snprintf(m.l[0], sizeof m.l[0], "Base cases: dp[0][w] = 0 (no items)  and  dp[i][0] = 0 (no capacity)");
    snprintf(m.l[1], sizeof m.l[1], "Recurrence: dp[i][w] = max( dp[i-1][w],  v_i + dp[i-1][w - w_i] )  if w_i <= w");
    printf("\033[2J\033[H");
    render(&v, -1, 0, -1, -1, -1, -1, &m);
    if (delay_ms) sleep_ms(delay_ms * 10);

    /* ----- fill the table, one cell at a time ----- */
    for (int i = 1; i <= n; i++) {
        int wi = tc->wt[i - 1], vi = tc->val[i - 1];
        for (int w = 1; w <= W; w++) {
            int skip = dp[i - 1][w];
            int bi = -1, bw = -1;
            memset(&m, 0, sizeof m);
            snprintf(m.l[0], sizeof m.l[0],
                     "Item %d: weight = %d, value = %d   ->   filling dp[%d][%d]  (capacity %d)",
                     i, wi, vi, i, w, w);

            if (wi > w) {
                dp[i][w] = skip;
                snprintf(m.l[1], sizeof m.l[1],
                         "weight %d > capacity %d : item does NOT fit", wi, w);
                snprintf(m.l[2], sizeof m.l[2],
                         "dp[%d][%d] = dp[%d][%d] = %d", i, w, i - 1, w, skip);
            } else {
                int take = vi + dp[i - 1][w - wi];
                bi = i - 1; bw = w - wi;
                dp[i][w] = take > skip ? take : skip;
                snprintf(m.l[1], sizeof m.l[1],
                         "A  skip : dp[%d][%d] = %d", i - 1, w, skip);
                snprintf(m.l[2], sizeof m.l[2],
                         "B  take : %d + dp[%d][%d] = %d + %d = %d",
                         vi, i - 1, w - wi, vi, dp[i - 1][w - wi], take);
                snprintf(m.l[3], sizeof m.l[3],
                         "dp[%d][%d] = max(%d, %d) = %d   ->  %s",
                         i, w, skip, take, dp[i][w], take > skip ? "TAKE item" : "SKIP item");
            }
            st[i * total + w] = 1;
            render(&v, i, w, i - 1, w, bi, bw, &m);
            if (delay_ms) sleep_ms(delay_ms);
        }
    }

    int best = dp[n][W];

    /* ----- traceback: which items were taken ----- */
    char *taken = (char *)calloc((size_t)n + 1, 1);
    if (!taken) { fprintf(stderr, "Out of memory\n"); exit(1); }
    int tw = W, total_wt = 0;
    for (int i = n; i >= 1; i--) {
        int wi = tc->wt[i - 1], vi = tc->val[i - 1];
        st[i * total + tw] = 2;

        memset(&m, 0, sizeof m);
        snprintf(m.l[0], sizeof m.l[0],
                 "TRACEBACK from dp[%d][%d] = %d  (walk back to row 0)", n, W, best);
        snprintf(m.l[1], sizeof m.l[1],
                 "dp[%d][%d] = %d   vs   dp[%d][%d] = %d", i, tw, dp[i][tw], i - 1, tw, dp[i - 1][tw]);
        if (dp[i][tw] != dp[i - 1][tw]) {
            taken[i] = 1;
            total_wt += wi;
            snprintf(m.l[2], sizeof m.l[2],
                     "different -> item %d (w=%d, v=%d) was TAKEN; go to dp[%d][%d]",
                     i, wi, vi, i - 1, tw - wi);
            render(&v, i, tw, -1, -1, -1, -1, &m);
            tw -= wi;
        } else {
            snprintf(m.l[2], sizeof m.l[2],
                     "same -> item %d was NOT taken; go to dp[%d][%d]", i, i - 1, tw);
            render(&v, i, tw, -1, -1, -1, -1, &m);
        }
        if (delay_ms) sleep_ms(delay_ms * 4);
    }
    st[0 * total + tw] = 2;

    /* ----- result line ----- */
    make_line(line_out, line_size, tno, tc, best);

    /* ----- final frame ----- */
    {
        char items[200] = "";
        size_t len = 0;
        for (int i = 1; i <= n; i++)
            if (taken[i] && len < sizeof items - 24)
                len += (size_t)snprintf(items + len, sizeof items - len, "#%d(w=%d,v=%d) ",
                                        i, tc->wt[i - 1], tc->val[i - 1]);
        memset(&m, 0, sizeof m);
        snprintf(m.l[0], sizeof m.l[0], "Items taken : %s", len ? items : "(none)");
        snprintf(m.l[1], sizeof m.l[1], "Total weight = %d  (capacity %d)    Total value = %d",
                 total_wt, W, best);
        snprintf(m.l[2], sizeof m.l[2], "%sRESULT: %s%s", C_BOLD, line_out, C_RESET);
        render(&v, -1, -1, -1, -1, -1, -1, &m);
        if (delay_ms) sleep_ms(delay_ms * 30);
    }

    /* ----- save: combined file + this test's own folder ----- */
    fprintf(all, "%s\n", line_out);
    fflush(all);

    char dir[64], path[128];
    snprintf(dir, sizeof dir, "test_%d", tno);
    make_dir(dir);
    snprintf(path, sizeof path, "%s/result.txt", dir);
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "Cannot write '%s': %s\n", path, strerror(errno));
    } else {
        fprintf(f, "%s\n\n", line_out);
        fprintf(f, "Items taken (weight, value):");
        for (int i = 1; i <= n; i++)
            if (taken[i]) fprintf(f, " #%d(%d,%d)", i, tc->wt[i - 1], tc->val[i - 1]);
        fprintf(f, "\nTotal weight = %d / %d\n\n", total_wt, W);

        fprintf(f, "Final DP table (rows = items, columns = capacity 0..%d):\n", W);
        fprintf(f, "%-18s|", "w ->");
        for (int w = 0; w <= W; w++) fprintf(f, " %*d", v.numw, w);
        fprintf(f, "\n");
        for (int i = 0; i <= n; i++) {
            char label[64];
            if (i == 0) snprintf(label, sizeof label, "i=0 (no items)");
            else snprintf(label, sizeof label, "i=%d (w=%d, v=%d)", i, tc->wt[i - 1], tc->val[i - 1]);
            fprintf(f, "%-18s|", label);
            for (int w = 0; w <= W; w++) fprintf(f, " %*d", v.numw, dp[i][w]);
            fprintf(f, "\n");
        }
        fclose(f);
    }

    for (int i = 0; i <= n; i++) free(dp[i]);
    free(dp); free(st); free(taken);
    return best;
}

/* =====================================================================
 *  main
 * ===================================================================== */

static void restore_terminal(void)
{
    fputs("\033[?25h" C_RESET, stdout);          /* show cursor again */
    fflush(stdout);
}

static void on_sigint(int sig)
{
    (void)sig;
    restore_terminal();
    fputs("\n", stdout);
    exit(130);
}

int main(int argc, char **argv)
{
    const char *in_file = (argc > 1) ? argv[1] : "knapsack_data.txt";
    int delay_ms = (argc > 2) ? atoi(argv[2]) : 40;
    if (delay_ms < 0) delay_ms = 0;

    int count = 0;
    TestCase *tests = read_tests(in_file, &count);
    if (!tests || count == 0) {
        fprintf(stderr, "No test cases found in '%s'.\n", in_file);
        return 1;
    }

    FILE *all = fopen("result.txt", "w");
    if (!all) { fprintf(stderr, "Cannot create result.txt: %s\n", strerror(errno)); return 1; }

    enable_ansi();
    signal(SIGINT, on_sigint);
    atexit(restore_terminal);
    fputs("\033[?25l", stdout);                  /* hide cursor while animating */

    char (*lines)[160] = (char (*)[160])malloc((size_t)count * 160);
    if (!lines) { fprintf(stderr, "Out of memory\n"); return 1; }

    for (int t = 0; t < count; t++)
        process_test(&tests[t], t + 1, delay_ms, all, lines[t], 160);

    fclose(all);
    restore_terminal();

    printf("\nAll %d test cases solved. Results:\n\n", count);
    for (int t = 0; t < count; t++) printf("%s\n", lines[t]);
    printf("\nSaved in: result.txt  and  test_1/result.txt ... test_%d/result.txt\n", count);

    for (int t = 0; t < count; t++) { free(tests[t].wt); free(tests[t].val); }
    free(tests); free(lines);
    return 0;
}
