#ifndef STRUCTPARSER_H
#define STRUCTPARSER_H

#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <sstream>
#include <map>
#include <regex>
#include <memory>
#include <cstring>
#include <algorithm>

class BitFieldStructParser
{
    friend inline std::string FieldType(const std::string &StructText, const std::string &FieldName);
private:
    struct FieldInfo
    {
        std::string type;
        std::string name;
        int bitWidth;
        int byteOffset;
        int bitOffset;
        size_t size;
        bool isBitField;
        bool isAnonymous;
    };

    struct StructInfo
    {
        std::string name;
        std::vector<FieldInfo> fields;
        size_t totalSize;
    };

    static std::map<std::string, size_t> typeSizes;

    static void initializeTypeSizes()
    {
        if (!typeSizes.empty()) return;

        typeSizes["uint8_t"] = 1;
        typeSizes["int8_t"] = 1;
        typeSizes["char"] = 1;
        typeSizes["uint16_t"] = 2;
        typeSizes["int16_t"] = 2;
        typeSizes["short"] = 2;
        typeSizes["uint32_t"] = 4;
        typeSizes["int32_t"] = 4;
        typeSizes["float"] = 4;
        typeSizes["uint64_t"] = 8;
        typeSizes["int64_t"] = 8;
        typeSizes["double"] = 8;
    }

    static size_t getTypeSize(const std::string& type)
    {
        initializeTypeSizes();
        auto it = typeSizes.find(type);
        if (it != typeSizes.end())
        {
            return it->second;
        }
        throw std::invalid_argument("Unknown type: " + type);
    }

    static std::string trim(const std::string& str)
    {
        size_t start = str.find_first_not_of(" \t\n\r");
        size_t end = str.find_last_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        return str.substr(start, end - start + 1);
    }

    static std::string removeCommentsAndExtraSpaces(const std::string& str)
    {
        std::string result;
        bool inLineComment = false;
        bool inBlockComment = false;

        for (size_t i = 0; i < str.length(); ++i)
        {
            if (inLineComment)
            {
                if (str[i] == '\n')
                {
                    inLineComment = false;
                    result += ' '; // Заменяем комментарий на пробел
                }
                continue;
            }

            if (inBlockComment)
            {
                if (str[i] == '*' && i + 1 < str.length() && str[i + 1] == '/')
                {
                    inBlockComment = false;
                    ++i; // Пропускаем '/'
                }
                continue;
            }

            // Проверяем начало комментариев
            if (str[i] == '/' && i + 1 < str.length())
            {
                if (str[i + 1] == '/')
                {
                    inLineComment = true;
                    ++i; // Пропускаем второй '/'
                    continue;
                }
                else if (str[i + 1] == '*')
                {
                    inBlockComment = true;
                    ++i; // Пропускаем '*'
                    continue;
                }
            }

            // Заменяем все пробельные символы на обычные пробелы
            if (std::isspace(static_cast<unsigned char>(str[i])))
            {
                if (result.empty() || result.back() != ' ')
                {
                    result += ' ';
                }
            }
            else
            {
                result += str[i];
            }
        }

        return trim(result);
    }

