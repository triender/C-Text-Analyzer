#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <windows.h>

extern "C" {
#include "compress.h"
#include "hashtable.h"
}

// Định nghĩa các mã lệnh
#define CMD_UNKNOWN     0
#define CMD_READ        2
#define CMD_ANALYST     3
#define CMD_FIND        4
#define CMD_COMPRESS    5
#define CMD_DECOMPRESS  6

#define SORT_NONE    0
#define SORT_ALPHA   1 // Theo alphabet
#define SORT_LEN_DEC 2 // Theo độ dài giảm dần
#define SORT_LEN_ASC 3 // Theo độ dài tăng dần
#define HASH_TABLE_SIZE 10007

// Macro để kiểm tra cấp phát bộ nhớ
#define CHECK_ALLOC(ptr, message) \
    if ((ptr) == NULL) { \
        fprintf(stderr, "Lỗi cấp phát bộ nhớ: %s\n", message); \
        exit(EXIT_FAILURE); \
    }

typedef struct {
    char *word; // Con trỏ để lưu chuỗi (từ)
    int count;  // Số lần xuất hiện
} WordStats;

// bảng ánh xạ thuật toán nén
typedef struct {
    const char* name;
    CompressionAlgorithm algo;
} AlgoMap;

// Tạo một bảng tra cứu tĩnh
static const AlgoMap algo_mappings[] = {
    { "rle",     ALG_RLE },
    { "huffman", ALG_HUFFMAN }
    // Dễ dàng thêm thuật toán mới ở đây, ví dụ:
    // { "lz77", ALG_LZ77 },
};
// Tính số lượng thuật toán trong bảng
static const int num_algos = sizeof(algo_mappings) / sizeof(algo_mappings[0]);

// Cấu hình chương trình
typedef struct {
    int command_code;
    char *input_filename;
    char *output_filename;
    char *keyword;

    // Tùy chọn
    int case_sensitive;
    int exact_match;
    int sort_mode;
    CompressionAlgorithm algo;
    int algo_is_manual;
} Config;

// --- Khai báo các hàm ---
CompressionAlgorithm get_algo_from_string(const char* str);
CompressionAlgorithm get_algo_from_filename(const char *filename);
const char* get_string_from_algo(CompressionAlgorithm algo);

int get_command_code(const char *command_str);
int parse_arguments(int argc, char *argv[], Config *config);
void print_usage(char *program_name);
void perform_read(FILE *file);
void perform_analysis(FILE *file, int case_sensitive, int sort_mode, const char* output_filename);
void perform_find(FILE *file, int case_sensitive, int exact_match, const char *word_to_find, const char *output_filename);
int perform_compress(FILE* input_file, const Config* config);
int perform_decompress(FILE* input_file, const Config* config);

void to_lowercase(char *str);
int compare_alpha(const void *a, const void *b);
int compare_len_dec(const void *a, const void *b);
int compare_len_asc(const void *a, const void *b);

WordStats* ht_to_array(HashTable* table, int* count);

// --- Hàm main ---
int main(int argc, char *argv[]) {
    // Thiết lập console để in tiếng Việt
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);

    // --- Kiểm tra số lượng đối số ---
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    // --- Lấy mã lệnh và tên tệp từ đối số ---
    Config config;
    if(parse_arguments(argc, argv, &config) != 0) {
        print_usage(argv[0]);
        return 1;
    }

    const char* input_mode = (config.command_code == CMD_COMPRESS || config.command_code == CMD_DECOMPRESS) ? "rb" : "r";
    FILE* input_file = fopen(config.input_filename, input_mode);
    if (input_file == NULL) {
        fprintf(stderr, "Lỗi: Không thể mở tệp đầu vào '%s'\n", config.input_filename);
        return 1;
    }

    switch (config.command_code) {
        case CMD_READ:
            perform_read(input_file);
            break;
        case CMD_ANALYST:
            perform_analysis(input_file, config.case_sensitive, config.sort_mode, config.output_filename);
            break;
        case CMD_FIND:
            perform_find(input_file, config.case_sensitive, config.exact_match, config.keyword, config.output_filename);
            break;
        case CMD_COMPRESS:
            if (perform_compress(input_file, &config) != 0) {
                return 1; // Trả về lỗi nếu nén thất bại
            }
            break;
        case CMD_DECOMPRESS:
            if (perform_decompress(input_file, &config) != 0) {
                return 1; // Trả về lỗi nếu giải nén thất bại
            }
            break;
        default:
            printf("Lỗi: Lệnh '%s' không hợp lệ.\n", argv[1]);
            print_usage(argv[0]);
            break;
    }

    fclose(input_file);
    return 0;
}

