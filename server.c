/* Exam Result Ranking System - C backend (tiny HTTP server + REST API)
 * DSA concepts: array of structs, merge sort, ranking with ties, binary search, file handling
 *
 * Windows (MinGW/Dev-C++/Code::Blocks): double-click build.bat   or   gcc server.c -o server.exe -lws2_32
 * Windows (Visual Studio): open "Developer Command Prompt" -> cl server.c
 * Linux / Mac:  gcc server.c -o server && ./server
 * Then open http://localhost:8080
 */
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
  #include <winsock2.h>
  #ifdef _MSC_VER
    #pragma comment(lib, "ws2_32.lib")
    #if _MSC_VER < 1900
      #define snprintf _snprintf
    #endif
  #endif
  typedef SOCKET sock_t;
  #define CLOSESOCK closesocket
  #define BAD_SOCK INVALID_SOCKET
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <unistd.h>
  typedef int sock_t;
  #define CLOSESOCK close
  #define BAD_SOCK (-1)
#endif

#define START_PORT 8080
#define MAX_STUDENTS 1000
#define SUBJECTS 5
#define DATA_FILE "students.txt"
#define BUF 16384

typedef struct {
    int roll;
    char name[50];
    int marks[SUBJECTS];
    int total;
    double pct;
    int rank;
} Student;

static Student db[MAX_STUDENTS];
static Student view[MAX_STUDENTS];
static Student tmp[MAX_STUDENTS];
static int n = 0;

/* ---------- file handling ---------- */
static void load_data(void) {
    FILE *f;
    Student s;
    int i;
    f = fopen(DATA_FILE, "r");
    if (!f) return;
    while (n < MAX_STUDENTS && fscanf(f, "%d,%49[^,],%d,%d,%d,%d,%d", &s.roll, s.name,
           &s.marks[0], &s.marks[1], &s.marks[2], &s.marks[3], &s.marks[4]) == 7) {
        s.total = 0;
        for (i = 0; i < SUBJECTS; i++) s.total += s.marks[i];
        s.pct = s.total / (double)SUBJECTS;
        s.rank = 0;
        db[n++] = s;
    }
    fclose(f);
}

static void save_data(void) {
    FILE *f;
    int i;
    f = fopen(DATA_FILE, "w");
    if (!f) return;
    for (i = 0; i < n; i++)
        fprintf(f, "%d,%s,%d,%d,%d,%d,%d\n", db[i].roll, db[i].name,
                db[i].marks[0], db[i].marks[1], db[i].marks[2], db[i].marks[3], db[i].marks[4]);
    fclose(f);
}

/* ---------- merge sort (O(n log n), stable) ---------- */
typedef int (*cmp_fn)(const Student *, const Student *);

static int cmp_rank(const Student *a, const Student *b) {
    if (a->total != b->total) return b->total - a->total;   /* higher total first */
    return a->roll - b->roll;                                /* tie -> smaller roll first */
}
static int cmp_roll(const Student *a, const Student *b) { return a->roll - b->roll; }
static int cmp_name(const Student *a, const Student *b) { return strcmp(a->name, b->name); }

static void merge_sort(Student *a, int l, int r, cmp_fn cmp) {
    int m, i, j, k;
    if (l >= r) return;
    m = (l + r) / 2; i = l; j = m + 1; k = l;
    merge_sort(a, l, m, cmp);
    merge_sort(a, m + 1, r, cmp);
    while (i <= m && j <= r) tmp[k++] = (cmp(&a[i], &a[j]) <= 0) ? a[i++] : a[j++];
    while (i <= m) tmp[k++] = a[i++];
    while (j <= r) tmp[k++] = a[j++];
    for (k = l; k <= r; k++) a[k] = tmp[k];
}

/* copy db -> view, assign ranks (equal totals share a rank), then sort for display */
static void build_view(const char *sort) {
    int i;
    if (n == 0) return;
    memcpy(view, db, sizeof(Student) * n);
    merge_sort(view, 0, n - 1, cmp_rank);
    for (i = 0; i < n; i++)
        view[i].rank = (i > 0 && view[i].total == view[i - 1].total) ? view[i - 1].rank : i + 1;
    if (strcmp(sort, "roll") == 0) merge_sort(view, 0, n - 1, cmp_roll);
    else if (strcmp(sort, "name") == 0) merge_sort(view, 0, n - 1, cmp_name);
}

/* ---------- binary search on roll-sorted view ---------- */
static int binary_search_roll(int roll, int *steps) {
    int lo = 0, hi = n - 1, mid;
    *steps = 0;
    while (lo <= hi) {
        mid = (lo + hi) / 2;
        (*steps)++;
        if (view[mid].roll == roll) return mid;
        if (view[mid].roll < roll) lo = mid + 1; else hi = mid - 1;
    }
    return -1;
}

/* ---------- helpers ---------- */
static int find_index(int roll) {
    int i;
    for (i = 0; i < n; i++) if (db[i].roll == roll) return i;
    return -1;
}

