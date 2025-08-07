#include <iostream>
#include <windows.h>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cctype>

// GLAD phải được include trước GLFW
#include "libs/glad/include/glad/glad.h"
#include "libs/glfw/include/GLFW/glfw3.h"

#include "libs/imgui/imgui.h"
#include "libs/imgui/backends/imgui_impl_glfw.h"
#include "libs/imgui/backends/imgui_impl_opengl3.h"

#include <commdlg.h>

#define STB_IMAGE_IMPLEMENTATION
#include "libs/stb/stb_image.h"     

#include "core_logic/version.h"
// Include các module core logic
extern "C" {
#include "core_logic/compress.h"
#include "core_logic/hashtable.h"
}

using namespace std;
using namespace ImGui;

// Định nghĩa các mã lệnh và chế độ sắp xếp từ text_analyst.cpp
#define SORT_NONE     0
#define SORT_ALPHA    1
#define SORT_LEN_DEC  2
#define SORT_LEN_ASC  3
#define SORT_FREQ_ASC 4
#define SORT_FREQ_DEC 5
#define HASH_TABLE_SIZE 10007

// Cấu trúc để lưu trữ thống kê từ
typedef struct {
    char *word;
    int count;
} WordStats;

// Cấu trúc để lưu trữ kết quả phân tích
typedef struct {
    long char_count;
    long total_word_count;
    int unique_word_count;
    int line_count;
    WordStats* word_list;
    bool is_analyzed;
} AnalysisResult;

// Struct để lưu vị trí (bắt đầu, kết thúc) của một từ khóa được tìm thấy
struct MatchPosition {
    int start;
    int end;
};

// Struct để lưu thông tin của một dòng có chứa kết quả
struct FoundLine {
    int line_number;
    string line_content;
    vector<MatchPosition> matches; // Danh sách các vị trí của từ khóa trong dòng
};

// Cấu trúc để lưu trữ kết quả tìm kiếm
typedef struct {
    vector<FoundLine> found_lines;
    int total_matches;
    bool is_searched;
} SearchResult;

// Biến toàn cục để lưu trữ kết quả
static AnalysisResult g_analysis_result;
static SearchResult g_search_result;
static char g_status_message[512] = "Chưa chọn tệp nào";

// Khai báo các hàm
void error_callback(int error, const char* description);
void RenderFileSelector(char* filePathBuffer, size_t bufferSize);
void RenderAnalysisTab(char* selectedFile);
void RenderCompressTab(char* selectedFile);
void RenderFindTab(char* selectedFile, int theme_choice);
void RenderSettingsTab(int* selected_font_index, int* theme_choice);

void to_lowercase(char *str);
void to_lowercase_string(string& str);
int compare_alpha(const void *a, const void *b);
int compare_len_dec(const void *a, const void *b);
int compare_len_asc(const void *a, const void *b);
int compare_freq_asc(const void *a, const void *b);
int compare_freq_dec(const void *a, const void *b);
WordStats* ht_to_array(HashTable* table, int* count);
void perform_analysis_gui(const char* filename, int case_sensitive, int sort_mode);
void perform_find_gui(const char* filename, const char* keyword, int case_sensitive, int exact_match);
long long perform_compress_gui(const char* input_filename, const char* full_output_filename, CompressionAlgorithm algo);
long long perform_decompress_gui(const char* input_filename, const char* output_filename, CompressionAlgorithm algo);
// Các hàm phụ trợ
CompressionAlgorithm get_algo_from_string(const char* str);
const char* get_string_from_algo(CompressionAlgorithm algo);
CompressionAlgorithm get_algo_from_filename(const char* filename);
long long get_file_size(const char* filename);
void format_file_size(long long size, char* buffer, size_t buffer_size);

void cleanup_analysis_result();
void cleanup_search_result();

