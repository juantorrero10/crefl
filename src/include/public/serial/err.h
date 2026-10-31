#ifndef SERIAL_ERR_H_
#define SERIAL_ERR_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DE_NULL_PTR,
    DE_SYNTAX_ERROR,
    DE_STREAM_INCOMPATIBLE,
    DE_NOT_SUPPORTED_DTARRAYS,
    DE_UNBALANCED_STREAM,
    DE_UNEXPECTED_CHARACTER,
    DE_UNEXPECTED_EOF,
    DE_TOO_MUCH_NESTING,
    DE_UNCLOSED_DQUOTE,
    DE_NUMBER_CONVERSION_ERROR,
    DE_MEMORY_ERROR,
    DE_UNKNOWN_KEYWORD,
    DE_EXPECTED_EOF,
    DE_EMPTY_STREAM,
    DE_BADLY_SCAPED_STRING,
    DE_LABEL_NOT_FOUND,
    DE_ARRAY_TOO_BIG,
    DE_TARGET_TYPE_TOO_COMPLEX,
} DeserializeError;

typedef struct {
    enum { DR_OK, DR_ERR } st;
    DeserializeError err;
    union 
    {   
        char* err_str;
        void* buff;
    } data;
} DeserializeResult;

char* deserialize_strerror(DeserializeError e);

#ifdef __cplusplus
}
#endif


#endif // SERIAL_ERR_H_