/**
 * @brief Chuyển đổi chuỗi lệnh thành mã lệnh số nguyên.
 * @return Mã lệnh tương ứng hoặc CMD_UNKNOWN nếu không hợp lệ.
 */
int get_command_code(const char *command_str) {
    if (strcmp(command_str, "read") == 0) return CMD_READ;
    if (strcmp(command_str, "analyst") == 0) return CMD_ANALYST;
    if (strcmp(command_str, "find") == 0) return CMD_FIND;
    if (strcmp(command_str, "compress") == 0) return CMD_COMPRESS;
    if (strcmp(command_str, "decompress") == 0) return CMD_DECOMPRESS;
    return CMD_UNKNOWN;
}

/**
 * @brief Phân tích các tham số dòng lệnh và điền vào struct AppConfig.
 * @param argc Số lượng tham số.
 * @param argv Mảng các tham số.
 * @param config Con trỏ đến struct cấu hình để điền dữ liệu vào.
 * @return int Trả về 0 nếu thành công, -1 nếu có lỗi.
 */
int parse_arguments(int argc, char *argv[], Config *config) {
    // Khởi tạo giá trị mặc định
    config->command_code = get_command_code(argv[1]);
    config->input_filename = argv[2];
    config->output_filename = NULL;
    config->keyword = NULL;
    config->case_sensitive = 0;
    config->exact_match = 0;
    config->sort_mode = SORT_NONE;
    config->algo = ALG_RLE;
    config->algo_is_manual = 0;

    // Xác định tham số bắt buộc và điểm bắt đầu của tùy chọn
    int start_options_index = 3;
    if (config->command_code == CMD_FIND) {
        if (argc < 4) {
            fprintf(stderr, "Lỗi: Lệnh 'find' cần có từ khóa tìm kiếm.\n");
            return -1;
        }
        config->keyword = argv[3];
        start_options_index = 4;
    }

    // Vòng lặp xử lý các tùy chọn còn lại
    for (int i = start_options_index; i < argc; i++) {
        // Kiểm tra case-sensitive
        if (strcmp(argv[i], "--case-sensitive") == 0) config->case_sensitive = 1;
        // Kiểm tra match exact
        else if (strcmp(argv[i], "--match") == 0) config->exact_match = 1;

        // Kiểm tra tùy chọn sort
        else if (strcmp(argv[i], "--sort") == 0 && config->command_code == CMD_ANALYST) {
            if (i + 1 < argc) { // Đảm bảo có giá trị đi kèm
                i++; // Chuyển sang đối số tiếp theo
                if (strcmp(argv[i], "alpha") == 0) config->sort_mode = SORT_ALPHA;
                else if (strcmp(argv[i], "dec") == 0) config->sort_mode = SORT_LEN_DEC;
                else if (strcmp(argv[i], "asc") == 0) config->sort_mode = SORT_LEN_ASC;
                else {
                    fprintf(stderr, "Lỗi: Tùy chọn sắp xếp không hợp lệ '%s'.\n", argv[i]);
                    print_usage(argv[0]);
                    return 1;
                }
            }
            else {
                printf("Lỗi: Cần cung cấp tùy chọn sắp xếp sau '--sort'.\n");
                print_usage(argv[0]);
                return 1;
            }
        }

        // Kiểm tra tùy chọn đầu ra cho kết quả
        // đối với lệnh compress và decompress thì tùy chọn này là bắt buộc
        else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) {
                i++;
                config->output_filename = argv[i];
            }
            else {
                fprintf(stderr, "Lỗi: Cần cung cấp tên tệp đầu ra sau tùy chọn '-o' hoặc '--output'.\n");
                print_usage(argv[0]);
                return 1;
            }
        }

        // Kiểm tra thuật toán nén khi lệnh là nén hoặc giải nén
        else if (strcmp(argv[i], "--algo") == 0 && (config->command_code == CMD_COMPRESS || config->command_code == CMD_DECOMPRESS)) {
            if (i + 1 < argc) {
                i++;
                config->algo = get_algo_from_string(argv[i]);
                if (config->algo == ALG_UNKNOWN) {
                    fprintf(stderr, "Lỗi: Thuật toán nén không hợp lệ '%s'.\n", argv[i]);
                    print_usage(argv[0]);
                    return 1;
                }
                config->algo_is_manual = 1; // Đánh dấu rằng thuật toán đã được chỉ định
            } else {
                fprintf(stderr, "Lỗi: Cần cung cấp thuật toán nén sau tùy chọn '--algo'.\n");
                print_usage(argv[0]);
                return 1;
            }
        }
    }

    // Kiểm tra các điều kiện bắt buộc sau khi đã phân tích
    if ((config->command_code == CMD_COMPRESS || config->command_code == CMD_DECOMPRESS) && config->output_filename == NULL) {
        fprintf(stderr, "Lỗi: Lệnh '%s' cần có tệp đầu ra (-o).\n", argv[1]);
        return -1;
    }

    return 0; // Thành công
}