void error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main() {
    // Thiết lập mã hóa ký tự UTF-8 cho console
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    // --- 1. Khởi tạo GLFW và tạo cửa sổ ---
    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) {
        return 1;
    }
    
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Text Analyst GUI", NULL, NULL);
    if (window == NULL) {
        return 1;
    }
    
    // Khai báo mảng để chứa các ảnh icon
    GLFWimage images[3];
    int width, height, channels;

    // --- Tải ảnh 16x16 ---
    images[0].pixels = stbi_load("assets/app_icon16.png", &width, &height, &channels, 4);
    if (images[0].pixels) {
        images[0].width = width;
        images[0].height = height;
    } else {
        // Nếu không tải được, gán con trỏ là NULL để biết và bỏ qua
        images[0].pixels = NULL; 
        cerr << "Loi: Khong the tai tep icon 'assets/app_icon16.png'" << endl;
    }

    // --- Tải ảnh 32x32 ---
    images[1].pixels = stbi_load("assets/app_icon32.png", &width, &height, &channels, 4);
    if (images[1].pixels) {
        images[1].width = width;
        images[1].height = height;
    } else {
        images[1].pixels = NULL;
        cerr << "Loi: Khong the tai tep icon 'assets/app_icon32.png'" << endl;
    }

    // --- Tải ảnh 48x48 ---
    images[2].pixels = stbi_load("assets/app_icon48.png", &width, &height, &channels, 4);
    if (images[2].pixels) {
        images[2].width = width;
        images[2].height = height;
    } else {
        images[2].pixels = NULL;
        cerr << "Loi: Khong the tai tep icon 'assets/app_icon48.png'" << endl;
    }

    // --- Gán icon cho cửa sổ ---
    // Chỉ gán khi ít nhất một ảnh đã được tải thành công
    if (images[0].pixels || images[1].pixels || images[2].pixels) {
        // Lưu ý: GLFW đủ thông minh để bỏ qua các ảnh có con trỏ pixels là NULL.
        // Tuy nhiên, để chặt chẽ, bạn có thể tạo một mảng tạm chỉ chứa các ảnh hợp lệ.
        // Nhưng cách đơn giản là cứ truyền cả 3.
        glfwSetWindowIcon(window, 3, images);
    }

    // --- Giải phóng bộ nhớ cho TẤT CẢ các ảnh đã tải ---
    // Luôn kiểm tra con trỏ khác NULL trước khi giải phóng
    if (images[0].pixels) {
        stbi_image_free(images[0].pixels);
    }
    if (images[1].pixels) {
        stbi_image_free(images[1].pixels);
    }
    if (images[2].pixels) {
        stbi_image_free(images[2].pixels);
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // --- 2. Khởi tạo GLAD ---
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cerr << "Failed to initialize GLAD" << endl;
        return -1;
    }

    // --- 3. Khởi tạo Dear ImGui ---
    IMGUI_CHECKVERSION();
    CreateContext();
    ImGuiIO& io = GetIO(); (void)io;
    
    StyleColorsDark(); // Chọn theme màu tối
    const ImWchar* glyph_ranges = io.Fonts->GetGlyphRangesVietnamese();
    io.Fonts->AddFontDefault();

    ImFont* font_arial = io.Fonts->AddFontFromFileTTF("fonts/arial.ttf", 20.0f, NULL, glyph_ranges);
    ImFont* font_roboto = io.Fonts->AddFontFromFileTTF("fonts/Roboto-Regular.ttf", 20.0f, NULL, glyph_ranges);
    ImFont* font_noto_sans = io.Fonts->AddFontFromFileTTF("fonts/NotoSans-Regular.ttf", 20.0f, NULL, glyph_ranges);
    ImFont* font_merriweather = io.Fonts->AddFontFromFileTTF("fonts/Merriweather.ttf", 20.0f, NULL, glyph_ranges);

    // Setup các backend
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    
    // --- 4. Vòng lặp chính của ứng dụng (Game Loop) ---
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        //kiểm tra phím tắt để thoát
        if ((glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS) && glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        // Bắt đầu một frame ImGui mới
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        NewFrame();

        const ImGuiViewport* viewport = GetMainViewport();
        SetNextWindowPos(viewport->WorkPos);
        SetNextWindowSize(viewport->WorkSize);
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        // 3. Bắt đầu vẽ cửa sổ nền, bọc tất cả giao diện của bạn vào đây
        
        static int selected_font_index = 0;
        static int theme_choice = 0; // 0: Dark, 1: Light
        static char selectedFile[MAX_PATH] = "Chưa chọn tệp nào";

        // Con trỏ tới font hiện tại đang được dùng
        ImFont* current_font = nullptr;
        switch (selected_font_index) {
            case 0: current_font = font_arial; break;
            case 1: current_font = font_roboto; break;
            case 2: current_font = font_noto_sans; break;
            case 3: current_font = font_merriweather; break;
            default: current_font = GetFont(); break; // Font mặc định của ImGui
        }

        // --- Vẽ giao diện ImGui ở đây ---
        {
        // Đẩy font đã chọn vào stack để sử dụng
        PushFont(current_font);
        Begin("MainAppWindow", NULL, window_flags);

        // --- 2. Thanh Tab cho các chức năng chính ---
        PushStyleVar(ImGuiStyleVar_TabRounding, 4.0f);
        if (theme_choice == 0) { // Dark Theme
            PushStyleColor(ImGuiCol_Tab, ImVec4(0.15f, 0.16f, 0.18f, 1.0f)); 
            PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.3f, 0.32f, 0.35f, 1.0f));
            PushStyleColor(ImGuiCol_TabActive, ImVec4(0.25f, 0.55f, 0.8f, 1.0f)); 
        } else { // Light Theme (theme_choice == 1)
            PushStyleColor(ImGuiCol_Tab, ImVec4(0.88f, 0.88f, 0.88f, 1.0f)); // Màu tab không được chọn (sáng hơn)
            PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.75f, 0.8f, 0.9f, 1.0f)); // Màu tab khi hover (hơi xanh)
            PushStyleColor(ImGuiCol_TabActive, ImVec4(0.6f, 0.75f, 1.0f, 1.0f));  // Màu tab được chọn (xanh dương nhạt)
        }

        if (BeginTabBar("FunctionTabs")) {
            RenderAnalysisTab(selectedFile);
            RenderCompressTab(selectedFile);
            RenderFindTab(selectedFile, theme_choice);
            RenderSettingsTab(&selected_font_index, &theme_choice); // Truyền địa chỉ của biến
            // RenderAboutTab();

            EndTabBar();
        }
        
        // Khôi phục style
        PopStyleColor(3);
        PopStyleVar(1);
        
        End();

        // Lấy font ra khỏi stack để các cửa sổ khác (nếu có) không bị ảnh hưởng
        PopFont();
        }

        // --- 5. Rendering ---
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Vẽ giao diện ImGui
        Render();
        ImGui_ImplOpenGL3_RenderDrawData(GetDrawData());

        glfwSwapBuffers(window);
    }

    // --- 6. Dọn dẹp ---
    cleanup_analysis_result();
    cleanup_search_result();
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

// === CÁC HÀM HELPER VÀ LOGIC TÍCH HỢP ===

void to_lowercase(char *str) {
    for (; *str; ++str) *str = tolower(*str);
}

