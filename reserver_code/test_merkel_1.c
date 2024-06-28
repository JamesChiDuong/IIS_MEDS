#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>

#define DATA_SIZE 64 // Size of data blocks
#define HASH_SIZE SHA256_DIGEST_LENGTH // Size of SHA-256 hash

typedef struct TreeNode {
    unsigned char hash[HASH_SIZE]; // Hash of the node
    struct TreeNode *left; // Pointer to left child
    struct TreeNode *right; // Pointer to right child
} TreeNode;

TreeNode* create_node() {
    TreeNode *node = (TreeNode*)malloc(sizeof(TreeNode));
    memset(node->hash, 0, HASH_SIZE);
    node->left = NULL;
    node->right = NULL;
    return node;
}

void hash_data(unsigned char *data, size_t len, unsigned char *hash) {
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data, len);
    SHA256_Final(hash, &sha256);
    printf("Data: %s\r\n",data);
    printf("Sha256: %s\r\n", sha256);
    printf("Hash: %s\r\n", hash);
}

void build_tree(TreeNode **nodes, int count) {
    if (count == 1) return;
    
    int new_count = (count + 1) / 2;
    TreeNode **new_nodes = (TreeNode**)malloc(new_count * sizeof(TreeNode*));
    
    for (int i = 0; i < new_count; i++) {
        TreeNode *node = create_node();
        
        int left_idx = 2 * i;
        int right_idx = 2 * i + 1;
        
        node->left = nodes[left_idx];
        if (right_idx < count) {
            node->right = nodes[right_idx];
            unsigned char combined[2 * HASH_SIZE];
            memcpy(combined, nodes[left_idx]->hash, HASH_SIZE);
            memcpy(combined + HASH_SIZE, nodes[right_idx]->hash, HASH_SIZE);
            hash_data(combined, 2 * HASH_SIZE, node->hash);
        } else {
            memcpy(node->hash, nodes[left_idx]->hash, HASH_SIZE);
        }
        
        new_nodes[i] = node;
    }
    
    build_tree(new_nodes, new_count);
    
    *nodes = *new_nodes;
    free(new_nodes);
}

TreeNode* create_merkle_tree(unsigned char **data_blocks, int count) {
    TreeNode **nodes = (TreeNode**)malloc(count * sizeof(TreeNode*));
    
    for (int i = 0; i < count; i++) {
        nodes[i] = create_node();
        hash_data(data_blocks[i], DATA_SIZE, nodes[i]->hash);
    }
    
    build_tree(nodes, count);
    TreeNode *root = nodes[0];
    free(nodes);
    return root;
}
void print_hash(unsigned char *hash) {
    for (int i = 0; i < HASH_SIZE; i++) {
        printf("%02x", hash[i]);
    }
    printf("\n");
}

void free_tree(TreeNode *node) {
    if (node == NULL) return;
    free_tree(node->left);
    free_tree(node->right);
    free(node);
}
int main() {
    unsigned char *data_blocks[4];
    data_blocks[0] = (unsigned char*)"Hello, world!";
    data_blocks[1] = (unsigned char*)"Merkle trees";
    data_blocks[2] = (unsigned char*)"are very useful.";
    data_blocks[3] = (unsigned char*)"Cryptography";

    TreeNode *root = create_merkle_tree(data_blocks, 4);
    printf("Root hash: ");
    print_hash(root->hash);
    
    free_tree(root);
    return 0;
}