/** @brief In hướng dẫn sử dụng chương trình.
 * @param program_name Tên chương trình, thường là argv[0].
 */
void print_usage(char *program_name) {
    printf("Cách dùng: %s <lệnh> <tên_tệp> [tùy_chọn]\n", program_name);
    printf("Các lệnh:\n");
    printf("  read        Đọc và in nội dung của tệp.\n");
    printf("  analyst     Phân tích tệp.\n");
    printf("  find        Tìm kiếm một từ trong tệp.\n");
    printf("  compress    Nén tệp.\n");
    printf("  decompress  Giải nén tệp.\n\n");
    printf("Các tùy chọn cho 'analyst':\n");
    printf("  --sort type Sắp xếp kết quả ('alpha', 'dec', 'asc').\n");
    printf("  --case-sensitive  Phân biệt chữ hoa/thường.\n");
    printf("  -o <file>   Ghi kết quả ra tệp.\n");
    printf("Các tùy chọn cho 'find':\n");
    printf("  --match     Tìm kiếm khớp chính xác (mặc định là tìm chuỗi con).\n");
    printf("  --case-sensitive  Phân biệt chữ hoa/thường.\n");
}

/** @brief Đọc và in nội dung của tệp.
 * @param file Con trỏ đến tệp cần đọc.
 */
void perform_read(FILE *file) {
    printf("--- Nội dung của tệp ---\n");
    int character;
    while ((character = fgetc(file)) != EOF)
        putchar(character);
    printf("\n------------------------\n");
}

/**
 * @brief Phân tích tệp văn bản, thống kê các từ và xuất kết quả.
 * @param file Con trỏ đến tệp cần phân tích.
 * @param case_sensitive Chế độ phân biệt chữ hoa/thường.
 * @param sort_mode Chế độ sắp xếp kết quả.
 * @param output_filename Tên tệp đầu ra (nếu có).
 */