void to_lowercase_string(string& str) {
    for(auto& ch : str) {
        ch = tolower(ch);
    }
}

int compare_alpha(const void *a, const void *b) {
    WordStats *wa = (WordStats*)a;
    WordStats *wb = (WordStats*)b;
    return strcmp(wa->word, wb->word);
}

int compare_len_dec(const void *a, const void *b) {
    WordStats *wa = (WordStats*)a;
    WordStats *wb = (WordStats*)b;
    return strlen(wb->word) - strlen(wa->word);
}

int compare_len_asc(const void *a, const void *b) {
    WordStats *wa = (WordStats*)a;
    WordStats *wb = (WordStats*)b;
    return strlen(wa->word) - strlen(wb->word);
}

int compare_freq_asc(const void *a, const void *b) {
    WordStats *wa = (WordStats*)a;
    WordStats *wb = (WordStats*)b;
    return wb->count - wa->count;
}

int compare_freq_dec(const void *a, const void *b) {
    WordStats *wa = (WordStats*)a;
    WordStats *wb = (WordStats*)b;
    return wa->count - wb->count;
}

WordStats* ht_to_array(HashTable* table, int* count) {
    int list_count = 0;
    for (int i = 0; i < table->size; i++) {
        Entry* entry = table->entries[i];
        while (entry != NULL) {
            list_count++;
            entry = entry->next;
        }
    }
    *count = list_count;
    if (list_count == 0) return NULL;

    WordStats* list = (WordStats*)malloc(list_count * sizeof(WordStats));
    if (list == NULL) return NULL;

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

void cleanup_analysis_result() {
    if (g_analysis_result.word_list != NULL) {
        for (int i = 0; i < g_analysis_result.unique_word_count; i++) {
            free(g_analysis_result.word_list[i].word);
        }
        free(g_analysis_result.word_list);
    }
    memset(&g_analysis_result, 0, sizeof(AnalysisResult));
}

void cleanup_search_result() {
    g_search_result.found_lines.clear();
    g_search_result.total_matches = 0;
    g_search_result.is_searched = false;
}

void perform_analysis_gui(const char* filename, int case_sensitive, int sort_mode) {
    cleanup_analysis_result();
    snprintf(g_status_message, sizeof(g_status_message), "%s", "Đang phân tích...");

    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        snprintf(g_status_message, sizeof(g_status_message), "%s", "Lỗi: Không thể mở tệp");
        return;
    }

    HashTable *hash_table = create_table(HASH_TABLE_SIZE);
    if (hash_table == NULL) {
        fclose(file);
        snprintf(g_status_message, sizeof(g_status_message), "%s", "Lỗi: Không thể tạo bảng băm");
        return;
    }

    char line_buffer[2048];
    g_analysis_result.char_count = 0;
    g_analysis_result.line_count = 0;
    g_analysis_result.total_word_count = 0;

    while(fgets(line_buffer, sizeof(line_buffer), file) != NULL) {
        g_analysis_result.char_count += strlen(line_buffer);
        g_analysis_result.line_count++;

        char *token = strtok(line_buffer, " \t\n\r,.;:!?\"()");
        while (token != NULL) {
            g_analysis_result.total_word_count++;
            if (!case_sensitive) {
                to_lowercase(token);
            }
            ht_insert(hash_table, token);
            token = strtok(NULL, " \t\n\r,.;:!?\"()");
        }
    }

    g_analysis_result.word_list = ht_to_array(hash_table, &g_analysis_result.unique_word_count);
    free_table(hash_table);
    fclose(file);

    if (g_analysis_result.word_list != NULL) {
        if (sort_mode == SORT_ALPHA) 
            qsort(g_analysis_result.word_list, g_analysis_result.unique_word_count, sizeof(WordStats), compare_alpha);
        else if (sort_mode == SORT_LEN_DEC)
            qsort(g_analysis_result.word_list, g_analysis_result.unique_word_count, sizeof(WordStats), compare_len_dec);
        else if (sort_mode == SORT_LEN_ASC)
            qsort(g_analysis_result.word_list, g_analysis_result.unique_word_count, sizeof(WordStats), compare_len_asc);
        else if (sort_mode == SORT_FREQ_ASC)
            qsort(g_analysis_result.word_list, g_analysis_result.unique_word_count, sizeof(WordStats), compare_freq_asc);
        else if (sort_mode == SORT_FREQ_DEC)
            qsort(g_analysis_result.word_list, g_analysis_result.unique_word_count, sizeof(WordStats), compare_freq_dec);   
    }

    g_analysis_result.is_analyzed = true;
    snprintf(g_status_message, sizeof(g_status_message), "%s", "Phân tích hoàn thành");
}

