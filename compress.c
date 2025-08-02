#include "compress.h"
#include <stdlib.h> // Cho các hàm khác nếu cần
#include <stdio.h>
#include <string.h>

// Khai báo các cấu trúc dữ liệu cần thiết cho thuật toán nén Huffman
// Node cho cây Huffman
typedef struct HuffmanNode {
    unsigned char data;
    unsigned int freq;
    struct HuffmanNode *left, *right;
} HuffmanNode;

// Hàng đợi ưu tiên (Min-Heap)
typedef struct MinHeap {
    unsigned int size;
    unsigned int capacity;
    HuffmanNode** array;
} MinHeap;

// --- KHAI BÁO CÁC HÀM "PRIVATE" (CHỈ DÙNG TRONG FILE NÀY) ---
// Chữ ký hàm cho RLE
static int perform_rle_compress(FILE* input, FILE* output);
static int perform_rle_decompress(FILE* input, FILE* output);

// Chữ ký hàm cho Huffman
HuffmanNode* create_node(unsigned char data, unsigned int freq);
MinHeap* create_min_heap(unsigned int capacity);
void swap_nodes(HuffmanNode** a, HuffmanNode** b);
void min_heapify(MinHeap* minHeap, int idx);
HuffmanNode* extract_min(MinHeap* minHeap);
void insert_node(MinHeap* minHeap, HuffmanNode* node);
HuffmanNode* build_huffman_tree(unsigned int freq[]);
void generate_codes_recursive(HuffmanNode* root, int arr[], int top, char** codes);
void free_huffman_tree(HuffmanNode* root);
static int perform_huffman_compress(FILE* input, FILE* output);
static int perform_huffman_decompress(FILE* input, FILE* output);

// --- CÀI ĐẶT CÁC HÀM "PUBLIC" ---

int compress_file(FILE* input, FILE* output, CompressionAlgorithm algo) {
    switch (algo) {
        case ALG_RLE:
            return perform_rle_compress(input, output);
        case ALG_HUFFMAN:
            return perform_huffman_compress(input, output);
        default:
            fprintf(stderr, "Lỗi: Thuật toán nén không xác định.\n");
            return -1;
    }
}

int decompress_file(FILE* input, FILE* output, CompressionAlgorithm algo) {
    switch (algo) {
        case ALG_RLE:
            return perform_rle_decompress(input, output);
        case ALG_HUFFMAN:
            return perform_huffman_decompress(input, output);
        default:
            fprintf(stderr, "Lỗi: Thuật toán giải nén không xác định.\n");
            return -1;
    }
}

// --- CÀI ĐẶT CÁC HÀM "PRIVATE" ---

static int perform_rle_compress(FILE* input, FILE* output) {
    // Xử lý trường hợp file rỗng
   int last_char = fgetc(input);
    if (last_char == EOF) {
        return 0; // File rỗng, nén thành công
    }

    int count = 1;
    int current_char;

    // Vòng lặp đọc các ký tự còn lại
    while ((current_char = fgetc(input)) != EOF) {
        if (current_char == last_char && count < 255) {
            // Nếu ký tự giống ký tự trước và số đếm chưa đầy 1 byte
            count++;
        } else {
            // Nếu ký tự khác, hoặc đã đếm đủ 255
            // Ghi số đếm và ký tự cũ ra file
            fputc(count, output);
            fputc(last_char, output);

            // Reset lại cho ký tự mới
            last_char = current_char;
            count = 1;
        }
    }

    // GHI CẶP CUỐI CÙNG
    fputc(count, output);
    fputc(last_char, output);

    return 0;
}

static int perform_rle_decompress(FILE* input, FILE* output) {
    int count;
    int data;

    // Vòng lặp đọc từng cặp byte
    while ((count = fgetc(input)) != EOF) {
        data = fgetc(input);
        if (data == EOF) {
            // File nén bị lỗi (thiếu byte dữ liệu)
            return -1;
        }

        // Ghi ký tự ra file `count` lần
        for (int i = 0; i < count; i++) {
            fputc(data, output);
        }
    }

    return 0; // Thành công
}

