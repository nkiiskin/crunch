#include "list.h"

int main() {
    struct Node* head = 0;
    struct Snapshot snapshot;
    snapshot.nodes = 0;
    snapshot.count = 0;
    if (remove_node(&head) || !snapshot_list(head, &snapshot) ||
        snapshot.count != 0) return 1;
    if (!insert_node(&head, 10) || !insert_node(&head, 20) ||
        !insert_node(&head, 30)) {
        clear_list(&head);
        return 2;
    }
    if (!snapshot_list(head, &snapshot) || snapshot.count != 3 ||
        snapshot.nodes[0]->value != 30 || snapshot.nodes[1]->value != 20 ||
        snapshot.nodes[2]->value != 10) {
        clear_snapshot(&snapshot);
        clear_list(&head);
        return 3;
    }
    if (!insert_node(&head->next, 25) ||
        !insert_node(&head->next->next->next->next, 5)) {
        clear_snapshot(&snapshot);
        clear_list(&head);
        return 4;
    }
    if (!snapshot_list(head, &snapshot) || snapshot.count != 5 ||
        snapshot.nodes[1]->value != 25 || snapshot.nodes[2]->value != 20 ||
        snapshot.nodes[4]->value != 5) {
        clear_snapshot(&snapshot);
        clear_list(&head);
        return 5;
    }
    if (!remove_node(&head->next) || !snapshot_list(head, &snapshot) ||
        snapshot.count != 4 || snapshot.nodes[1]->value != 20 ||
        snapshot.nodes[3]->value != 5) {
        clear_snapshot(&snapshot);
        clear_list(&head);
        return 6;
    }
    clear_snapshot(&snapshot);
    if (!remove_node(&head) || head->value != 20 ||
        !remove_node(&head->next->next) || !remove_node(&head->next) ||
        head->next != 0) {
        clear_list(&head);
        return 7;
    }
    clear_list(&head);
    if (head != 0 || remove_node(&head)) return 8;
    return 0;
}
