#include "pref-tree.h"

void get_info_from_string(char* s, ip_info *ii) {
	ii->ip = 0;
	u8 ip_tmp[4];
	sscanf(s, "%hhu.%hhu.%hhu.%hhu", &ip_tmp[3], &ip_tmp[2], &ip_tmp[1], &ip_tmp[0]);
	ii->ip = ip_tmp[3];
	ii->ip = (ii->ip << 8) | ip_tmp[2];
	ii->ip = (ii->ip << 8) | ip_tmp[1];
	ii->ip = (ii->ip << 8) | ip_tmp[0];
	sscanf(s, "%*s%hhu", &ii->length);
	sscanf(s, "%*s%*s%hhu", &ii->port);
}

// To build the basic prefix tree.
void build_pref_tree(Tree tr) {
	tr->port = 255;
	FILE *fp = fopen(filename, "r");
	u32 ip1 = 1 << 31;
	TNode *node = NULL;
	ip_info *ii = (ip_info*)malloc(sizeof(ip_info));
	char s[25];

	while (!feof(fp)) {
		fgets(s, 25, fp);
		get_info_from_string(s, ii);
		node = tr;
		for (int j = 0; j < ii->length; j++) {
			if (ii->ip & (ip1 >> j)) {
				if (!node->RNode) {
					TNode *node_tmp = (TNode*)malloc(sizeof(TNode));
					node_tmp->LNode = node_tmp->RNode = NULL;
					node_tmp->port = node->port;
					node->RNode = node_tmp;
				}
				node = node->RNode;
			} else {
				if (!node->LNode) {
					TNode *node_tmp = (TNode*)malloc(sizeof(TNode));
					node_tmp->LNode = node_tmp->RNode = NULL;
					node_tmp->port = node->port;
					node->LNode = node_tmp;
				}
				node = node->LNode;
			}
		}
		node->port = ii->port;
	}
	fclose(fp);
}

// To look up ip in the basic prefix tree.
u8 lookup_pref_tree(Tree tr, u32 ip) {
	u32 ip1 = 1 << 31;
	TNode *node = tr;
	TNode *node_tmp = NULL;
	for (int i = 0; node; i++) {
		node_tmp = node;
		node = (ip & (ip1 >> i)) ? node->RNode : node->LNode;
	}
	return node_tmp->port;
}

// To count nodes in the basic prefix tree.
void count_nodes(Tree tr, int* nodes) {
	if (tr->LNode) count_nodes(tr->LNode, nodes);
	if (tr->RNode) count_nodes(tr->RNode, nodes);
	*nodes = *nodes + 1;
}

// To achieve leaf pushing for two-bit prefix tree
void leaf_push(Tree_pro tr) {
	if (!tr->RRNode) {
		TNode_pro *node_tmp = (TNode_pro*)malloc(sizeof(TNode_pro));
		init_node_pro(node_tmp, tr->port);
		tr->RRNode = node_tmp;
	} else {
		leaf_push(tr->RRNode);
	}
	if (!tr->RLNode) {
		TNode_pro *node_tmp = (TNode_pro*)malloc(sizeof(TNode_pro));
		init_node_pro(node_tmp, tr->port);
		tr->RLNode = node_tmp;
	} else {
		leaf_push(tr->RLNode);
	}
	if (!tr->LRNode) {
		TNode_pro *node_tmp = (TNode_pro*)malloc(sizeof(TNode_pro));
		init_node_pro(node_tmp, tr->port);
		tr->LRNode = node_tmp;
	} else {
		leaf_push(tr->LRNode);
	}
	if (!tr->LLNode) {
		TNode_pro *node_tmp = (TNode_pro*)malloc(sizeof(TNode_pro));
		init_node_pro(node_tmp, tr->port);
		tr->LLNode = node_tmp;
	} else {
		leaf_push(tr->LLNode);
	}
}