static void url_decode(char *dst, const char *src, size_t len) {
    size_t i, o = 0;
    char h[3];
    for (i = 0; i < len && src[i]; i++) {
        if (src[i] == '+') dst[o++] = ' ';
        else if (src[i] == '%' && i + 2 < len && isxdigit((unsigned char)src[i+1]) && isxdigit((unsigned char)src[i+2])) {
            h[0] = src[i+1]; h[1] = src[i+2]; h[2] = 0;
            dst[o++] = (char)strtol(h, NULL, 16);
            i += 2;
        } else dst[o++] = src[i];
    }
    dst[o] = 0;
}

/* read key from "a=1&b=2" style string */
static int get_param(const char *s, const char *key, char *out, size_t cap) {
    size_t kl = strlen(key), l;
    const char *p = s, *v;
    while (p && *p) {
        if (strncmp(p, key, kl) == 0 && p[kl] == '=') {
            v = p + kl + 1;
            l = strcspn(v, "&\r\n ");
            if (l >= cap) l = cap - 1;
            url_decode(out, v, l);
            return 1;
        }
        p = strchr(p, '&');
        if (p) p++;
    }
    out[0] = 0;
    return 0;
}

static int student_json(const Student *s, char *o, size_t cap) {
    return snprintf(o, cap,
        "{\"roll\":%d,\"name\":\"%s\",\"marks\":[%d,%d,%d,%d,%d],\"total\":%d,\"percentage\":%.2f,\"rank\":%d}",
        s->roll, s->name, s->marks[0], s->marks[1], s->marks[2], s->marks[3], s->marks[4],
        s->total, s->pct, s->rank);
}

/* ---------- HTTP output ---------- */
static void send_all(sock_t c, const char *d, size_t len) {
    size_t sent = 0;
    int r;
    while (sent < len) {
        r = send(c, d + sent, (int)(len - sent), 0);
        if (r <= 0) return;
        sent += r;
    }
}

static void respond(sock_t c, int code, const char *type, const char *body, size_t len) {
    const char *msg;
    char h[256];
    int hl;
    msg = code == 200 ? "OK" : code == 201 ? "Created" : code == 400 ? "Bad Request" :
          code == 404 ? "Not Found" : "Error";
    hl = snprintf(h, sizeof h,
        "HTTP/1.1 %d %s\r\nContent-Type: %s; charset=utf-8\r\nContent-Length: %lu\r\nConnection: close\r\n\r\n",
        code, msg, type, (unsigned long)len);
    send_all(c, h, hl);
    send_all(c, body, len);
}

static void json_msg(sock_t c, int code, const char *key, const char *text) {
    char b[200];
    int l = snprintf(b, sizeof b, "{\"%s\":\"%s\"}", key, text);
    respond(c, code, "application/json", b, l);
}

static void serve_static(sock_t c, const char *path) {
    char fp[300];
    FILE *f;
    long sz;
    char *data;
    size_t got;
    const char *type;
    if (strcmp(path, "/") == 0) path = "/index.html";
    if (strstr(path, "..") || strlen(path) > 200) { json_msg(c, 404, "error", "Not found"); return; }
    snprintf(fp, sizeof fp, "public%s", path);
    f = fopen(fp, "rb");
    if (!f) { json_msg(c, 404, "error", "Not found"); return; }
    fseek(f, 0, SEEK_END); sz = ftell(f); rewind(f);
    data = (char *)malloc(sz + 1);
    got = fread(data, 1, sz, f);
    fclose(f);
    type = strstr(fp, ".html") ? "text/html" : strstr(fp, ".css") ? "text/css" :
           strstr(fp, ".js") ? "application/javascript" : "application/octet-stream";
    respond(c, 200, type, data, got);
    free(data);
}

/* ---------- API ---------- */
static void api_list(sock_t c, const char *query) {
    char sort[16];
    char *out;
    size_t pos = 0;
    int i;
    get_param(query, "sort", sort, sizeof sort);
    build_view(sort);
    out = (char *)malloc((size_t)n * 260 + 8);
    out[pos++] = '[';
    for (i = 0; i < n; i++) {
        if (i) out[pos++] = ',';
        pos += student_json(&view[i], out + pos, 260);
    }
    out[pos++] = ']';
    respond(c, 200, "application/json", out, pos);
    free(out);
}

