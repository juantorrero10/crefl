#ifndef DOT_CREFL_H_
#define DOT_CREFL_H_


/**
 * Definitions for parsing the .CREFL file
 */
#include <stdint.h>

#if defined(__GNUC__) || defined(__clang__)
#  define CREFL_PACKED __attribute__((packed))
#elif defined(_MSC_VER)
#  define CREFL_PACKED __pragma(pack(push, 1))
#else
#  error "Unknown compiler"
#endif



#define CREFL_MAGIC 0x435245464C583634


typedef struct CREFL_PACKED 
{
    uint32_t offset;            // From start of string pool
    uint32_t size;              // String size no NULL term.
} CREFL_String;

/**
 * When an attribute requires an arbitrary amount 
 * of references to something it employs the INDEX_TABLE
 * An entry to the table is simply the offset / 4 from the start of the section.
 * The meaning of these indexes are dependent on the employer.
 * 
 *    index0     index1       index2     index3
 * AA AA AA AA BB BB BB BB CC CC CC CC DD DD DD DD
 * ^ entry
 */
typedef uint32_t index_table_offset;


typedef struct CREFL_PACKED
{
    uint64_t magic;             // "CREFLX64"
    uint16_t version;           // 00 00
    uint16_t flags;             // Maybe useful in the future
    uint32_t n_sections;        // 4
    uint32_t string_pool_size;
} CREFL_Header;

// Right after

typedef struct CREFL_PACKED 
{
    uint32_t kind;              // Means the same as the enum but it has to be 4 bytes
    uint64_t start;             // From FILE SET
    uint32_t n_elements;
    uint32_t element_size;
} CREFL_Section_table;

typedef struct CREFL_PACKED
{
    CREFL_String name;
    uint32_t offset;            // Runtime offset, nothing to do with the .CREFL
    uint32_t array_size;
    uint32_t kind;              // Same as RKind


    // Its meaning derives from the kind att
    // K_STRUCT, K_UNION, K_ENUM                        -> index of the type into the respective section.
    // K_OPTION, K_FIXED_ARRAY, K_DYN_ARRAY, K_POINTER  -> index of the subfield in the field section
    // K_PRIMITIVE                                      -> Type of primitive, same as KPimitive enum.
    // K_STRING                                         -> Unused
    uint32_t subtype_data;
} CREFL_Field;

typedef struct CREFL_PACKED
{
    CREFL_String type_name;
    // Storing n_elements's DWORD id's of the fields
    index_table_offset fields;
    uint32_t n_elements;
} CREFL_Struct, CREFL_Union;

typedef struct CREFL_PACKED {
    CREFL_String type_name;
    // Referencing n_elements * 2 DWORDS in index_table
    // They are string offset-size pairs
    // For parsing we need to construct the strings using the pairs
    index_table_offset names;
    
    // Referencing the values (indexes) of each enum entry.
    // Taking n_elements DWORD from the table
    index_table_offset indexes;
    uint32_t n_elements;
} CREFL_Enum;



typedef enum 
{
    CREFL_SECTION_FIELDS,
    CREFL_SECTION_ENUMS,
    CREFL_SECTION_STRUCTS,
    CREFL_SECTION_UNIONS,
    CREFL_SECTION_INDEX_TABLE,
    CREFL_SECTION_STRING_POOL,
    CREFL_SECTION_HASHTABLES
} CREFL_Section_kind;




#endif // DOT_CREFL_H_