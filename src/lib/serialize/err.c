#include <serial/err.h>

char* deserialize_strerror(DeserializeError e) {
    switch (e)
    {
    case DE_NOT_SUPPORTED_DTARRAYS:
        return "Non-homogeneus arrays are not supported for deserializing.";
    case DE_STREAM_INCOMPATIBLE:
        return "Given stream and given model type were not compatible.";
        
    
    default:
        return "";
    }
}