    static std::vector<std::string> splitFields(const std::string& content)
    {
        std::vector<std::string> fields;
        std::string currentField;
        int braceLevel = 0;

        for (char c : content)
        {
            if (c == '{')
            {
                braceLevel++;
            }
            else if (c == '}')
            {
                braceLevel--;
            }

            if (c == ';' && braceLevel == 0)
            {
                if (!currentField.empty())
                {
                    fields.push_back(trim(currentField));
                    currentField.clear();
                }
            }
            else
            {
                currentField += c;
            }
        }

        // Добавляем последнее поле, если оно есть
        if (!currentField.empty())
        {
            fields.push_back(trim(currentField));
        }

        return fields;
    }

public:
    static StructInfo parseStruct(const std::string& structText)
    {
        StructInfo structInfo;

        // Предварительная обработка текста
        std::string processedText = removeCommentsAndExtraSpaces(structText);

        // Извлекаем имя структуры (игнорируем переводы строк)
        std::regex structNameRegex(R"(struct\s+(\w+)\s*\{)");
        std::smatch match;
        if (std::regex_search(processedText, match, structNameRegex))
        {
            structInfo.name = match[1];
        }

        // Извлекаем содержимое между {} (с учетом многострочности)
        std::regex contentRegex(R"(\{(.*)\})");
        if (std::regex_search(processedText, match, contentRegex))
        {
            std::string content = match[1];

            // Разбиваем на поля
            std::vector<std::string> fields = splitFields(content);

            int currentByteOffset = 0;
            int currentBitOffset = 0;

            for (const auto& fieldLine : fields)
            {
                if (fieldLine.empty()) continue;

                FieldInfo field;

                // Упрощаем строку поля - убираем лишние пробелы
                std::string simplifiedLine = std::regex_replace(fieldLine, std::regex("\\s+"), " ");
                simplifiedLine = trim(simplifiedLine);

                // Проверяем на битовое поле
                std::regex bitFieldRegex(R"((\w+)\s+(\w*)\s*:\s*(\d+))");
                std::regex normalFieldRegex(R"((\w+)\s+(\w+))");
                std::regex anonymousBitFieldRegex(R"((\w+)\s*:\s*(\d+))");
                std::regex normalFieldWithMultipleSpaces(R"((\w+)\s+(\w+).*)");

                if (std::regex_match(simplifiedLine, match, bitFieldRegex))
                {
                    // Битовое поле с именем
                    field.type = match[1];
                    field.name = match[2];
                    field.bitWidth = std::stoi(match[3]);
                    field.isBitField = true;
                    field.isAnonymous = false;
                }
                else if (std::regex_match(simplifiedLine, match, anonymousBitFieldRegex))
                {
                    // Анонимное битовое поле
                    field.type = match[1];
                    field.name = "";
                    field.bitWidth = std::stoi(match[2]);
                    field.isBitField = true;
                    field.isAnonymous = true;
                }
                else if (std::regex_match(simplifiedLine, match, normalFieldRegex))
                {
                    // Обычное поле
                    field.type = match[1];
                    field.name = match[2];
                    field.bitWidth = 0;
                    field.isBitField = false;
                    field.isAnonymous = false;
                }
                else if (std::regex_match(simplifiedLine, match, normalFieldWithMultipleSpaces))
                {
                    // Обычное поле (более гибкое регулярное выражение)
                    field.type = match[1];
                    field.name = match[2];
                    field.bitWidth = 0;
                    field.isBitField = false;
                    field.isAnonymous = false;
                }
                else
                {
                    // Пропускаем непонятные строки, но выводим предупреждение
                    std::cerr << "Предупреждение: не удалось разобрать поле: '" << simplifiedLine << "'" << std::endl;
                    continue;
                }

                if (!field.isBitField)
                {
                    // Обычное поле - занимает полный размер типа
                    field.size = getTypeSize(field.type);
                    field.byteOffset = currentByteOffset;
                    field.bitOffset = 0;
                    currentByteOffset += field.size;
                    currentBitOffset = 0;
                }
                else
                {
                    // Битовое поле
                    size_t typeSize = getTypeSize(field.type);

                    if (currentBitOffset + field.bitWidth > typeSize * 8)
                    {
                        // Не помещается в текущую единицу - переходим к следующей
                        currentByteOffset += typeSize;
                        currentBitOffset = 0;
                    }

                    field.size = typeSize;
                    field.byteOffset = currentByteOffset;
                    field.bitOffset = currentBitOffset;
                    currentBitOffset += field.bitWidth;

                    if (currentBitOffset >= typeSize * 8)
                    {
                        currentByteOffset += typeSize;
                        currentBitOffset = 0;
                    }
                }

                if (!field.isAnonymous)
                {
                    structInfo.fields.push_back(field);
                }
            }

            structInfo.totalSize = currentByteOffset + (currentBitOffset > 0 ? getTypeSize("uint8_t") : 0);
        }
        else
        {
            throw std::invalid_argument("Не удалось найти содержимое структуры между {}");
        }

        return structInfo;
    }

    // Функция определения размера структуры в байтах
    static size_t struct_sizeof(const std::string& structText)
    {
        StructInfo structInfo = parseStruct(structText);
        return structInfo.totalSize;
    }