static int perform_huffman_compress(FILE* input, FILE* output) {
    // 1. Đếm tần suất và lấy kích thước file
    unsigned int freq[MAX_TREE_HT] = {0};
    int c;
    uint64_t original_size = 0;
    uint8_t num_symbols = 0;

    rewind(input);
    while ((c = fgetc(input)) != EOF) {
        if (freq[c] == 0) {
            num_symbols++;
        }
        freq[c]++;
        original_size++;
    }

    // Xử lý file rỗng
    if (original_size == 0) {
        HuffmanHeader header;
        memcpy(header.magic, HUFFMAN_MAGIC, 4);
        header.num_symbols = 0;
        header.original_size = 0;
        fwrite(&header, sizeof(HuffmanHeader), 1, output);
        return 0;
    }

    // 2. Xây dựng cây Huffman
    HuffmanNode* root = build_huffman_tree(freq);

    // 3. Tạo bảng mã
    char* codes[MAX_TREE_HT] = {NULL};
    int arr[MAX_TREE_HT], top = 0;
    generate_codes_recursive(root, arr, top, codes);

    // 4. Ghi header đã tối ưu
    HuffmanHeader header;
    memcpy(header.magic, HUFFMAN_MAGIC, 4);
    header.num_symbols = num_symbols;
    header.original_size = original_size;
    fwrite(&header, sizeof(HuffmanHeader), 1, output);

    // Ghi bảng tần suất rút gọn
    for (int i = 0; i < MAX_TREE_HT; i++) {
        if (freq[i] > 0) {
            SymbolFreq sf = {(uint8_t)i, freq[i]};
            fwrite(&sf, sizeof(SymbolFreq), 1, output);
        }
    }

    // 5. Nén và ghi body
    rewind(input);
    unsigned char byte_buffer = 0;
    int bit_count = 0;
    while ((c = fgetc(input)) != EOF) {
        char* code = codes[c];
        for (int i = 0; code[i] != '\0'; i++) {
            byte_buffer <<= 1;
            if (code[i] == '1') {
                byte_buffer |= 1;
            }
            bit_count++;
            if (bit_count == 8) {
                fputc(byte_buffer, output);
                bit_count = 0;
                byte_buffer = 0;
            }
        }
    }
    // Ghi các bit còn lại nếu có
    if (bit_count > 0) {
        byte_buffer <<= (8 - bit_count);
        fputc(byte_buffer, output);
    }

    // 6. Dọn dẹp
    free_huffman_tree(root);
    for(int i=0; i < MAX_TREE_HT; i++) {
        free(codes[i]);
    }
    return 0;
}

static int perform_huffman_decompress(FILE* input, FILE* output) {
    // 1. Đọc và xác thực header
    HuffmanHeader header;
    size_t read_bytes = fread(&header, sizeof(HuffmanHeader), 1, input);
    if (read_bytes < 1 || memcmp(header.magic, HUFFMAN_MAGIC, 4) != 0) {
        fprintf(stderr, "Lỗi: File không phải là định dạng Huffman hợp lệ hoặc header bị hỏng.\n");
        return -1;
    }

    // Xử lý file rỗng
    if (header.original_size == 0) {
        return 0;
    }

    // 2. Đọc bảng tần suất rút gọn và xây dựng lại bảng đầy đủ
    unsigned int freq[MAX_TREE_HT] = {0};
    for (int i = 0; i < header.num_symbols; i++) {
        SymbolFreq sf;
        if (fread(&sf, sizeof(SymbolFreq), 1, input) < 1) {
            fprintf(stderr, "Lỗi: File nén bị hỏng khi đang đọc bảng tần suất.\n");
            return -1;
        }
        freq[sf.symbol] = sf.frequency;
    }

    // 3. Tái tạo cây Huffman
    HuffmanNode* root = build_huffman_tree(freq);
    HuffmanNode* current = root;

    // 4. Giải nén body
    int c;
    uint64_t decoded_count = 0;
    while (decoded_count < header.original_size && (c = fgetc(input)) != EOF) {
        for (int i = 7; i >= 0; i--) {
            int bit = (c >> i) & 1;
            if (bit) {
                current = current->right;
            } else {
                current = current->left;
            }

            // Nếu là node lá, ghi ký tự và quay về gốc
            if (current->left == NULL && current->right == NULL) {
                fputc(current->data, output);
                decoded_count++;
                if (decoded_count == header.original_size) break;
                current = root;
            }
        }
    }

    // 5. Dọn dẹp
    free_huffman_tree(root);
    
    // Kiểm tra xem số lượng ký tự đã giải nén có khớp không
    if (decoded_count != header.original_size) {
        fprintf(stderr, "Lỗi: Dữ liệu giải nén bị hỏng hoặc không đầy đủ.\n");
        return -1;
    }

    return 0;
}