// To build the two-bit prefix tree with leaf pushing.
void build_pref_tree_pro(Tree_pro tr) {
	tr->port = 255;
	FILE *fp = fopen(filename, "r");
	u32 ip1 = 1 << 31;
	TNode_pro *node = NULL;
	ip_info *ii = (ip_info*)malloc(sizeof(ip_info));
	char s[25];

	while (!feof(fp)) {
		fgets(s, 25, fp);
		get_info_from_string(s, ii);
		node = tr;
		for (int j = 0; j < ii->length-1; j ++) {
			if (ii->ip & (ip1 >> j)) {
				if (ii->ip & (ip1 >> ++j)) {
					if (!node->RRNode) {
						TNode_pro *node_tmp = (TNode_pro*)malloc(sizeof(TNode_pro));
						init_node_pro(node_tmp, node->port);
						node->RRNode = node_tmp;
					}
					node = node->RRNode;
				} else {
					if (!node->RLNode) {
						TNode_pro *node_tmp = (TNode_pro*)malloc(sizeof(TNode_pro));
						init_node_pro(node_tmp, node->port);
						node->RLNode = node_tmp;
					}
					node = node->RLNode;
				}
			} else {
				if (ii->ip & (ip1 >> ++j)) {
					if (!node->LRNode) {
						TNode_pro *node_tmp = (TNode_pro*)malloc(sizeof(TNode_pro));
						init_node_pro(node_tmp, node->port);
						node->LRNode = node_tmp;
					}
					node = node->LRNode;
				} else {
					if (!node->LLNode) {
						TNode_pro *node_tmp = (TNode_pro*)malloc(sizeof(TNode_pro));
						init_node_pro(node_tmp, node->port);
						node->LLNode = node_tmp;
					}
					node = node->LLNode;
				}
			}
		} // for j
		if (ii->length % 2) {
			if (ii->ip & (ip1 >> (ii->length-1))) {
				if (!node->RRNode) {
					TNode_pro *node_tmp = (TNode_pro*)malloc(sizeof(TNode_pro));
					init_node_pro(node_tmp, ii->port);
					node->RRNode = node_tmp;
				}
				if (!node->RLNode) {
					TNode_pro *node_tmp = (TNode_pro*)malloc(sizeof(TNode_pro));
					init_node_pro(node_tmp, ii->port);
					node->RLNode = node_tmp;
				}
			} else {
				if (!node->LRNode) {
					TNode_pro *node_tmp = (TNode_pro*)malloc(sizeof(TNode_pro));
					init_node_pro(node_tmp, ii->port);
					node->LRNode = node_tmp;
				}
				if (!node->LLNode) {
					TNode_pro *node_tmp = (TNode_pro*)malloc(sizeof(TNode_pro));
					init_node_pro(node_tmp, ii->port);
					node->LLNode = node_tmp;
				}
			}
		} else {
			node->port = ii->port;
		}
		node = tr;
	}
	fclose(fp);
	leaf_push(tr);
}

// To look up ip in the two-bit prefix tree.
u8 lookup_pref_tree_pro(Tree_pro tr, u32 ip) {
	u32 ip1 = 1 << 31;
	TNode_pro *node = tr;
	TNode_pro *node_tmp = NULL;
	for (int i = 0; node; i++) {
		node_tmp = node;
		if (ip & (ip1 >> i)) {
			node = (ip & (ip1 >> ++i)) ? node->RRNode : node->RLNode;
		} else {
			node = (ip & (ip1 >> ++i)) ? node->LRNode : node->LLNode;
		}
	}
	return node_tmp->port;
}

// To count nodes in the two-bit prefix tree.
void count_nodes_pro(Tree_pro tr, int* nodes) {
	if(tr->LLNode) count_nodes_pro(tr->LLNode, nodes);
	if(tr->LRNode) count_nodes_pro(tr->LRNode, nodes);
	if(tr->RLNode) count_nodes_pro(tr->RLNode, nodes);
	if(tr->RRNode) count_nodes_pro(tr->RRNode, nodes);
	*nodes = *nodes + 1;
}

// To build the two-bit prefix tree with pointer and vector compress by two-bit prefix tree with leaf pushing.
void build_pref_tree_comp(Tree_pro tr2, Tree_comp tr3){
	tr3->type = (tr2->LLNode->LLNode)? 1 : 0;
	tr3->type = tr3->type << 1 | ((tr2->LRNode->LLNode)? 1 : 0);
	tr3->type = tr3->type << 1 | ((tr2->RLNode->LLNode)? 1 : 0);
	tr3->type = tr3->type << 1 | ((tr2->RRNode->LLNode)? 1 : 0);
	int num_inter = __builtin_popcount(tr3->type);
	int num_leaf = 4 - num_inter;
	tr3->ptr_0 = (TNode_comp*)malloc(num_leaf * sizeof(TNode_comp));
	tr3->ptr_1 = (TNode_comp*)malloc(num_inter * sizeof(TNode_comp));
	int i_inter = 0;
	int i_leaf = 0;
	for (int i = 0; i < 4; i++) {
		if (tr3->type & (1 << i)) {
			build_pref_tree_comp(i_to_node(tr2, i), &(tr3->ptr_1[i_inter]));
			i_inter++;
		} else {
			tr3->ptr_0[i_leaf].port = i_to_node(tr2, i)->port;
			i_leaf++;
		}
	}
}

