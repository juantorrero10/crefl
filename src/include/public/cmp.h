
#ifndef PUBLIC_CMP_H_
#define PUBLIC_CMP_H_

int refl_comp_struct_nametype_subset(RStruct* a, RStruct* b);
int refl_comp_field_nametypw(RField* a, RField* b);
int refl_comp_enum_label_subset(REnum* a, REnum* b);

int refl_enum_migrate_by_label(REnum* old, REnum* new, int val);
RObject refl_struct_migrate(RStruct* old, RStruct* new, RObject instance);
int refl_comp_struct_schema(RStruct* model, RStruct* generated);


#endif // PUBLIC_CMP_H_