void perform_analysis(FILE *file, int case_sensitive, int sort_mode, const char* output_filename) {
    FILE *output_stream = stdout; // Mặc định in ra console
    if (output_filename != NULL) {
        output_stream = fopen(output_filename, "w");
        if (output_stream == NULL) {
            printf("Lỗi: Không thể tạo tệp đầu ra '%s'\n", output_filename);
            return; // Thoát nếu không tạo được file
        }
        printf("Đã ghi kết quả vào tệp: %s\n", output_filename);
    }

    HashTable *hash_table = create_table(HASH_TABLE_SIZE);
    if (hash_table == NULL) {
        fprintf(output_stream, "Lỗi: Không thể tạo bảng băm.\n");
        if (output_stream != stdout) fclose(output_stream);
        return;
    }

    // --- Phân tích tệp ---
    long char_count = 0;
    int line_count = 0;
    long total_word_count = 0;
    char line_buffer[2048];

    rewind(file); // Đặt con trỏ tệp về đầu để đọc lại
    while((fgets(line_buffer, sizeof(line_buffer), file)) != NULL) {
        char_count += strlen(line_buffer);
        line_count++;

        // Tách từ đầu tiên trong dòng
        char *token = strtok(line_buffer, " \t\n\r,.;:!?\"()"); // Các ký tự phân tách
        while (token != NULL) {
            total_word_count++;

            if (!case_sensitive) {
                to_lowercase(token);
            }
            ht_insert(hash_table, token);
            // Tiếp tục tách các từ sau, bắt đầu sau từ hiện tại
            token = strtok(NULL, " \t\n\r,.;:!?\"()");
        }
    }
    
    // chuyển đổi bảng băm thành mảng
    int unique_word_count = 0;
    WordStats *word_list = ht_to_array(hash_table, &unique_word_count);
    free_table(hash_table); // không cần bảng băm nữa

    if (word_list == NULL) {
        fprintf(output_stream, "Không có từ nào trong tệp.\n");
        if (output_stream != stdout) fclose(output_stream);
        return;
    }
    // --- In thống kê cơ bản ---
    fprintf(output_stream, "--- Thống kê cơ bản ---\n");
    fprintf(output_stream, "Số ký tự: %ld\n", char_count);
    fprintf(output_stream, "Số từ (tổng cộng): %ld\n", total_word_count);
    fprintf(output_stream, "Số từ (duy nhất): %d\n", unique_word_count);
    fprintf(output_stream, "Số dòng: %d\n", line_count);
    
    // --sort_mode 
    if (sort_mode == SORT_ALPHA) 
        qsort(word_list, unique_word_count, sizeof(WordStats), compare_alpha);
    else if (sort_mode == SORT_LEN_DEC)
        qsort(word_list, unique_word_count, sizeof(WordStats), compare_len_dec);
    else if (sort_mode == SORT_LEN_ASC)
        qsort(word_list, unique_word_count, sizeof(WordStats), compare_len_asc);

    // Tính toán độ dài min/max và tần suất min/max
    size_t max_len = strlen(word_list[0].word);
    size_t min_len = max_len;
    int max_freq = word_list[0].count;
    int min_freq = max_freq;
    for (int i = 0; i < unique_word_count; i++) {
        size_t current_len = strlen(word_list[i].word);
        int current_freq = word_list[i].count;
        if (current_len > max_len) max_len = current_len;
        if (current_len < min_len) min_len = current_len;
        if (current_freq > max_freq) max_freq = current_freq;
        if (current_freq < min_freq) min_freq = current_freq;
    }

    // In thống kê chi tiết
    fprintf(output_stream, "--- Phân tích chi tiết ---\n\n");

    // In tất cả các từ dài nhất
    fprintf(output_stream, "Các từ dài nhất (%zu ký tự):\n", max_len);
    for (int i = 0; i < unique_word_count; i++)
        if (strlen(word_list[i].word) == max_len)
            fprintf(output_stream, "  - %s\n", word_list[i].word);
    fprintf(output_stream, "\n");

    // In tất cả các từ ngắn nhất
    fprintf(output_stream, "Các từ ngắn nhất (%zu ký tự):\n", min_len);
    for (int i = 0; i < unique_word_count; i++)
        if (strlen(word_list[i].word) == min_len)
            fprintf(output_stream, "  - %s\n", word_list[i].word);
    fprintf(output_stream, "\n");

    // In tất cả các từ xuất hiện nhiều nhất
    fprintf(output_stream, "Các từ xuất hiện nhiều nhất (%d lần):\n", max_freq);
    for (int i = 0; i < unique_word_count; i++)
        if (word_list[i].count == max_freq)
            fprintf(output_stream, "  - %s\n", word_list[i].word);
    fprintf(output_stream, "\n");

    // In tất cả các từ xuất hiện ít nhất
    fprintf(output_stream, "Các từ xuất hiện ít nhất (%d lần):\n", min_freq);
    for (int i = 0; i < unique_word_count; i++)
        if (word_list[i].count == min_freq)
            fprintf(output_stream, "  - %s\n", word_list[i].word);
    fprintf(output_stream, "-------------------------\n");

    // --- Giải phóng bộ nhớ ---
    for (int i = 0; i < unique_word_count; i++)
        free(word_list[i].word);
    free(word_list);
    if (output_stream != stdout) {
        fclose(output_stream);
    }
}

