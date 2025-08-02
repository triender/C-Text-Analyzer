#ifndef COMPRESS_H
#define COMPRESS_H

#include <stdio.h>
#include <stdint.h> // For fixed-width integers

#define MAX_TREE_HT 256
#define HUFFMAN_MAGIC "HUFF"

/**
 * @brief Enum để định danh các thuật toán nén.
 * Sử dụng enum giúp mã nguồn dễ đọc và an toàn hơn so với dùng số nguyên.
 */
typedef enum {
    ALG_RLE,      // Thuật toán Run-Length Encoding
    ALG_HUFFMAN,  // Thuật toán Huffman Coding
    ALG_UNKNOWN
} CompressionAlgorithm;

/**
 * @brief Cấu trúc để lưu trữ tần suất của một ký hiệu cho header Huffman.
 */
typedef struct {
    uint8_t symbol;
    uint32_t frequency;
} SymbolFreq;

/**
 * @brief Header cho file Huffman đã được tối ưu.
 * Cấu trúc này được đóng gói (packed) để đảm bảo không có byte đệm,
 * giúp việc đọc/ghi file được chính xác.
 */
#pragma pack(push, 1)
typedef struct {
    char magic[4];          // "Số ma thuật" để nhận diện loại file (ví dụ: "HUFF")
    uint8_t num_symbols;    // Số lượng ký hiệu duy nhất trong bảng tần suất.
    uint64_t original_size; // Kích thước file gốc (trước khi nén).
} HuffmanHeader;
#pragma pack(pop)


/**
 * @brief Nén một tệp sử dụng thuật toán được chỉ định.
 * @param input Con trỏ đến tệp đầu vào đã mở.
 * @param output Con trỏ đến tệp đầu ra đã mở.
 * @param algo Thuật toán nén để sử dụng (ALG_RLE hoặc ALG_HUFFMAN).
 * @return int Trả về 0 nếu thành công, -1 nếu thất bại.
 */
int compress_file(FILE* input, FILE* output, CompressionAlgorithm algo);

/**
 * @brief Giải nén một tệp.
 * @param input Con trỏ đến tệp cần giải nén đã mở.
 * @param output Con trỏ đến tệp đầu ra đã mở.
 * @param algo Thuật toán đã được dùng để nén (ALG_RLE hoặc ALG_HUFFMAN).
 * @return int Trả về 0 nếu thành công, -1 nếu thất bại.
 */
int decompress_file(FILE* input, FILE* output, CompressionAlgorithm algo);

#endif // COMPRESS_H