void perform_find_gui(const char* filename, const char* keyword, int case_sensitive, int exact_match) {
    g_search_result = SearchResult();
    snprintf(g_status_message, sizeof(g_status_message), "%s", "Đang tìm kiếm...");

    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        snprintf(g_status_message, sizeof(g_status_message), "%s", "Lỗi: Không thể mở tệp");
        return;
    }

    string keyword_str(keyword);
    if (!case_sensitive) {to_lowercase_string(keyword_str);}
    size_t keyword_len = keyword_str.length();
    if (keyword_len == 0) {
        fclose(file);
        snprintf(g_status_message, sizeof(g_status_message), "%s", "Từ khóa không được để trống");
        return;
    }

    char line_buffer[4096];
    int line_number = 0;
    while (fgets(line_buffer, sizeof(line_buffer), file) != NULL) {
        line_number++;
        string original_line(line_buffer);
        string line_to_search = original_line;
        if (!case_sensitive) to_lowercase_string(line_to_search);

        FoundLine current_found_line;
        current_found_line.line_number = line_number;
        current_found_line.line_content = original_line;

        size_t start_pos = 0;
        while ((start_pos = line_to_search.find(keyword_str, start_pos)) != string::npos) {
            // Logic cho "Chỉ khớp toàn bộ từ"
            if (exact_match) {
                bool is_word_boundary_before = (start_pos == 0) || !isalnum(line_to_search[start_pos - 1]);
                bool is_word_boundary_after = (start_pos + keyword_len == line_to_search.length()) || !isalnum(line_to_search[start_pos + keyword_len]);
                if (!is_word_boundary_before || !is_word_boundary_after) {
                    start_pos += 1; // Không phải toàn bộ từ, tìm tiếp
                    continue;
                }
            }
            
            // Tìm thấy một kết quả hợp lệ, lưu lại vị trí
            current_found_line.matches.push_back({(int)start_pos, (int)(start_pos + keyword_len)});
            g_search_result.total_matches++;
            
            // Di chuyển vị trí tìm kiếm đến sau từ vừa tìm thấy
            start_pos += keyword_len;
        }

        // Nếu dòng này có chứa kết quả, thêm nó vào danh sách
        if (!current_found_line.matches.empty()) {
            g_search_result.found_lines.push_back(current_found_line);
        }
    }

    fclose(file);
    g_search_result.is_searched = true;

    if (g_search_result.total_matches > 0) {
        snprintf(g_status_message, sizeof(g_status_message), "Tìm thấy %d kết quả", g_search_result.total_matches);
    } else {
        snprintf(g_status_message, sizeof(g_status_message), "%s", "Không tìm thấy kết quả nào");
    }
}

CompressionAlgorithm get_algo_from_string(const char* str) {
    if (str == NULL) return ALG_UNKNOWN;
    if (strcmp(str, "rle") == 0) return ALG_RLE;
    if (strcmp(str, "huff") == 0) return ALG_HUFFMAN;
    return ALG_UNKNOWN;
}

const char* get_string_from_algo(CompressionAlgorithm algo) {
    switch(algo) {
        case ALG_RLE: return "rle";
        case ALG_HUFFMAN: return "huff";
        default: return "unknown";
    }
}

CompressionAlgorithm get_algo_from_filename(const char* filename) {
    if (filename == NULL) return ALG_UNKNOWN;
    const char* ext = strrchr(filename, '.');
    if (ext != NULL) {
        if (strcmp(ext, ".rle") == 0) return ALG_RLE;
        if (strcmp(ext, ".huff") == 0) return ALG_HUFFMAN;
    }
    return ALG_UNKNOWN;
}

long long perform_compress_gui(const char* input_filename, const char* full_output_filename, CompressionAlgorithm algo) {
    snprintf(g_status_message, sizeof(g_status_message), "%s", "Đang nén...");

    FILE* input_file = fopen(input_filename, "rb");
    if (input_file == NULL) {
        snprintf(g_status_message, sizeof(g_status_message), "%s", "Lỗi: Không thể mở tệp đầu vào");
        return -1;
    }

    FILE* output_file = fopen(full_output_filename, "wb");
    if (output_file == NULL) {
        fclose(input_file);
        snprintf(g_status_message, sizeof(g_status_message), "%s", "Lỗi: Không thể tạo tệp đầu ra");
        return -1;
    }

    int result = compress_file(input_file, output_file, algo);
    fclose(input_file);
    fclose(output_file);

    if (result == 0) {
        snprintf(g_status_message, sizeof(g_status_message), "Nén thành công: %s", full_output_filename);
        return get_file_size(full_output_filename);
    } else {
        snprintf(g_status_message, sizeof(g_status_message), "%s", "Lỗi: Nén thất bại");
        return -1;
    }
}

long long perform_decompress_gui(const char* input_filename, const char* output_filename, CompressionAlgorithm algo) {
    snprintf(g_status_message, sizeof(g_status_message), "%s", "Đang giải nén...");

    FILE* input_file = fopen(input_filename, "rb");
    if (input_file == NULL) {
        snprintf(g_status_message, sizeof(g_status_message), "%s, %s.", "Lỗi: Không thể mở tệp đầu vào", input_filename);
        return -1;
    }

    FILE* output_file = fopen(output_filename, "wb");    
    if (output_file == NULL) {
        fclose(input_file);
        snprintf(g_status_message, sizeof(g_status_message), "%s, %s.%s", "Lỗi: Không thể tạo tệp đầu ra", output_filename, "\nKiểm tra xem có thư mục trùng tên không?");
        return -1;
    }

    int result = decompress_file(input_file, output_file, algo);
    fclose(input_file);
    fclose(output_file);

    if (result == 0) {
        snprintf(g_status_message, sizeof(g_status_message), "Giải nén thành công: %s", output_filename);
        return get_file_size(output_filename);
    } else {
        snprintf(g_status_message, sizeof(g_status_message), "%s", "Lỗi: Giải nén thất bại");
        return -1;
    }
}

