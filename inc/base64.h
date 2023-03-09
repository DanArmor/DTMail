#ifndef _INC_BASE64_H
#define _INC_BASE64_H

#include "util.h"

/// @brief Возвращает данные, закодированные в Base64
/// @details Немного модифицированная реализация https://stackoverflow.com/a/6782480
/// @param data Кодируемые данные
/// @param input_length Длина входных данных
/// @param output_length Длина выходных данныъ
/// @return Возвращает указатель на закодированные данные в динамической памяти
char *Base64Encode(const unsigned char *data,
                    int input_length,
                    int *output_length);

/// @brief Возвращает таблицу декодирования в динамической памяти
/// @details Немного модифицированная реализация https://stackoverflow.com/a/6782480
/// @return Возвращает таблицу декодирования в динамической памяти
char *Base64BuildDecodeTable();

/// @brief Возвращает данные, декодированные из Base64
/// @details Немного модифицированная реализация https://stackoverflow.com/a/6782480
/// @param data Декодируемые данные
/// @param input_length Длина входных данных
/// @param output_length Длина выходных данныъ
/// @return Возвращает указатель на декодированные данные в динамической памяти
char *Base64Decode(const char *data,
                             int input_length,
                             int *output_length);
#endif