/**
 * @brief Tìm kiếm một từ trong tệp và in kết quả.
 * @param file Con trỏ đến tệp cần tìm kiếm.
 * @param case_sensitive Chế độ phân biệt chữ hoa/thường.
 * @param exact_match Chế độ tìm kiếm khớp chính xác hay chuỗi con.
 * @param word_to_find Từ cần tìm.
 * @param output_filename Tên tệp đầu ra (nếu có).
 */
void perform_find(FILE *file, int case_sensitive, int exact_match, const char *word_to_find, const char *output_filename) {
    FILE *output_stream = stdout; // Mặc định in ra console
    if (output_filename != NULL) {
        output_stream = fopen(output_filename, "w");
        if (output_stream == NULL) {
            printf("Lỗi: Không thể tạo tệp đầu ra '%s'\n", output_filename);
            return; // Thoát nếu không tạo được file
        }
        printf("Đã ghi kết quả vào tệp: %s\n", output_filename);
    }

    // --- Tìm kiếm từ trong tệp ---
    char line_buffer[2048];
    int found = 0;

    // Chuyển đổi từ cần tìm sang chữ thường nếu không phân biệt hoa/thường
    char *keyword_to_find = strdup(word_to_find);
    if (!case_sensitive) to_lowercase(keyword_to_find);

    rewind(file); // Đặt con trỏ tệp về đầu để đọc lại
    while (fgets(line_buffer, sizeof(line_buffer), file) != NULL) {
        if (exact_match) {
            // --- LOGIC TÌM TỪ KHỚP CHÍNH XÁC (strtok) ---
            char *line_copy = strdup(line_buffer);
            char *token = strtok(line_copy, " \t\n\r,.;:!?\"()");
            while (token != NULL) {
                char* word_to_compare = token;
                char* temp_token_copy = NULL;

                if (!case_sensitive) {
                    temp_token_copy = strdup(token);
                    to_lowercase(temp_token_copy);
                    word_to_compare = temp_token_copy;
                }

                if (strcmp(word_to_compare, keyword_to_find) == 0) {
                    fprintf(output_stream, "Tìm thấy từ '%s' trong dòng: %s", word_to_find, line_buffer);
                    found++;
                    free(temp_token_copy); // Giải phóng ở đây
                    free(line_copy);       // Và giải phóng line_copy
                    goto next_line;        // Nhảy ra ngoài để tránh break 2 lớp
                }
                free(temp_token_copy); // Giải phóng nếu không khớp
                token = strtok(NULL, " \t\n\r,.;:!?\"()");
            }
            free(line_copy);
            next_line:; // Nhãn để goto

        } else {
            // --- LOGIC TÌM CHUỖI CON (strstr) ---
            char *search_target = line_buffer;
            char *temp_line_copy = NULL;
            if (!case_sensitive) {
                temp_line_copy = strdup(line_buffer);
                to_lowercase(temp_line_copy);
                search_target = temp_line_copy;
            }
            if (strstr(search_target, keyword_to_find) != NULL) {
                fprintf(output_stream, "Tìm thấy từ '%s' trong dòng: %s", keyword_to_find, line_buffer);
                found++;
            }
            free(temp_line_copy);
        }
    }

    free(keyword_to_find); // Giải phóng bộ nhớ đã cấp phát cho từ cần tìm

    if (!found) fprintf(output_stream, "Không tìm thấy từ '%s' trong tệp.\n", word_to_find);
    if (output_stream != stdout) fclose(output_stream);
}