// Hàm này vẽ khu vực chọn/kéo thả file
void RenderFileSelector(char* filePathBuffer, size_t bufferSize) {
    Text(u8"Tệp đầu vào:");
    Separator();
    
    // Nút chọn file
    if (Button(u8"Chọn tệp...")) {
        OPENFILENAMEA ofn = {0};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL; // Nếu có handle cửa sổ GLFW thì tốt hơn
        ofn.lpstrFile = filePathBuffer;
        ofn.nMaxFile = bufferSize;
        // Chuỗi filter mới, mỗi cặp được ngăn cách bởi \0, và toàn bộ kết thúc bằng \0\0
        ofn.lpstrFilter = "All Supported Files\0*.txt;*.rle;*.huff\0"
                          "Text Files (*.txt)\0*.txt\0"
                          "RLE Compressed Files (*.rle)\0*.rle\0"
                          "Huffman Compressed Files (*.huff)\0*.huff\0"
                          "All Files (*.*)\0*.*\0";
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
        GetOpenFileNameA(&ofn);
    }
    SameLine();
    Text("%s", filePathBuffer);

    // Xử lý kéo/thả file
    if (BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = AcceptDragDropPayload("FILES")) {
            // ImGui backends cho GLFW sẽ cung cấp đường dẫn file dưới dạng này
            const char* path = *(const char**)payload->Data;
            strncpy(filePathBuffer, path, bufferSize - 1);
            filePathBuffer[bufferSize - 1] = '\0';
        }
        EndDragDropTarget();
    }
    Separator();
    Dummy(ImVec2(0.0f, 10.0f)); // Thêm một khoảng trống nhỏ
}

// Hàm vẽ giao diện cho Tab "Phân tích"
void RenderAnalysisTab(char* selectedFile) {
    if (BeginTabItem(u8"Phân tích")) {
        // --- Khu vực chọn tệp (toàn chiều rộng) ---
        RenderFileSelector(selectedFile, MAX_PATH);
        
        // Thêm một chút khoảng cách
        Dummy(ImVec2(0.0f, 10.0f));

        // --- Giao diện chính của tab (chia 2 cột) ---
        // ImVec2(350, 0): Rộng 350 pixel, cao tự động co giãn hết không gian còn lại
        BeginChild("LeftPane", ImVec2(350, 0), true, ImGuiWindowFlags_MenuBar);

        // --- Nội dung cột trái ---
        {
        BeginMenuBar();
        Text(u8"Tùy chọn");
        EndMenuBar();
        static bool case_sensitive = false;
        Checkbox(u8"Phân biệt hoa/thường", &case_sensitive);

        static int sort_mode = 0;
        Text(u8"Sắp xếp kết quả:");
        RadioButton(u8"Mặc định", &sort_mode, 0);
        RadioButton("A-Z", &sort_mode, 1);
        RadioButton(u8"Độ dài (giảm)", &sort_mode, 2);
        RadioButton(u8"Độ dài (tăng)", &sort_mode, 3);
        RadioButton(u8"tần suất (giảm)", &sort_mode, 4);
        RadioButton(u8"tần suất (tăng)", &sort_mode, 5);

        Dummy(ImVec2(0.0f, 20.0f));

        if (Button(u8"Bắt đầu phân tích", ImVec2(-FLT_MIN, 40))) {
            if (strlen(selectedFile) > 0 && strcmp(selectedFile, "Chưa chọn tệp nào") != 0) {
                perform_analysis_gui(selectedFile, case_sensitive, sort_mode);
            } else {
                snprintf(g_status_message, sizeof(g_status_message), "%s", "Vui lòng chọn tệp trước");
            }
        }

        Separator();
        TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), u8"Trạng thái: %s", g_status_message);

        EndChild();
        }         

        SameLine();

        // --- Cột phải: Kết quả ---
        {
        BeginChild("RightPane", ImVec2(0, 0), true, ImGuiWindowFlags_MenuBar);

        BeginMenuBar();
        Text(u8"Kết quả phân tích");
        EndMenuBar();
        
        if (g_analysis_result.is_analyzed) {
            Text("--- Thống kê cơ bản ---");
            Text("Số ký tự: %ld", g_analysis_result.char_count);
            Text("Số từ (tổng): %ld", g_analysis_result.total_word_count);
            Text("Số từ (duy nhất): %d", g_analysis_result.unique_word_count);
            Text("Số dòng: %d", g_analysis_result.line_count);
            Separator();
            
            if (g_analysis_result.word_list != NULL && g_analysis_result.unique_word_count > 0) {
                Text("--- Danh sách từ ---");
                float table_height = 200.0f; // Chiều cao cố định cho bảng 
                if (BeginTable("WordList", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, table_height))) {
                    TableSetupColumn("Từ");
                    TableSetupColumn("Số lần");
                    TableHeadersRow();
                    
                    for (int i = 0; i < g_analysis_result.unique_word_count; i++) {
                        TableNextRow();
                        TableSetColumnIndex(0); 
                        Text("%s", g_analysis_result.word_list[i].word);
                        TableSetColumnIndex(1); 
                        Text("%d", g_analysis_result.word_list[i].count);
                    }
                    EndTable();
                }
                
                // Phân tích chi tiết
                Separator();
                Text("--- Phân tích chi tiết ---");
                
                if (g_analysis_result.unique_word_count > 0) {
                    // Tìm từ dài nhất và ngắn nhất
                    size_t max_len = strlen(g_analysis_result.word_list[0].word);
                    size_t min_len = max_len;
                    int max_freq = g_analysis_result.word_list[0].count;
                    int min_freq = max_freq;
                    
                    for (int i = 0; i < g_analysis_result.unique_word_count; i++) {
                        size_t current_len = strlen(g_analysis_result.word_list[i].word);
                        int current_freq = g_analysis_result.word_list[i].count;
                        if (current_len > max_len) max_len = current_len;
                        if (current_len < min_len) min_len = current_len;
                        if (current_freq > max_freq) max_freq = current_freq;
                        if (current_freq < min_freq) min_freq = current_freq;
                    }
                    
                    Text("Từ dài nhất (%zu ký tự):", max_len);
                    for (int i = 0; i < g_analysis_result.unique_word_count; i++) {
                        if (strlen(g_analysis_result.word_list[i].word) == max_len) {
                            BulletText("%s", g_analysis_result.word_list[i].word);
                        }
                    }
                    
                    Text("Từ ngắn nhất (%zu ký tự):", min_len);
                    for (int i = 0; i < g_analysis_result.unique_word_count; i++) {
                        if (strlen(g_analysis_result.word_list[i].word) == min_len) {
                            BulletText("%s", g_analysis_result.word_list[i].word);
                        }
                    }
                    
                    Text("Từ xuất hiện nhiều nhất (%d lần):", max_freq);
                    for (int i = 0; i < g_analysis_result.unique_word_count; i++) {
                        if (g_analysis_result.word_list[i].count == max_freq) {
                            BulletText("%s", g_analysis_result.word_list[i].word);
                        }
                    }
                }
            }
        } else {
            TextWrapped(u8"Chưa có kết quả phân tích. Vui lòng chọn tệp và nhấn 'Bắt đầu phân tích'.");
        }
        
        // EndChild();
        }

        EndChild();
        EndTabItem();
    }
}   