HuffmanNode* create_node(unsigned char data, unsigned int freq) {
    HuffmanNode* node = (HuffmanNode*)malloc(sizeof(HuffmanNode));
    node->left = node->right = NULL;
    node->data = data;
    node->freq = freq;
    return node;
}

MinHeap* create_min_heap(unsigned int capacity) {
    MinHeap* minHeap = (MinHeap*)malloc(sizeof(MinHeap));
    minHeap->size = 0;
    minHeap->capacity = capacity;
    minHeap->array = (HuffmanNode**)malloc(minHeap->capacity * sizeof(HuffmanNode*));
    return minHeap;
}

void swap_nodes(HuffmanNode** a, HuffmanNode** b) {
    HuffmanNode* t = *a;
    *a = *b;
    *b = t;
}

void min_heapify(MinHeap* minHeap, int idx) {
    int smallest = idx;
    unsigned int left = 2 * idx + 1;
    unsigned int right = 2 * idx + 2;
    // Kiểm tra khoảng bên trái và phải có vượt quá kích thước không và so sánh tần số
    // của các node con với node hiện tại
    if (left < minHeap->size && minHeap->array[left]->freq < minHeap->array[smallest]->freq)
        smallest = left;
    if (right < minHeap->size && minHeap->array[right]->freq < minHeap->array[smallest]->freq)
        smallest = right;

    // Nếu node hiện tại không phải là nhỏ nhất, hoán đổi và tiếp tục heapify
    if (smallest != idx) {
        swap_nodes(&minHeap->array[smallest], &minHeap->array[idx]);
        min_heapify(minHeap, smallest);
    }
}

HuffmanNode* extract_min(MinHeap* minHeap) {
    HuffmanNode* temp = minHeap->array[0];
    minHeap->array[0] = minHeap->array[minHeap->size - 1];
    --minHeap->size;
    min_heapify(minHeap, 0);
    return temp;
}

void insert_node(MinHeap* minHeap, HuffmanNode* node) {
    ++minHeap->size;
    int i = minHeap->size - 1;
    while (i && node->freq < minHeap->array[(i - 1) / 2]->freq) {
        minHeap->array[i] = minHeap->array[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    minHeap->array[i] = node;
}

HuffmanNode* build_huffman_tree(unsigned int freq[]) {
    MinHeap* minHeap = create_min_heap(MAX_TREE_HT);
    for (int i = 0; i < MAX_TREE_HT; ++i) {
        if (freq[i]) {
            insert_node(minHeap, create_node(i, freq[i]));
        }
    }

    // Trường hợp đặc biệt: chỉ có 1 ký tự duy nhất
    if (minHeap->size == 1) {
        HuffmanNode* single_node = extract_min(minHeap);
        // Tạo một root giả với node đó làm con trái
        HuffmanNode* root = create_node('$', single_node->freq);
        root->left = single_node;
        root->right = NULL;
        free(minHeap->array);
        free(minHeap);
        return root;
    }

    while (minHeap->size > 1) {
        HuffmanNode* left = extract_min(minHeap);
        HuffmanNode* right = extract_min(minHeap);
        
        // Tạo node nội, data có thể là ký tự đặc biệt như '$'
        HuffmanNode* top = create_node('$', left->freq + right->freq);
        top->left = left;
        top->right = right;
        insert_node(minHeap, top);
    }
    
    HuffmanNode* root = extract_min(minHeap);
    free(minHeap->array);
    free(minHeap);
    return root;
}

void generate_codes_recursive(HuffmanNode* root, int arr[], int top, char** codes) {
    if (root->left) {
        arr[top] = 0;
        generate_codes_recursive(root->left, arr, top + 1, codes);
    }
    if (root->right) {
        arr[top] = 1;
        generate_codes_recursive(root->right, arr, top + 1, codes);
    }
    if (!(root->left) && !(root->right)) { // Nếu là node lá
        codes[root->data] = (char*)malloc(top + 1);
        for (int i = 0; i < top; ++i) {
            codes[root->data][i] = arr[i] + '0';
        }
        codes[root->data][top] = '\0';
    }
}

void free_huffman_tree(HuffmanNode* root) {
    if (root == NULL) return;
    free_huffman_tree(root->left);
    free_huffman_tree(root->right);
    free(root);
}
