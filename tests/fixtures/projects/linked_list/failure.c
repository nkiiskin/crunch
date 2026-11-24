#include <stddef.h>

struct Node { int value; struct Node *next; };
struct Snapshot { struct Node **nodes; int count; int padding; };

extern int insert_node(struct Node **link, int value);
extern int remove_node(struct Node **link);
extern void clear_list(struct Node **head);
extern int snapshot_list(struct Node *head, struct Snapshot *snapshot);
extern void clear_snapshot(struct Snapshot *snapshot);

extern void *__real___crunch_malloc(size_t size);
extern void __real___crunch_free(void *address);

static int fail_node_at;
static int node_calls;
static int node_frees;
static int bad_free;
static int fail_array;
static int array_frees;
static int snapshot_active;
static void *live[8];

void *__wrap___crunch_malloc(size_t size) {
    if (snapshot_active)
        return fail_array ? NULL : __real___crunch_malloc(size);
    node_calls++;
    if (node_calls == fail_node_at) return NULL;
    void *result = __real___crunch_malloc(size);
    if (result) {
        for (int i = 0; i < 8; i++) {
            if (!live[i]) {
                live[i] = result;
                return result;
            }
        }
        bad_free = 1;
    }
    return result;
}

void __wrap___crunch_free(void *address) {
    if (snapshot_active) {
        if (address) array_frees++;
        __real___crunch_free(address);
        return;
    }
    int found = 0;
    for (int i = 0; i < 8; i++) {
        if (address && live[i] == address) {
            live[i] = NULL;
            found = 1;
            break;
        }
    }
    if (!found) bad_free = 1;
    node_frees++;
    __real___crunch_free(address);
}

static int take_snapshot(struct Node *head, struct Snapshot *snapshot) {
    snapshot_active = 1;
    int result = snapshot_list(head, snapshot);
    snapshot_active = 0;
    return result;
}

int main(void) {
    struct Node *head = NULL;
    struct Snapshot snapshot = {NULL, 0, 123};
    fail_node_at = 1;
    if (insert_node(&head, 1) || head || node_calls != 1 || node_frees) return 1;

    node_calls = 0;
    fail_node_at = 3;
    if (!insert_node(&head, 1) || !insert_node(&head, 2)) return 2;
    struct Node *first = head;
    struct Node *second = head->next;
    if (insert_node(&head->next, 3) || head != first || head->next != second ||
        node_calls != 3 || node_frees) return 3;

    fail_array = 1;
    if (take_snapshot(head, &snapshot) || snapshot.nodes || snapshot.count ||
        snapshot.padding != 123 || node_frees || array_frees) return 4;
    fail_array = 0;
    if (!take_snapshot(head, &snapshot) || snapshot.count != 2 ||
        snapshot.nodes[0] != first || snapshot.nodes[1] != second) return 5;
    fail_node_at = node_calls + 1;
    if (insert_node(&head, 7) || head != first || snapshot.count != 2 ||
        snapshot.nodes[0] != first || node_frees) return 10;

    fail_node_at = 0;
    if (!insert_node(&head, 4)) return 6;
    fail_array = 1;
    if (take_snapshot(head, &snapshot) || snapshot.nodes || snapshot.count ||
        snapshot.padding != 123 || array_frees != 1 || node_frees ||
        head->next != first || first->next != second) return 7;
    clear_snapshot(&snapshot);
    clear_list(&head);
    if (head || node_frees != 3 || array_frees != 1 || bad_free ||
        remove_node(&head)) return 8;
    for (int i = 0; i < 8; i++)
        if (live[i]) return 9;
    return 0;
}