void find_unique_output_filename(char* buffer, size_t buffer_size) {
    int suffix = 0;
    const char* extensions[] = {".rle", ".huff"}; // Các đuôi file cần kiểm tra

    while (suffix < 1000) { // Giới hạn 1000 lần thử để tránh lặp vô tận
        char base_name[MAX_PATH];
        if (suffix == 0) {
            snprintf(base_name, sizeof(base_name), "output");
        } else {
            snprintf(base_name, sizeof(base_name), "output%d", suffix);
        }

        bool name_exists = false;
        for (int i = 0; i < 2; ++i) {
            char full_name[MAX_PATH];
            snprintf(full_name, sizeof(full_name), "%s%s", base_name, extensions[i]);
            
            FILE* f = fopen(full_name, "r");
            if (f) {
                fclose(f);
                name_exists = true;
                break; // Tìm thấy file trùng, thử suffix tiếp theo
            }
        }

        if (!name_exists) {
            strncpy(buffer, base_name, buffer_size - 1);
            buffer[buffer_size - 1] = '\0';
            return; // Tìm thấy tên hợp lệ, thoát hàm
        }

        suffix++;
    }

    // Nếu không tìm được sau 1000 lần, dùng tên mặc định
    strncpy(buffer, "output_new", buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
}

// Lấy kích thước của một tệp theo đường dẫn.
long long get_file_size(const char* filename) {
    if (!filename || filename[0] == '\0' || strcmp(filename, "Chưa chọn tệp nào") == 0) {
        return -1;
    }

    FILE* p_file = fopen(filename, "rb");
    if (p_file == NULL) {
        return -1;
    }

    fseek(p_file, 0, SEEK_END);
    long long size = ftell(p_file);
    fclose(p_file);
    return size;
}

// Định dạng kích thước tệp (byte) thành chuỗi dễ đọc (KB, MB, GB).
void format_file_size(long long size, char* buffer, size_t buffer_size) {
    if (size < 0) {
        snprintf(buffer, buffer_size, "N/A");
        return;
    }
    if (size < 1024) {
        snprintf(buffer, buffer_size, "%lld B", size);
    } else if (size < 1024 * 1024) {
        snprintf(buffer, buffer_size, "%.2f KB", (double)size / 1024.0);
    } else if (size < 1024 * 1024 * 1024) {
        snprintf(buffer, buffer_size, "%.2f MB", (double)size / (1024.0 * 1024.0));
    } else {
        snprintf(buffer, buffer_size, "%.2f GB", (double)size / (1024.0 * 1024.0 * 1024.0));
    }
}

// Hàm vẽ giao diện cho Tab "Nén / Giải nén"
void RenderCompressTab(char* selectedFile) {
    if (BeginTabItem(u8"Nén / Giải nén")) {
        RenderFileSelector(selectedFile, MAX_PATH);

        static int operation = 0;
        static long long input_file_size = -1;
        static long long output_file_size = -1;

        //cập nhật kích thước tệp đầu vào mỗi khi tệp được chọn
        input_file_size = get_file_size(selectedFile);

        if (RadioButton(u8"Nén tệp", &operation, 0)) { output_file_size = -1; }
        SameLine();
        if (RadioButton(u8"Giải nén tệp", &operation, 1)) { output_file_size = -1; }

        static int compress_algo = 0;
        static int decompress_mode = 2; // Mặc định là Tự động
        
        Text(u8"Chọn thuật toán:");
        if (operation == 0) { // Giao diện khi Nén
            RadioButton("RLE##compress", &compress_algo, 0); SameLine();
            RadioButton("Huffman##compress", &compress_algo, 1);
        } else { // Giao diện khi Giải nén
            RadioButton("RLE##decompress", &decompress_mode, 0); SameLine();
            RadioButton("Huffman##decompress", &decompress_mode, 1); SameLine();
            RadioButton(u8"Tự động nhận diện", &decompress_mode, 2);
        }

        static char output_path[MAX_PATH] = "output"; // Tên tệp đầu ra mặc định
        InputText(u8"Tên tệp đầu ra", output_path, MAX_PATH);

        // --- THÊM TÙY CHỌN GHI ĐÈ ---
        static bool allow_overwrite = false;
        Checkbox(u8"Cho phép ghi đè tệp đã tồn tại", &allow_overwrite);

        Dummy(ImVec2(0.0f, 10.0f));

        if (Button(operation == 0 ? u8"Bắt đầu nén" : u8"Bắt đầu giải nén", ImVec2(150, 40))) {
            if (strlen(selectedFile) > 0 && strcmp(selectedFile, "Chưa chọn tệp nào") != 0) {
                if (strlen(output_path) > 0) {
                    
                    // --- LOGIC XỬ LÝ GHI ĐÈ ---
                    char final_output_name[MAX_PATH];
                    CompressionAlgorithm algo_to_use;

                    if (operation == 0) { // Nén
                        algo_to_use = (compress_algo == 0) ? ALG_RLE : ALG_HUFFMAN;
                        const char* extension = get_string_from_algo(algo_to_use);
                        snprintf(final_output_name, sizeof(final_output_name), "%s.%s", output_path, extension);
                    } else { // Giải nén
                        snprintf(final_output_name, sizeof(final_output_name), "%s.txt", output_path);
                    }

                    // Kiểm tra ghi đè chỉ khi không cho phép
                    if (!allow_overwrite) {
                        FILE* f_test = fopen(final_output_name, "r");
                        if (f_test) {
                            fclose(f_test);
                            snprintf(g_status_message, sizeof(g_status_message), "Lỗi: Tệp '%s' đã tồn tại. Tick vào ô cho phép ghi đè.", final_output_name);
                            goto end_compress_logic; // Nhảy đến cuối khối lệnh
                        }
                    }

                    // --- Logic nén/giải nén chính ---
                    if (operation == 0) { // Nén
                        output_file_size = perform_compress_gui(selectedFile, final_output_name, algo_to_use);
                    } else { // Giải nén
                        if (decompress_mode == 2) {
                            algo_to_use = get_algo_from_filename(selectedFile);
                        } else {
                            algo_to_use = (decompress_mode == 0) ? ALG_RLE : ALG_HUFFMAN;
                        }

                        if (algo_to_use != ALG_UNKNOWN) {
                            output_file_size = perform_decompress_gui(selectedFile, final_output_name, algo_to_use);
                        } else {
                            snprintf(g_status_message, sizeof(g_status_message), "Lỗi: Không nhận diện được định dạng tệp nén");
                            output_file_size = -1;
                        }
                    }

                } else { /* Lỗi thiếu tên tệp đầu ra */ 
                    snprintf(g_status_message, sizeof(g_status_message), "%s", "Vui lòng nhập tên tệp đầu ra");
                }
            } else { /* Lỗi thiếu tệp đầu vào */ 
                snprintf(g_status_message, sizeof(g_status_message), "%s", "Vui lòng chọn tệp trước");}
        }
        end_compress_logic:;
        // --- KHU VỰC HIỂN THỊ THÔNG TIN KÍCH THƯỚC MỚI ---
        Separator();
        Text(u8"Thông tin kích thước:");
        
        char input_size_str[32];
        char output_size_str[32];
        format_file_size(input_file_size, input_size_str, sizeof(input_size_str));
        format_file_size(output_file_size, output_size_str, sizeof(output_size_str));

        Text(u8"Kích thước tệp đầu vào: %s", input_size_str);

        if (operation == 0) { // Chế độ nén
            Text(u8"Kích thước tệp đã nén:  %s", output_size_str);
            if (input_file_size > 0 && output_file_size >= 0) {
                 double ratio = ((double)(input_file_size - output_file_size) / input_file_size) * 100.0;
                 Text(u8"Tỷ lệ nén: %.2f%%", ratio);
            }
        } else { // Chế độ giải nén
            Text(u8"Kích thước tệp đã giải nén: %s", output_size_str);
        }
                        

        // Hiển thị trạng thái
        Separator();
        TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), u8"Trạng thái: %s", g_status_message);

        // Thông tin hướng dẫn
        Separator();
        TextWrapped(u8"Ghi chú:");
        BulletText(u8"Khi nén, phần mở rộng sẽ được tự động thêm (.rle hoặc .huff)");
        // BulletText(u8"Khi giải nén, thuật toán sẽ được tự động nhận diện từ phần mở rộng");
        BulletText(u8"RLE phù hợp cho dữ liệu có nhiều ký tự lặp lại");
        BulletText(u8"Huffman phù hợp cho văn bản thông thường");

        EndTabItem();
    }
}

