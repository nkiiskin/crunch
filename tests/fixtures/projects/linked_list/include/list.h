#ifndef LINKED_LIST_H
#define LINKED_LIST_H

struct Node { int value; struct Node* next; };
struct Snapshot { struct Node** nodes; int count; };

int insert_node(struct Node** link, int value);
int remove_node(struct Node** link);
void clear_list(struct Node** head);
int snapshot_list(struct Node* head, struct Snapshot* snapshot);
void clear_snapshot(struct Snapshot* snapshot);

#endif
