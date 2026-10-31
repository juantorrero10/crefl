#ifndef DOT_CREFL_H_
#define DOT_CREFL_H_


/**
 * Definitions for parsing the .CREFL file
 */
#include <stdint.h>
#include "../public/types.h"

#if defined(__GNUC__) || defined(__clang__)
#  define CREFL_PACKED __attribute__((packed))
#elif defined(_MSC_VER)
#  define CREFL_PACKED __pragma(pack(push, 1))
#else
#  error "Unknown compiler"
#endif


#define DEREF_NEW(type, new_var, origin, offset) type* new_var = (type*)((u8*)origin + offset)
#define DEREF(type, new_var, origin, offset) new_var = (type*)((u8*)origin + offset)
#define CREFL_MAGIC 0x435245464C583634


// All strings store in string pool are null terminated
typedef struct CREFL_PACKED 
{
    u32 offset;            // From start of string pool
    u32 size;              // Not accounting NULL term
} CREFL_String;

/**
 * When an attribute requires an arbitrary amount 
 * of references to something it employs the INDEX_TABLE
 * An entry to the table is simply the offset / 4 from the start of the section.
 * The meaning of these indexes are arbitrary.
 * 
 *    index0     index1       index2     index3
 * AA AA AA AA BB BB BB BB CC CC CC CC DD DD DD DD
 * ^ entry
 */
typedef u32 index_table_offset;


typedef struct CREFL_PACKED
{
    u64    magic;             // "CREFLX64"
    u16    version;           // 00 00
    u8     flags1;            
    u8     flags2;
    u8     checksum[16];
    u32    n_sections;        // 7
} CREFL_Header;

typedef enum 
{
    // Arrays, one element next to eachother
    CREFL_SECTION_FIELDS,
    CREFL_SECTION_ENUMS,
    CREFL_SECTION_STRUCTS,
    CREFL_SECTION_UNIONS,

    // General purpose
    CREFL_SECTION_STRING_POOL,
    CREFL_SECTION_INDEX_TABLE,

    // Runtime hash tables
    CREFL_SECTION_HASHTABLES
} CREFL_Section_kind;
// Right after

typedef struct CREFL_PACKED 
{
    u32 kind;              // Means the same as the enum but it has to be 4 bytes
    u64 start;             // From FILE SET
    u32 n_elements;
    u32 element_size;
} CREFL_Section_table;

typedef struct CREFL_PACKED
{
    CREFL_String name;
    u32 offset;            // Runtime offset, nothing to do with the .CREFL
    u32 next_offset;
    u32 array_size;        // For K_OPTION, byte size of the inline option wrapper
    u32 kind;              // Same as RKind


    // Its meaning derives from the kind att
    // K_STRUCT, K_UNION, K_ENUM                        -> index of the type into the respective section.
    // K_OPTION, K_FIXED_ARRAY, K_DYN_ARRAY, K_POINTER  -> index of the subfield in the field section
    // K_OPTION subtype field offset                    -> offset of the inline payload in the wrapper
    // K_PRIMITIVE                                      -> Type of primitive, same as KPimitive enum.
    // K_STRING                                         -> Unused
    u32 subtype_data;
} CREFL_Field;

typedef struct CREFL_PACKED
{
    CREFL_String type_name;
    // Storing n_elements's DWORD id's of the fields
    index_table_offset fields;
    u32 n_elements;
    u32 total_size;
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
    u32 n_elements;
} CREFL_Enum;


// Kind of a linked list node containing the offset to the next hashtable
// This is just before the actual hashtable
typedef struct CREFL_PACKED {
    u32 n_elements;
    u32 next_hash_table_offset;  // From start of section
} CREFL_Hashtable_Header;

typedef struct CREFL_PACKED
{
    u32 element_id;
    u32 hashed_index;
} CREFL_Hashtable_Entry;



#endif // DOT_CREFL_H_