#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

typedef uint8_t u8;
typedef uint32_t u32;

typedef struct TNode {
	struct TNode *LNode, *RNode;
	u8 port;
} TNode, *Tree;

typedef struct TNode_pro {
	struct TNode_pro *LLNode, *LRNode, *RLNode, *RRNode;
	u8 port;
} TNode_pro, *Tree_pro;

typedef struct TNode_comp {
	u8 type[4];
	u8 port;
	struct TNode_comp *ptr_0, *ptr_1;
} TNode_comp, *Tree_comp;

typedef struct ip_info {
	u32 ip;
	u8 length;
	u8 port;
} ip_info;

#define i_to_node(tr, i) ((i == 0)? tr->LLNode: \
						 (i == 1)? tr->LRNode: \
						 (i == 2)? tr->RLNode:tr->RRNode)

#define TREE_TYPE 0
char *tree_s = (TREE_TYPE == 0)? "pref_tree" : \
		   (TREE_TYPE == 1)? "pref_tree_pro" : "pref_tree_comp";

static inline void init_node_pro(TNode_pro *node_tmp, u8 port) {
	node_tmp->LLNode = node_tmp->LRNode = NULL;
	node_tmp->RLNode = node_tmp->RRNode = NULL;
	node_tmp->port = port;
}

static inline void print_result(u8 port, ip_info *ii) {
	printf("For ip %u.%u.%u.%u, res_port = %u, result of %s is wrong.\n", \
		ii->ip>>24, (ii->ip>>16)&0xff, (ii->ip>>8)&0xff, ii->ip&0xff, port, tree_s);
}

char *filename = "forwarding-table.txt";
char *testname = "test-table.txt";
