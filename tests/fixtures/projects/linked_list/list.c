#include "list.h"
#include <stdlib.h>

int insert_node(struct Node** link, int value) {
    struct Node* node = malloc(sizeof(struct Node));
    if (!node) return 0;
    node->value = value;
    node->next = *link;
    *link = node;
    return 1;
}

int remove_node(struct Node** link) {
    struct Node* node = *link;
    if (!node) return 0;
    *link = node->next;
    free(node);
    return 1;
}

void clear_list(struct Node** head) {
    while (*head) {
        remove_node(head);
    }
}

int snapshot_list(struct Node* head, struct Snapshot* snapshot) {
    int count = 0;
    struct Node* node = head;
    while (node) {
        if (count == 2147483647) return 0;
        count++;
        node = node->next;
    }
    clear_snapshot(snapshot);
    if (count == 0) return 1;
    snapshot->nodes = malloc((long)count * sizeof(struct Node*));
    if (!snapshot->nodes) return 0;
    snapshot->count = count;
    node = head;
    int index = 0;
    while (node) {
        snapshot->nodes[index] = node;
        index++;
        node = node->next;
    }
    return 1;
}

void clear_snapshot(struct Snapshot* snapshot) {
    if (snapshot->nodes) free(snapshot->nodes);
    snapshot->nodes = 0;
    snapshot->count = 0;
}