static void api_save(sock_t c, const char *params, int is_update) {
    char v[64], name[64], key[8];
    Student s;
    int o = 0, i, m, idx;
    char ch;
    memset(&s, 0, sizeof s);
    get_param(params, "roll", v, sizeof v);
    s.roll = atoi(v);
    if (s.roll <= 0) { json_msg(c, 400, "error", "Roll number must be a positive number"); return; }
    get_param(params, "name", name, sizeof name);
    for (i = 0; name[i] && o < 49; i++) {
        ch = name[i];
        if (ch == '"' || ch == '\\' || ch == ',' || (unsigned char)ch < 32) ch = ' ';
        if (ch == ' ' && (o == 0 || s.name[o - 1] == ' ')) continue;
        s.name[o++] = ch;
    }
    while (o > 0 && s.name[o - 1] == ' ') o--;
    s.name[o] = 0;
    if (o == 0) { json_msg(c, 400, "error", "Name is required"); return; }
    for (i = 0; i < SUBJECTS; i++) {
        snprintf(key, sizeof key, "m%d", i + 1);
        if (!get_param(params, key, v, sizeof v) || v[0] == 0) { json_msg(c, 400, "error", "All marks are required"); return; }
        m = atoi(v);
        if (m < 0 || m > 100) { json_msg(c, 400, "error", "Marks must be between 0 and 100"); return; }
        s.marks[i] = m;
        s.total += m;
    }
    s.pct = s.total / (double)SUBJECTS;
    idx = find_index(s.roll);
    if (is_update) {
        if (idx < 0) { json_msg(c, 404, "error", "Student not found"); return; }
        db[idx] = s;
    } else {
        if (idx >= 0) { json_msg(c, 400, "error", "Roll number already exists"); return; }
        if (n >= MAX_STUDENTS) { json_msg(c, 400, "error", "Database full"); return; }
        db[n++] = s;
    }
    save_data();
    json_msg(c, is_update ? 200 : 201, "ok", "saved");
}

static void api_delete(sock_t c, const char *query) {
    char v[32];
    int idx, i;
    get_param(query, "roll", v, sizeof v);
    idx = find_index(atoi(v));
    if (idx < 0) { json_msg(c, 404, "error", "Student not found"); return; }
    for (i = idx; i < n - 1; i++) db[i] = db[i + 1];
    n--;
    save_data();
    json_msg(c, 200, "ok", "deleted");
}

static void api_search(sock_t c, const char *query) {
    char v[32], out[512];
    int steps = 0, idx, l;
    get_param(query, "roll", v, sizeof v);
    build_view("roll");                      /* sorted by roll -> binary search is valid */
    idx = binary_search_roll(atoi(v), &steps);
    if (idx < 0) l = snprintf(out, sizeof out, "{\"found\":false,\"steps\":%d}", steps);
    else {
        l = snprintf(out, sizeof out, "{\"found\":true,\"steps\":%d,\"student\":", steps);
        l += student_json(&view[idx], out + l, sizeof out - l);
        out[l++] = '}'; out[l] = 0;
    }
    respond(c, 200, "application/json", out, l);
}

/* ---------- request handling ---------- */
static void handle(sock_t c) {
    static char req[BUF + 1];
    int len = 0, hdr_end = -1, r, need;
    char *he, *cl, *query;
    char method[8], url[256];
    const char *body;
    while (len < BUF) {
        r = recv(c, req + len, BUF - len, 0);
        if (r <= 0) break;
        len += r; req[len] = 0;
        he = strstr(req, "\r\n\r\n");
        if (he) {
            hdr_end = (int)(he - req) + 4;
            cl = strstr(req, "Content-Length:");
            if (!cl) cl = strstr(req, "content-length:");
            need = cl ? atoi(cl + 15) : 0;
            if (len >= hdr_end + need) break;
        }
    }
    if (hdr_end < 0) return;
    req[len] = 0;
    if (sscanf(req, "%7s %255s", method, url) != 2) return;
    query = strchr(url, '?');
    if (query) *query++ = 0; else query = url + strlen(url);
    body = req + hdr_end;

    if (strcmp(url, "/api/students") == 0) {
        if (!strcmp(method, "GET")) api_list(c, query);
        else if (!strcmp(method, "POST")) api_save(c, body, 0);
        else if (!strcmp(method, "PUT")) api_save(c, body, 1);
        else if (!strcmp(method, "DELETE")) api_delete(c, query);
        else json_msg(c, 400, "error", "Method not allowed");
    } else if (strcmp(url, "/api/search") == 0 && !strcmp(method, "GET")) {
        api_search(c, query);
    } else if (!strcmp(method, "GET")) {
        serve_static(c, url);
    } else json_msg(c, 404, "error", "Not found");
}

int main(void) {
    sock_t srv, c;
    struct sockaddr_in addr;
    int opt = 1, port, bound = 0;
    FILE *chk;
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    chk = fopen("public/index.html", "r");
    if (!chk) {
        printf("ERROR: 'public/index.html' not found.\n"
               "Run this program from inside the project folder (the one that contains the 'public' folder).\n");
        return 1;
    }
    fclose(chk);
    load_data();

    srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv == BAD_SOCK) { printf("ERROR: could not create socket.\n"); return 1; }
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof opt);
    for (port = START_PORT; port < START_PORT + 10 && !bound; port++) {   /* try 8080..8089 */
        memset(&addr, 0, sizeof addr);
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons((unsigned short)port);
        if (bind(srv, (struct sockaddr *)&addr, sizeof addr) == 0) bound = 1;
    }
    port--;
    if (!bound || listen(srv, 16) != 0) { printf("ERROR: could not start server (ports 8080-8089 busy).\n"); return 1; }
    printf("Exam Result Ranking System is running (%d students loaded)\n", n);
    printf("Open this in your browser:  http://localhost:%d\n", port);
    printf("Press Ctrl+C to stop.\n");
    for (;;) {
        c = accept(srv, NULL, NULL);
        if (c == BAD_SOCK) continue;
        handle(c);
        CLOSESOCK(c);
    }
    return 0;
}