// To look up ip in the two-bit prefix tree with pointer and vector compress.
u8 lookup_pref_tree_comp(Tree_comp tr, u32 ip) {
	u32 ip1 = 1 << 31;
	TNode_comp *node = tr;
	for (int i = 0; ; i++) {
		if (ip & (ip1 >> i)) {
			if (ip & (ip1 >> ++i)) {
				if (node->type & 1) {
					node = &(node->ptr_1[0]);
				} else {
					return node->ptr_0[0].port;
				}
			} else {
				int t1 = __builtin_popcount(node->type & 1);
				if (node->type & 2) {
					node = &(node->ptr_1[t1]);
				} else {
					return node->ptr_0[1 - t1].port;
				}
			}
		} else {
			if (ip & (ip1 >> ++i)) {
				int t1 = __builtin_popcount(node->type & 3);
				if (node->type & 4) {
					node = &(node->ptr_1[t1]);
				} else {
					return node->ptr_0[2 - t1].port;
				}
			} else {
				int t1 = __builtin_popcount(node->type & 7);
				if (node->type & 8) {
					node = &(node->ptr_1[t1]);
				} else {
					return node->ptr_0[3 - t1].port;
				}
			}
		}
	}
}

// To count nodes in the two-bit prefix tree with pointer and vector compress.
void count_nodes_comp(Tree_comp tr, int* nodes) {
	int num_inter = __builtin_popcount(tr->type);
	for (int i = 0; i < num_inter; i++) {
		count_nodes_comp(&(tr->ptr_1[i]), nodes);
	}
	*nodes = *nodes + 4 - num_inter;
}

int main() {
	Tree tr1 = (Tree)malloc(sizeof(TNode));
	Tree_pro tr2 = (Tree_pro)malloc(sizeof(TNode_pro));
	Tree_comp tr3 = (Tree_comp)malloc(sizeof(TNode_comp));
	int nodes = 0;
	switch (TREE_TYPE) {
		case 0 :
			build_pref_tree(tr1);
			count_nodes(tr1, &nodes);
			printf("For all ips in forwarding-table.txt, %s has %d nodes, takes %ld B.\n", tree_s, nodes, sizeof(TNode) * nodes);
			break;
		case 1 :
			build_pref_tree_pro(tr2);
			count_nodes_pro(tr2, &nodes);
			printf("For all ips in forwarding-table.txt, %s has %d nodes, takes %ld B.\n", tree_s, nodes, sizeof(TNode_pro) * nodes);
			break;
		default :
			build_pref_tree_pro(tr2);
			build_pref_tree_comp(tr2, tr3);
			count_nodes_comp(tr3, &nodes);
			printf("For all ips in forwarding-table.txt, %s has %d nodes, takes %ld B.\n", tree_s, nodes, sizeof(TNode_comp) * nodes);
	}

	FILE *fp = fopen(testname, "r");
	ip_info *ii = (ip_info*)malloc(sizeof(ip_info));
	u8 res_port;
	char s[25];
	clock_t start, finish;
	start = clock();
	while (!feof(fp)) {
		fgets(s, 25, fp);
		get_info_from_string(s, ii);
		switch (TREE_TYPE) {
			case 0 :
				res_port = lookup_pref_tree(tr1, ii->ip);
				if (ii->port != res_port) {
					print_result(res_port, ii);
				}
				break;
			case 1 :
				res_port = lookup_pref_tree_pro(tr2, ii->ip);
				if (ii->port != res_port) {
					print_result(res_port, ii);
				}
				break;
			default :
				res_port = lookup_pref_tree_comp(tr3, ii->ip);
				if (ii->port != res_port) {
					print_result(res_port, ii);
				}
		}
	}
	finish = clock();
	printf("For all ips in test-table.txt, %s takes about %f seconds.\n", tree_s, (double)(finish - start) / CLOCKS_PER_SEC);
	fclose(fp);
}