/**
 * @brief Nén một tệp dựa trên cấu hình đã cho.
 * @param input_file Con trỏ đến tệp đầu vào.
 * @param config Cấu hình chứa tên tệp đầu ra và thuật toán.
 * @return 0 nếu thành công, -1 nếu thất bại.
 */
int perform_compress(FILE* input_file, const Config* config) {
    const char* extension = get_string_from_algo(config->algo);
    // Tạo vùng nhớ cho tên tệp đầu ra
    char* output_name = (char*)malloc(strlen(config->output_filename) + strlen(extension) + 2); // +2 cho '.' và '\0'
    CHECK_ALLOC(output_name, "Tạo tên tệp đầu ra cho compress");
    sprintf(output_name, "%s.%s", config->output_filename, extension);

    FILE* output_file = fopen(output_name, "wb");
    if (output_file == NULL) {
        fprintf(stderr, "Lỗi: Không thể tạo tệp đầu ra '%s'\n", output_name);
        free(output_name);
        return -1;
    }

    printf("Đang nén '%s' -> '%s' (thuật toán: %s)\n", config->input_filename, output_name, extension);
    int result = compress_file(input_file, output_file, config->algo);

    fclose(output_file); // Đóng tệp ngay sau khi dùng xong

    if (result == 0) {
        printf("Nén thành công!\n");
    } else {
        fprintf(stderr, "Lỗi: Nén thất bại!\n");
        free(output_name);
        return -1;
    }

    free(output_name);
    return 0;
}

/**
 * @brief Giải nén một tệp dựa trên cấu hình đã cho.
 * @param input_file Con trỏ đến tệp đầu vào.
 * @param config Cấu hình chứa tên tệp đầu vào/ra và thuật toán (nếu có).
 * @return 0 nếu thành công, -1 nếu thất bại.
 */
int perform_decompress(FILE* input_file, const Config* config) {
    CompressionAlgorithm detected_algo;
    if (config->algo_is_manual) {
        detected_algo = config->algo;
        printf("Giải nén bằng thuật toán chỉ định: %s\n", get_string_from_algo(detected_algo));
    } else {
        detected_algo = get_algo_from_filename(config->input_filename);
        if (detected_algo == ALG_UNKNOWN) {
            fprintf(stderr, "Lỗi: Không thể tự động nhận diện thuật toán từ tệp '%s'.\n", config->input_filename);
            return -1;
        }
        printf("Tự động nhận diện thuật toán: %s\n", get_string_from_algo(detected_algo));
    }

    FILE* output_file = fopen(config->output_filename, "wb");
    if (output_file == NULL) {
        fprintf(stderr, "Lỗi: Không thể tạo tệp đầu ra '%s'\n", config->output_filename);
        return -1;
    }

    int result = decompress_file(input_file, output_file, detected_algo);
    fclose(output_file); // Đóng tệp ngay sau khi dùng xong

    if (result == 0) {
        printf("Đã giải nén '%s' -> '%s' (thuật toán: %s)\n", config->input_filename, config->output_filename, get_string_from_algo(detected_algo));
    } else {
        fprintf(stderr, "Lỗi: Giải nén thất bại!\n");
        return -1;
    }

    return 0;
}

/**
 * @brief Chuyển một chuỗi thành chữ thường.
 */
void to_lowercase(char *str) {
    for (; *str; ++str) *str = tolower(*str);
}

/**
 * @brief Hàm so sánh cho qsort, sắp xếp theo alphabet.
 */
int compare_alpha(const void *a, const void *b) {
    WordStats *wa = (WordStats*)a;
    WordStats *wb = (WordStats*)b;
    return strcmp(wa->word, wb->word);
}

