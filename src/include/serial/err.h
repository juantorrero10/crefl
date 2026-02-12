#ifndef SERIAL_ERR_H_
#define SERIAL_ERR_H_

typedef enum {
    DE_STREAM_INCOMPATIBLE,
    DE_NOT_SUPPORTED_DTARRAYS,
} DeserializeError;

typedef struct {
    enum { DR_OK, DR_ERR } st;
    union 
    {
        void* buff;
        DeserializeError err;
    } data;
} DeserializeResult;

char* deserialize_strerror(DeserializeError e);

#endif // SERIAL_ERR_H_