// Hàm vẽ giao diện cho Tab "Tìm kiếm"
void RenderFindTab(char* selectedFile, int theme_choice) {
    if (BeginTabItem(u8"Tìm kiếm")) {
        RenderFileSelector(selectedFile, MAX_PATH);

        static char keyword_input[256] = "";
        InputText(u8"Nhập từ khóa", keyword_input, 256);

        static bool find_exact_match = false;
        Checkbox(u8"Khớp chính xác toàn bộ từ", &find_exact_match);

        static bool case_sensitive_find = false;
        Checkbox(u8"Phân biệt hoa/thường", &case_sensitive_find);

        if (Button(u8"Tìm kiếm", ImVec2(120, 0))) {
            if (strlen(selectedFile) > 0 && strcmp(selectedFile, "Chưa chọn tệp nào") != 0) {
                if (strlen(keyword_input) > 0) {
                    perform_find_gui(selectedFile, keyword_input, case_sensitive_find, find_exact_match);
                } else {
                    snprintf(g_status_message, sizeof(g_status_message), "%s", "Vui lòng nhập từ khóa");
                }
            } else {
                snprintf(g_status_message, sizeof(g_status_message), "%s", "Vui lòng chọn tệp trước");
            }
        }

        Separator();
        TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), u8"Trạng thái: %s", g_status_message);
        Separator();
        Text(u8"Kết quả tìm kiếm:");

        // Bắt đầu vùng chứa kết quả có thể cuộn
        if (BeginChild("FindResultsRegion", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar)) {
            
            // --- LOGIC HIỂN THỊ ĐÃ ĐƯỢC SỬA LẠI ĐỂ TRÁNH THOÁT SỚM ---
            if (!g_search_result.is_searched) {
                Text(u8"Chưa có kết quả tìm kiếm.");
            } else if (g_search_result.found_lines.empty()) {
                Text(u8"Không tìm thấy từ khóa trong tệp.");
            } else {
                // Chỉ hiển thị kết quả nếu có
                ImVec4 highlight_color;
                if(theme_choice == 0) { // Dark theme
                    highlight_color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // Vàng sáng
                } else if(theme_choice == 1) { // Light theme
                    highlight_color = ImVec4(0.39f, 0.58f, 0.93f, 1.0f);
                }
                
                for (const auto& found_line : g_search_result.found_lines) {
                    TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Dòng %d:", found_line.line_number);
                    SameLine();

                    const char* line_start = found_line.line_content.c_str();
                    int last_pos = 0;

                    for (const auto& match : found_line.matches) {
                        if (match.start > last_pos) {
                            TextUnformatted(line_start + last_pos, line_start + match.start);
                            SameLine(0, 0);
                        }

                        PushStyleColor(ImGuiCol_Text, highlight_color);
                        TextUnformatted(line_start + match.start, line_start + match.end);
                        PopStyleColor();
                        SameLine(0, 0);
                        
                        last_pos = match.end;
                    }

                    if (last_pos < (int)found_line.line_content.length()) {
                        TextUnformatted(line_start + last_pos);
                    } else {
                        NewLine();
                    }
                }
            }
        }
        // Đóng vùng chứa kết quả
        EndChild();

        // Đóng tab item
        EndTabItem();
    }
}

