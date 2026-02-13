#include <stdio.h>
#include <stdlib.h>

#include <serial/err.h>


char* deserialize_strerror(DeserializeError e) {
    switch (e)
    {
    case DE_NOT_SUPPORTED_DTARRAYS:
        return "Non-homogeneus arrays are not supported for deserializing.";
    case DE_STREAM_INCOMPATIBLE:
        return "Given stream and given model type were not compatible: ";
    case DE_NULL_PTR:
        return "One or more parameters were NULL pointers.";
    case DE_SYNTAX_ERROR:
        return "Invalid syntax";
    case DE_UNBALANCED_STREAM:
        return "Given stream's nesting characters '{', '}', '[', ']' were not balanced";
    case DE_UNEXPECTED_CHARACTER:
        return "Unexpected character";
    case DE_TOO_MUCH_NESTING:
        return "Given stream exceded nesting limit of 512";
    
    default:
        return "";
    }
}