    //template<typename T>
    static void struct_write(const std::string& structText, const std::string& fieldName, void *value, char* buffer)
    {
        StructInfo structInfo = parseStruct(structText);

        for (const auto& field : structInfo.fields)
        {
            if (field.name == fieldName)
            {
                if (!field.isBitField)
                {
                    // Обычное поле
//                    if (sizeof(T) != field.size)
//                    {
//                        throw std::invalid_argument("Size mismatch for field " + fieldName);
//                    }
                    std::memcpy(buffer + field.byteOffset, value, field.size);
                }
                else
                {
                    // Битовое поле
                    uint64_t mask = (1ULL << field.bitWidth) - 1;
                    uint64_t fieldValue = static_cast<uint64_t>(*(uint64_t *)value) & mask;

                    // Читаем текущее значение
                    uint64_t currentValue = 0;
                    std::memcpy(&currentValue, buffer + field.byteOffset, field.size);

                    // Очищаем биты поля и устанавливаем новые значения
                    uint64_t clearMask = ~(mask << field.bitOffset);
                    currentValue &= clearMask;
                    currentValue |= (fieldValue << field.bitOffset);

                    // Записываем обратно
                    std::memcpy(buffer + field.byteOffset, &currentValue, field.size);
                }
                return;
            }
        }

        throw std::invalid_argument("Field not found: " + fieldName);
    }

    template<typename T>
    static T struct_read(const std::string& structText, const std::string& fieldName, const char* buffer)
    {
        StructInfo structInfo = parseStruct(structText);

        for (const auto& field : structInfo.fields)
        {
            if (field.name == fieldName)
            {
                if (!field.isBitField)
                {
                    // Обычное поле
                    T value;
                    std::memcpy(&value, buffer + field.byteOffset, field.size);
                    return value;
                }
                else
                {
                    // Битовое поле
                    uint64_t containerValue = 0;
                    std::memcpy(&containerValue, buffer + field.byteOffset, field.size);

                    uint64_t mask = (1ULL << field.bitWidth) - 1;
                    uint64_t fieldValue = (containerValue >> field.bitOffset) & mask;

                    return static_cast<T>(fieldValue);
                }
            }
        }

        throw std::invalid_argument("Field not found: " + fieldName);
    }

    // Вспомогательная функция для вывода информации о структуре
    static void printStructInfo(const std::string& structText)
    {
        StructInfo structInfo = parseStruct(structText);

        std::cout << "Struct: " << structInfo.name << " (total size: " << structInfo.totalSize << " bytes)" << std::endl;
        for (const auto& field : structInfo.fields)
        {
            std::cout << "  " << field.type << " " << field.name;
            if (field.isBitField)
            {
                std::cout << " : " << field.bitWidth;
            }
            std::cout << " | offset: " << field.byteOffset;
            if (field.isBitField)
            {
                std::cout << ", bit offset: " << field.bitOffset;
            }
            std::cout << ", size: " << field.size << " bytes" << std::endl;
        }
    }
};

// Инициализация статического члена
std::map<std::string, size_t> BitFieldStructParser::typeSizes;

inline std::string FieldType(const std::string &StructText, const std::string &FieldName)
{
  BitFieldStructParser::StructInfo StructInfo = BitFieldStructParser::parseStruct(StructText);
  for (int i=0; i<(int)StructInfo.fields.size(); i++)
    if (StructInfo.fields[i].name==FieldName)
      return StructInfo.fields[i].type;
  return "";
}

inline int32_t StructWrite(const std::string &StructText, const std::string &FieldName, int64_t value, char *buffer)
{
  try
  {
    BitFieldStructParser::struct_write(StructText, FieldName, &value, buffer);
  }
  catch (...)
  {
    return -1;
  }
  return 0;
}

inline int32_t StructWrite(const std::string &StructText, const std::string &FieldName, float value, char *buffer)
{
  uint64_t IntValue;
  float *Float=(float *)&IntValue;
  *Float=value;
  try
  {
    BitFieldStructParser::struct_write(StructText, FieldName, &IntValue, buffer);
  }
  catch (...)
  {
    return -1;
  }
  return 0;
}

inline int32_t StructSizeOf(const std::string &StructString)
{
  return BitFieldStructParser::struct_sizeof(StructString);
}


#endif // STRUCTPARSER_H