/**
 * @brief Hàm so sánh cho qsort, sắp xếp theo độ dài từ (giảm dần).
 */
int compare_len_dec(const void *a, const void *b) {
    WordStats *wa = (WordStats*)a;
    WordStats *wb = (WordStats*)b;
    // Sắp xếp giảm dần: trả về số dương nếu b dài hơn a
    return strlen(wb->word) - strlen(wa->word);
}

/**
 * @brief Hàm so sánh cho qsort, sắp xếp theo độ dài từ (tăng dần).
 */
int compare_len_asc(const void *a, const void *b) {
    WordStats *wa = (WordStats*)a;
    WordStats *wb = (WordStats*)b;
    // Sắp xếp tăng dần: trả về số dương nếu a dài hơn b
    return strlen(wa->word) - strlen(wb->word);
}

/**
 * @brief Chuyển đổi bảng băm thành mảng các từ và số lần xuất hiện.
 * @param table Bảng băm cần chuyển đổi.
 * @param count Số lượng từ duy nhất trong bảng băm.
 * @return Mảng các WordStats chứa từ và số lần xuất hiện.
 */
WordStats* ht_to_array(HashTable* table, int* count) {
    // BƯỚC 1: Đếm số lượng từ trong bảng băm
    int list_count = 0;
    for (int i = 0; i < table->size; i++) {
        Entry* entry = table->entries[i];
        while (entry != NULL) {
            list_count++;
            entry = entry->next;
        }
    }
    *count = list_count; // Trả về số lượng từ duy nhất
    if (list_count == 0) return NULL;

    // BƯỚC 2: Cấp phát bộ nhớ một lần duy nhất
    WordStats* list = (WordStats*)malloc(list_count * sizeof(WordStats));
    CHECK_ALLOC(list, "Chuyển đổi bảng băm sang mảng WordStats");

    // BƯỚC 3: Đổ dữ liệu vào mảng
    int current_index = 0;
    for (int i = 0; i < table->size; i++) {
        Entry* entry = table->entries[i];
        while (entry != NULL) {
            list[current_index].word = strdup(entry->word);
            list[current_index].count = entry->count;
            current_index++;
            entry = entry->next;
        }
    }
    return list;
}

/**
 * @brief Chuyển đổi chuỗi tên thuật toán thành mã enum.
 * @return Mã enum tương ứng hoặc ALG_UNKNOWN nếu không tìm thấy.
 */
CompressionAlgorithm get_algo_from_string(const char* str) {
    for (int i = 0; i < num_algos; i++) {
        if (strcmp(str, algo_mappings[i].name) == 0) {
            return algo_mappings[i].algo;
        }
    }
    return ALG_UNKNOWN; // Giả sử bạn có định nghĩa ALG_UNKNOWN
}

/**
 * @brief Lấy chuỗi tên thuật toán từ mã enum.
 * @return Chuỗi tên (ví dụ: "rle") hoặc NULL nếu không hợp lệ.
 */
const char* get_string_from_algo(CompressionAlgorithm algo) {
    for (int i = 0; i < num_algos; i++) {
        if (algo_mappings[i].algo == algo) {
            return algo_mappings[i].name;
        }
    }
    return NULL;
}

/**
 * @brief Nhận diện thuật toán nén dựa vào đuôi của tên tệp.
 * @param filename Tên tệp cần kiểm tra.
 * @return Mã enum của thuật toán tương ứng hoặc ALG_UNKNOWN nếu không nhận diện được.
 */
CompressionAlgorithm get_algo_from_filename(const char *filename) {
    // Tìm dấu chấm cuối cùng trong tên tệp
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) {
        // Không có đuôi tệp
        return ALG_UNKNOWN;
    }

    // Lấy phần đuôi tệp (sau dấu chấm)
    const char *extension = dot + 1;

    if (strcmp(extension, "rle") == 0) return ALG_RLE;
    if (strcmp(extension, "huffman") == 0 || strcmp(extension, "huff") == 0) return ALG_HUFFMAN;
    // Backwards compatibility with short extensions
    return ALG_UNKNOWN;
}