// Lưu ý: selected_font_index là con trỏ
void RenderSettingsTab(int* selected_font_index, int* theme_choice) {
    if (BeginTabItem(u8"Cài đặt")) {
        Text(u8"Tùy chỉnh giao diện và font chữ.");

        Separator(); // Thêm một đường kẻ ngang cho đẹp
        
        // --- BẮT ĐẦU: LOGIC CHỌN THEME ---
        Text(u8"Giao diện (Theme):");
        
        // Khi người dùng click vào một RadioButton, giá trị của *theme_choice sẽ được cập nhật.
        // Chúng ta cũng gọi hàm đổi theme tương ứng.
        if (RadioButton("Dark", theme_choice, 0)) {
            StyleColorsDark();
        }
        SameLine(); // Đặt nút tiếp theo trên cùng một dòng
        if (RadioButton("Light", theme_choice, 1)) {
            StyleColorsLight();
        }
        Separator();

        Text(u8"Chọn một font chữ để áp dụng:");
        RadioButton("Arial", selected_font_index, 0);
        RadioButton("Roboto", selected_font_index, 1);
        RadioButton("Noto Sans", selected_font_index, 2);
        RadioButton("Merriweather", selected_font_index, 3);
        
        Separator();

        // if(Button("Hiển thị cửa sổ Demo của ImGui")) {
        //     // Biến này nên được khai báo ở phạm vi toàn cục hoặc trong main
        // // static bool show_demo_window = true;
        // // show_demo_window = true;
        // }

        // Thêm nút "Giới thiệu" ở đây
        if (Button(u8"Giới thiệu về ứng dụng")) {
            OpenPopup(u8"Giới thiệu");
        }

        // Cửa sổ popup "Giới thiệu"
        if (BeginPopupModal(u8"Giới thiệu", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            // --- HIỂN THỊ THÔNG TIN PHIÊN BẢN ---
            Text(u8"Phiên bản ứng dụng: %s", APP_VERSION_STRING);
            Text(u8"Ngày build: %s %s", __DATE__, __TIME__);

            // Dummy(ImVec2(0.0f, 20.0f)); // Tạo khoảng trống

            Separator();

            TextWrapped(u8"Đây là một ứng dụng học tập,");
            TextWrapped(u8"được xây dựng bằng C/C++ và thư viện đồ họa Dear ImGui.");
            
            Dummy(ImVec2(0.0f, 10.0f));
            
            Text(u8"Các tính năng chính:");
            BulletText(u8"Phân tích văn bản: Thống kê chi tiết về từ, ký tự, dòng.");
            BulletText(u8"Nén và giải nén: Hỗ trợ các thuật toán RLE và Huffman.");
            BulletText(u8"Tìm kiếm nâng cao: Tìm từ hoặc chuỗi con với nhiều tùy chọn.");
            BulletText(u8"Tùy chỉnh giao diện: Thay đổi theme và font chữ.");

            Dummy(ImVec2(0.0f, 10.0f));

            Text(u8"Tác giả: EnderirT");
            Text(u8"GitHub: https://github.com/triender/C-Text-Analyzer");

            Dummy(ImVec2(0.0f, 20.0f));
            Separator();
            
            Text(u8"Cảm ơn bạn đã sử dụng ứng dụng!");

            Separator();

            if (Button("OK", ImVec2(120, 0))) {
                CloseCurrentPopup();
            }
            EndPopup();
        }

        EndTabItem();
    }
}
