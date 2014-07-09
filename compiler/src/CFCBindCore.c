/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "charmony.h"

#include <string.h>
#include <stdio.h>

#define CFC_NEED_BASE_STRUCT_DEF
#include "CFCBase.h"
#include "CFCBindCore.h"
#include "CFCBindClass.h"
#include "CFCBindFile.h"
#include "CFCClass.h"
#include "CFCFile.h"
#include "CFCHierarchy.h"
#include "CFCParcel.h"
#include "CFCUtil.h"

#define STRING(s)  #s
#define XSTRING(s) STRING(s)

struct CFCBindCore {
    CFCBase base;
    CFCHierarchy *hierarchy;
    char         *header;
    char         *footer;
};

/* Write the "parcel.h" header file, which contains common symbols needed by
 * all classes, plus typedefs for all class structs.
 */
static void
S_write_parcel_h(CFCBindCore *self, CFCParcel *parcel);

/* Write the "parcel.c" file containing autogenerated implementation code.
 */
static void
S_write_parcel_c(CFCBindCore *self, CFCParcel *parcel);

/* Write the "cfish_platform.h" header file, which contains platform-specific
 * definitions.
 */
static void
S_write_platform_h(CFCBindCore *self);

static char*
S_charmony_feature_defines();

static char*
S_charmony_string_defines();

static char*
S_charmony_stdbool_defines();

static char*
S_charmony_stdint_defines();

static char*
S_charmony_alloca_defines();

static const CFCMeta CFCBINDCORE_META = {
    "Clownfish::CFC::Binding::Core",
    sizeof(CFCBindCore),
    (CFCBase_destroy_t)CFCBindCore_destroy
};

CFCBindCore*
CFCBindCore_new(CFCHierarchy *hierarchy, const char *header,
                const char *footer) {
    CFCBindCore *self = (CFCBindCore*)CFCBase_allocate(&CFCBINDCORE_META);
    return CFCBindCore_init(self, hierarchy, header, footer);
}

CFCBindCore*
CFCBindCore_init(CFCBindCore *self, CFCHierarchy *hierarchy,
                 const char *header, const char *footer) {
    CFCUTIL_NULL_CHECK(hierarchy);
    CFCUTIL_NULL_CHECK(header);
    CFCUTIL_NULL_CHECK(footer);
    self->hierarchy = (CFCHierarchy*)CFCBase_incref((CFCBase*)hierarchy);
    self->header    = CFCUtil_strdup(header);
    self->footer    = CFCUtil_strdup(footer);
    return self;
}

void
CFCBindCore_destroy(CFCBindCore *self) {
    CFCBase_decref((CFCBase*)self->hierarchy);
    FREEMEM(self->header);
    FREEMEM(self->footer);
    CFCBase_destroy((CFCBase*)self);
}

int
CFCBindCore_write_all_modified(CFCBindCore *self, int modified) {
    CFCHierarchy *hierarchy = self->hierarchy;
    const char   *header    = self->header;
    const char   *footer    = self->footer;

    // Discover whether files need to be regenerated.
    modified = CFCHierarchy_propagate_modified(hierarchy, modified);

    // Iterate over all File objects, writing out those which don't have
    // up-to-date auto-generated files.
    const char *inc_dest = CFCHierarchy_get_include_dest(hierarchy);
    CFCFile **files = CFCHierarchy_files(hierarchy);
    for (int i = 0; files[i] != NULL; i++) {
        if (CFCFile_get_modified(files[i])) {
            CFCBindFile_write_h(files[i], inc_dest, header, footer);
        }
    }

    // If any class definition has changed, rewrite the parcel.h and parcel.c
    // files.
    if (modified) {
        S_write_platform_h(self);

        CFCParcel **parcels = CFCParcel_all_parcels();
        for (size_t i = 0; parcels[i]; ++i) {
            CFCParcel *parcel = parcels[i];
            if (CFCParcel_required(parcel)) {
                S_write_parcel_h(self, parcel);
                if (!CFCParcel_included(parcel)) {
                    S_write_parcel_c(self, parcel);
                }
            }
        }
    }

    return modified;
}

/* Write the "parcel.h" header file, which contains common symbols needed by
 * all classes, plus typedefs for all class structs.
 */
static void
S_write_parcel_h(CFCBindCore *self, CFCParcel *parcel) {
    CFCHierarchy *hierarchy   = self->hierarchy;
    const char   *prefix      = CFCParcel_get_prefix(parcel);
    const char   *PREFIX      = CFCParcel_get_PREFIX(parcel);
    const char   *privacy_sym = CFCParcel_get_privacy_sym(parcel);

    // Declare object structs for all instantiable classes.
    char *typedefs = CFCUtil_strdup("");
    CFCClass **ordered = CFCHierarchy_ordered_classes(hierarchy);
    for (int i = 0; ordered[i] != NULL; i++) {
        CFCClass *klass = ordered[i];
        const char *class_prefix = CFCClass_get_prefix(klass);
        if (strcmp(class_prefix, prefix) != 0) { continue; }

        if (!CFCClass_inert(klass)) {
            const char *full_struct = CFCClass_full_struct_sym(klass);
            typedefs = CFCUtil_cat(typedefs, "typedef struct ", full_struct,
                                   " ", full_struct, ";\n", NULL);
        }
    }
    FREEMEM(ordered);

    // Special includes and macros for Clownfish parcel.
    const char *cfish_includes =
        "#include <stdarg.h>\n"
        "#include <stddef.h>\n"
        "\n"
        "#include \"cfish_platform.h\"\n"
        "#include \"cfish_hostdefs.h\"\n";

    // Special definitions for Clownfish parcel.
    const char *cfish_defs =
        "#define CFISH_UNUSED_VAR(var) ((void)var)\n"
        "#define CFISH_UNREACHABLE_RETURN(type) return (type)0\n"
        "\n"
        "/* Generic method pointer.\n"
        " */\n"
        "typedef void\n"
        "(*cfish_method_t)(const void *vself);\n"
        "\n"
        "/* Access the function pointer for a given method from the class.\n"
        " */\n"
        "#define CFISH_METHOD_PTR(_class, _full_meth) \\\n"
        "     ((_full_meth ## _t)cfish_method(_class, _full_meth ## _OFFSET))\n"
        "\n"
        "static CFISH_INLINE cfish_method_t\n"
        "cfish_method(const void *klass, size_t offset) {\n"
        "    union { char *cptr; cfish_method_t *fptr; } ptr;\n"
        "    ptr.cptr = (char*)klass + offset;\n"
        "    return ptr.fptr[0];\n"
        "}\n"
        "\n"
        "typedef struct cfish_Dummy {\n"
        "   CFISH_OBJ_HEAD\n"
        "   void *klass;\n"
        "} cfish_Dummy;\n"
        "\n"
        "/* Access the function pointer for a given method from the object.\n"
        " */\n"
        "static CFISH_INLINE cfish_method_t\n"
        "cfish_obj_method(const void *object, size_t offset) {\n"
        "    cfish_Dummy *dummy = (cfish_Dummy*)object;\n"
        "    return cfish_method(dummy->klass, offset);\n"
        "}\n"
        "\n"
        "/* Access the function pointer for the given method in the\n"
        " * superclass. */\n"
        "#define CFISH_SUPER_METHOD_PTR(_class, _full_meth) \\\n"
        "     ((_full_meth ## _t)cfish_super_method(_class, \\\n"
        "                                           _full_meth ## _OFFSET))\n"
        "\n"
        "extern CFISH_VISIBLE size_t cfish_Class_offset_of_parent;\n"
        "static CFISH_INLINE cfish_method_t\n"
        "cfish_super_method(const void *klass, size_t offset) {\n"
        "    char *class_as_char = (char*)klass;\n"
        "    cfish_Class **parent_ptr\n"
        "        = (cfish_Class**)(class_as_char + cfish_Class_offset_of_parent);\n"
        "    return cfish_method(*parent_ptr, offset);\n"
        "}\n"
        "\n"
        "/* Structs for Class initialization.\n"
        " */\n"
        "\n"
        "typedef struct cfish_NovelMethSpec {\n"
        "    size_t         *offset;\n"
        "    const char     *name;\n"
        "    cfish_method_t  func;\n"
        "    cfish_method_t  callback_func;\n"
        "} cfish_NovelMethSpec;\n"
        "\n"
        "typedef struct cfish_OverriddenMethSpec {\n"
        "    size_t         *offset;\n"
        "    size_t         *parent_offset;\n"
        "    cfish_method_t  func;\n"
        "} cfish_OverriddenMethSpec;\n"
        "\n"
        "typedef struct cfish_InheritedMethSpec {\n"
        "    size_t *offset;\n"
        "    size_t *parent_offset;\n"
        "} cfish_InheritedMethSpec;\n"
        "\n"
        "typedef struct cfish_ClassSpec {\n"
        "    cfish_Class **klass;\n"
        "    cfish_Class **parent;\n"
        "    const char   *name;\n"
        "    size_t        ivars_size;\n"
        "    size_t       *ivars_offset_ptr;\n"
        "    uint32_t      num_novel_meths;\n"
        "    uint32_t      num_overridden_meths;\n"
        "    uint32_t      num_inherited_meths;\n"
        "    const cfish_NovelMethSpec      *novel_meth_specs;\n"
        "    const cfish_OverriddenMethSpec *overridden_meth_specs;\n"
        "    const cfish_InheritedMethSpec  *inherited_meth_specs;\n"
        "} cfish_ClassSpec;\n"
        "\n"
        "#ifdef CFISH_USE_SHORT_NAMES\n"
        "  #define METHOD_PTR               CFISH_METHOD_PTR\n"
        "  #define SUPER_METHOD_PTR         CFISH_SUPER_METHOD_PTR\n"
        "#endif\n"
        "\n";

    const char *extra_defs;
    char *extra_includes;
    if (strcmp(prefix, "cfish_") == 0) {
        extra_defs = cfish_defs;
        extra_includes = CFCUtil_strdup(cfish_includes);
    }
    else {
        extra_defs = "";
        extra_includes = CFCUtil_strdup("");

        // Include parcel.h of prerequisite parcels.
        CFCParcel **prereq_parcels = CFCParcel_prereq_parcels(parcel);
        for (size_t i = 0; prereq_parcels[i]; ++i) {
            const char *prereq_prefix
                = CFCParcel_get_prefix(prereq_parcels[i]);
            extra_includes = CFCUtil_cat(extra_includes, "#include <",
                                         prereq_prefix, "parcel.h>\n", NULL);
        }
        FREEMEM(prereq_parcels);
    }

    const char pattern[] =
        "%s\n"
        "#ifndef CFISH_%sPARCEL_H\n"
        "#define CFISH_%sPARCEL_H 1\n"
        "\n"
        "#ifdef __cplusplus\n"
        "extern \"C\" {\n"
        "#endif\n"
        "\n"
        "%s" // Extra includes.
        "\n"
        "#ifdef %s\n"
        "  #define %sVISIBLE CFISH_EXPORT\n"
        "#else\n"
        "  #define %sVISIBLE CFISH_IMPORT\n"
        "#endif\n"
        "\n"
        "%s" // Typedefs.
        "\n"
        "%s" // Extra definitions.
        "%sVISIBLE void\n"
        "%sbootstrap_inheritance();\n"
        "\n"
        "%sVISIBLE void\n"
        "%sbootstrap_parcel();\n"
        "\n"
        "void\n"
        "%sinit_parcel();\n"
        "\n"
        "#ifdef __cplusplus\n"
        "}\n"
        "#endif\n"
        "\n"
        "#endif /* CFISH_%sPARCEL_H */\n"
        "\n"
        "%s\n"
        "\n";
    char *file_content
        = CFCUtil_sprintf(pattern, self->header, PREFIX, PREFIX,
                          extra_includes, privacy_sym, PREFIX, PREFIX,
                          typedefs, extra_defs, PREFIX, prefix, PREFIX, prefix,
                          prefix, PREFIX, self->footer);

    // Unlink then write file.
    const char *inc_dest = CFCHierarchy_get_include_dest(hierarchy);
    char *filepath = CFCUtil_sprintf("%s" CHY_DIR_SEP "%sparcel.h", inc_dest,
                                     prefix);
    remove(filepath);
    CFCUtil_write_file(filepath, file_content, strlen(file_content));
    FREEMEM(filepath);

    FREEMEM(typedefs);
    FREEMEM(extra_includes);
    FREEMEM(file_content);
}

static void
S_write_parcel_c(CFCBindCore *self, CFCParcel *parcel) {
    CFCHierarchy *hierarchy = self->hierarchy;
    const char   *prefix    = CFCParcel_get_prefix(parcel);

    // Aggregate C code for the parcel.
    char *privacy_syms = CFCUtil_strdup("");
    char *includes     = CFCUtil_strdup("");
    char *c_data       = CFCUtil_strdup("");
    char *class_specs  = CFCUtil_strdup(
        "static const cfish_ClassSpec class_specs[] = {\n");
    int num_specs = 0;
    CFCClass **ordered  = CFCHierarchy_ordered_classes(hierarchy);
    for (int i = 0; ordered[i] != NULL; i++) {
        CFCClass *klass = ordered[i];
        const char *class_prefix = CFCClass_get_prefix(klass);
        if (strcmp(class_prefix, prefix) != 0) { continue; }

        const char *include_h = CFCClass_include_h(klass);
        includes = CFCUtil_cat(includes, "#include \"", include_h,
                               "\"\n", NULL);

        CFCBindClass *class_binding = CFCBindClass_new(klass);
        char *class_c_data = CFCBindClass_to_c_data(class_binding);
        c_data = CFCUtil_cat(c_data, class_c_data, "\n", NULL);
        FREEMEM(class_c_data);
        if (!CFCClass_inert(klass)) {
            if (num_specs != 0) {
                class_specs = CFCUtil_cat(class_specs, ",\n", NULL);
            }
            char *class_spec = CFCBindClass_spec_def(class_binding);
            class_specs = CFCUtil_cat(class_specs, class_spec, NULL);
            FREEMEM(class_spec);
            ++num_specs;
        }
        CFCBase_decref((CFCBase*)class_binding);
        const char *privacy_sym = CFCClass_privacy_symbol(klass);
        privacy_syms = CFCUtil_cat(privacy_syms, "#define ",
                                   privacy_sym, "\n", NULL);
    }
    class_specs = CFCUtil_cat(class_specs, "\n};\n", NULL);
    FREEMEM(ordered);

    // Bootstrapping code for prerequisite parcels.
    //
    // bootstrap_inheritance() first calls bootstrap_inheritance() for all
    // parcels from which classes are inherited. Then the Classes of the parcel
    // are initialized. It aborts on recursive invocation.
    //
    // bootstrap_parcel() first calls bootstrap_inheritance() of its own
    // parcel. Then it calls bootstrap_parcel() for all prerequisite parcels.
    // Finally, it calls init_parcel(). Recursive invocation is allowed.

    char *inh_bootstrap    = CFCUtil_strdup("");
    char *prereq_bootstrap = CFCUtil_strdup("");
    CFCParcel **inh_parcels = CFCParcel_inherited_parcels(parcel);
    for (size_t i = 0; inh_parcels[i]; ++i) {
        const char *inh_prefix = CFCParcel_get_prefix(inh_parcels[i]);
        inh_bootstrap = CFCUtil_cat(inh_bootstrap, "    ", inh_prefix,
                                    "bootstrap_inheritance();\n", NULL);
    }
    FREEMEM(inh_parcels);
    CFCParcel **prereq_parcels = CFCParcel_prereq_parcels(parcel);
    for (size_t i = 0; prereq_parcels[i]; ++i) {
        const char *prereq_prefix = CFCParcel_get_prefix(prereq_parcels[i]);
        prereq_bootstrap = CFCUtil_cat(prereq_bootstrap, "    ", prereq_prefix,
                                       "bootstrap_parcel();\n", NULL);
    }
    FREEMEM(prereq_parcels);

    char pattern[] =
        "%s\n"
        "\n"
        "#define C_CFISH_CLASS\n"            // Needed for abstract methods.
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "%s\n"
        "#include \"%sparcel.h\"\n"
        "#include \"callbacks.h\"\n"
        "#include \"Clownfish/String.h\"\n"  // Needed for dump/load.
        "#include \"Clownfish/Err.h\"\n"     // Needed for dump/load.
        "#include \"Clownfish/Num.h\"\n"     // Needed for dump/load.
        "#include \"Clownfish/VArray.h\"\n"  // Needed for dump/load.
        "#include \"Clownfish/Class.h\"\n"   // Needed for bootstrap.
        "%s\n"
        "\n"
        "%s\n"
        "\n"
        "/* ClassSpec structs for initialization.\n"
        " */\n"
        "%s\n"
        "\n"
        "static int bootstrap_state = 0;\n"
        "\n"
        "void\n"
        "%sbootstrap_inheritance() {\n"
        "    if (bootstrap_state == 1) {\n"
        "        fprintf(stderr, \"Cycle in class inheritance between\"\n"
        "                        \" parcels detected.\\n\");\n"
        "        abort();\n"
        "    }\n"
        "    if (bootstrap_state >= 2) { return; }\n"
        "    bootstrap_state = 1;\n"
        "%s" // Bootstrap inherited parcels.
        "    cfish_Class_bootstrap(class_specs, %d);\n"
        "    bootstrap_state = 2;\n"
        "}\n"
        "\n"
        "void\n"
        "%sbootstrap_parcel() {\n"
        "    if (bootstrap_state >= 3) { return; }\n"
        "    %sbootstrap_inheritance();\n"
        "    bootstrap_state = 3;\n"
        "%s" // Finish bootstrapping of all prerequisite parcels.
        "    %sinit_parcel();\n"
        "}\n"
        "\n"
        "%s\n";
    char *file_content
        = CFCUtil_sprintf(pattern, self->header, privacy_syms, prefix,
                          includes, c_data, class_specs, prefix, inh_bootstrap,
                          num_specs, prefix, prefix, prereq_bootstrap, prefix,
                          self->footer);

    // Unlink then open file.
    const char *src_dest = CFCHierarchy_get_source_dest(hierarchy);
    char *filepath = CFCUtil_sprintf("%s" CHY_DIR_SEP "%sparcel.c", src_dest,
                                     prefix);
    remove(filepath);
    CFCUtil_write_file(filepath, file_content, strlen(file_content));
    FREEMEM(filepath);

    FREEMEM(privacy_syms);
    FREEMEM(includes);
    FREEMEM(c_data);
    FREEMEM(class_specs);
    FREEMEM(inh_bootstrap);
    FREEMEM(prereq_bootstrap);
    FREEMEM(file_content);
}

/* Write the "callbacks.h" header file, which contains declarations of host
 * callbacks.
 */
void
CFCBindCore_write_callbacks_h(CFCBindCore *self) {
    CFCHierarchy  *hierarchy   = self->hierarchy;
    CFCClass     **ordered     = CFCHierarchy_ordered_classes(hierarchy);
    char          *includes    = CFCUtil_strdup("");
    char          *all_cb_decs = CFCUtil_strdup("");

    for (int i = 0; ordered[i] != NULL; i++) {
        CFCClass *klass = ordered[i];

        const char *include_h = CFCClass_include_h(klass);
        includes = CFCUtil_cat(includes, "#include \"", include_h, "\"\n",
                               NULL);

        if (!CFCClass_included(klass)) {
            CFCBindClass *class_binding = CFCBindClass_new(klass);
            char *cb_decs = CFCBindClass_callback_decs(class_binding);
            all_cb_decs = CFCUtil_cat(all_cb_decs, cb_decs, NULL);
            FREEMEM(cb_decs);
            CFCBase_decref((CFCBase*)class_binding);
        }
    }

    FREEMEM(ordered);

    const char pattern[] =
        "%s\n"
        "#ifndef CFCCALLBACKS_H\n"
        "#define CFCCALLBACKS_H 1\n"
        "\n"
        "#ifdef __cplusplus\n"
        "extern \"C\" {\n"
        "#endif\n"
        "\n"
        "%s"
        "\n"
        "%s"
        "\n"
        "#ifdef __cplusplus\n"
        "}\n"
        "#endif\n"
        "\n"
        "#endif /* CFCCALLBACKS_H */\n"
        "\n"
        "%s\n"
        "\n";
    char *file_content
        = CFCUtil_sprintf(pattern, self->header, includes, all_cb_decs,
                          self->footer);

    // Unlink then write file.
    const char *inc_dest = CFCHierarchy_get_include_dest(hierarchy);
    char *filepath = CFCUtil_sprintf("%s" CHY_DIR_SEP "callbacks.h", inc_dest);
    remove(filepath);
    CFCUtil_write_file(filepath, file_content, strlen(file_content));
    FREEMEM(filepath);

    FREEMEM(includes);
    FREEMEM(all_cb_decs);
    FREEMEM(file_content);
}

/* Write the "cfish_platform.h" header file, which contains platform-specific
 * definitions.
 */
static void
S_write_platform_h(CFCBindCore *self) {
    char *feature_defs = S_charmony_feature_defines();
    char *string_defs  = S_charmony_string_defines();
    char *stdbool_defs = S_charmony_stdbool_defines();
    char *stdint_defs  = S_charmony_stdint_defines();
    char *alloca_defs  = S_charmony_alloca_defines();

    const char pattern[] =
        "%s"
        "\n"
        "#ifndef CFISH_PLATFORM_H\n"
        "#define CFISH_PLATFORM_H 1\n"
        "\n"
        "#ifdef __cplusplus\n"
        "extern \"C\" {\n"
        "#endif\n"
        "\n"
        "%s"
        "%s"
        "\n"
        "%s"
        "%s"
        "\n"
        "%s"
        "\n"
        "#ifdef __cplusplus\n"
        "}\n"
        "#endif\n"
        "\n"
        "#endif /* CFISH_PLATFORM_H */\n"
        "\n"
        "%s"
        "\n";
    char *file_content
        = CFCUtil_sprintf(pattern, self->header, feature_defs, string_defs,
                          stdbool_defs, stdint_defs, alloca_defs,
                          self->footer);

    // Unlink then write file.
    const char *inc_dest = CFCHierarchy_get_include_dest(self->hierarchy);
    char *filepath = CFCUtil_sprintf("%s" CHY_DIR_SEP "cfish_platform.h",
                                     inc_dest);
    remove(filepath);
    CFCUtil_write_file(filepath, file_content, strlen(file_content));
    FREEMEM(filepath);

    FREEMEM(feature_defs);
    FREEMEM(string_defs);
    FREEMEM(stdbool_defs);
    FREEMEM(stdint_defs);
    FREEMEM(alloca_defs);
    FREEMEM(file_content);
}

static char*
S_charmony_feature_defines() {
    char *defines = CFCUtil_strdup("");

#ifdef CHY_HAS_WINDOWS_H
    defines = CFCUtil_cat(defines, "#define CFISH_HAS_WINDOWS_H\n", NULL);
#endif
#ifdef CHY_HAS_SYS_ATOMIC_H
    defines = CFCUtil_cat(defines, "#define CFISH_HAS_SYS_ATOMIC_H\n", NULL);
#endif
#ifdef CHY_HAS_PTHREAD_H
    defines = CFCUtil_cat(defines, "#define CFISH_HAS_PTHREAD_H\n", NULL);
#endif
#ifdef CHY_LITTLE_END
    defines = CFCUtil_cat(defines, "#define CFISH_LITTLE_END\n", NULL);
#endif
#ifdef CHY_BIG_END
    defines = CFCUtil_cat(defines, "#define CFISH_BIG_END\n", NULL);
#endif
#ifdef CHY_HAS_FUNC_MACRO
    defines = CFCUtil_cat(defines, "#define CFISH_HAS_FUNC_MACRO\n", NULL);
#endif
#ifdef CHY_HAS_VARIADIC_MACROS
    defines = CFCUtil_cat(defines, "#define CFISH_HAS_VARIADIC_MACROS\n",
                          NULL);
#endif
#ifdef CHY_HAS_ISO_VARIADIC_MACROS
    defines = CFCUtil_cat(defines, "#define CFISH_HAS_ISO_VARIADIC_MACROS\n",
                          NULL);
#endif
#ifdef CHY_HAS_GNUC_VARIADIC_MACROS
    defines = CFCUtil_cat(defines, "#define CFISH_HAS_GNUC_VARIADIC_MACROS\n",
                          NULL);
#endif
#ifdef CHY_HAS_OSATOMIC_CAS_PTR
    defines = CFCUtil_cat(defines, "#define CFISH_HAS_OSATOMIC_CAS_PTR\n",
                          NULL);
#endif

    return defines;
}

static char*
S_charmony_string_defines() {
    const char *pattern =
        "#define CFISH_INLINE %s\n"
        "#define CFISH_EXPORT %s\n"
        "#define CFISH_IMPORT %s\n"
        "#define CFISH_SIZEOF_CHAR %s\n"
        "#define CFISH_SIZEOF_SHORT %s\n"
        "#define CFISH_SIZEOF_INT %s\n"
        "#define CFISH_SIZEOF_LONG %s\n"
        "#define CFISH_SIZEOF_SIZE_T %s\n"
        "#define CFISH_FUNC_MACRO %s\n"
        "#define CFISH_U64_TO_DOUBLE(x) %s\n";
    char *defines
        = CFCUtil_sprintf(pattern,
                          XSTRING(CHY_INLINE),
                          XSTRING(CHY_EXPORT),
                          XSTRING(CHY_IMPORT),
                          XSTRING(CHY_SIZEOF_CHAR),
                          XSTRING(CHY_SIZEOF_SHORT),
                          XSTRING(CHY_SIZEOF_INT),
                          XSTRING(CHY_SIZEOF_LONG),
                          XSTRING(CHY_SIZEOF_SIZE_T),
                          XSTRING(CHY_FUNC_MACRO),
                          XSTRING(CHY_U64_TO_DOUBLE(x)));

    return defines;
}

static char*
S_charmony_stdbool_defines() {
#ifdef CHY_HAS_STDBOOL_H
    const char *defines = "#include <stdbool.h>\n";
#else
    const char *defines =
        "#if (!defined(__cplusplus) && !defined(CFISH_HAS_STDBOOL))\n"
        "  typedef int bool;\n"
        "  #ifndef true\n"
        "    #define true 1\n"
        "  #endif\n"
        "  #ifndef false\n"
        "    #define false 0\n"
        "  #endif\n"
        "#endif\n";
#endif

    return CFCUtil_strdup(defines);
}

static char*
S_charmony_stdint_defines() {
#ifdef CHY_HAS_STDINT_H
    return CFCUtil_strdup("#include <stdint.h>\n");
#else
    const char *pattern =
        "#ifndef CFISH_HAS_STDINT\n"
        "  typedef %s int8_t;\n"
        "  typedef %s uint8_t;\n"
        "  typedef %s int16_t;\n"
        "  typedef %s uint16_t;\n"
        "  typedef %s int32_t;\n"
        "  typedef %s uint32_t;\n"
        "  typedef %s int64_t;\n"
        "  typedef %s uint64_t;\n"
        "#endif\n";
    return CFCUtil_sprintf(pattern,
                           XSTRING(CHY_INT8_T),  XSTRING(CHY_UINT8_T),
                           XSTRING(CHY_INT16_T), XSTRING(CHY_UINT16_T),
                           XSTRING(CHY_INT32_T), XSTRING(CHY_UINT32_T),
                           XSTRING(CHY_INT64_T), XSTRING(CHY_UINT64_T));
#endif
}

static char*
S_charmony_alloca_defines() {
    char *defines = CFCUtil_strdup("");

#if defined(CHY_HAS_ALLOCA_H)
    defines = CFCUtil_cat(defines, "#include <alloca.h>\n", NULL);
#elif defined(CHY_HAS_MALLOC_H)
    defines = CFCUtil_cat(defines, "#include <malloc.h>\n", NULL);
#elif defined(CHY_ALLOCA_IN_STDLIB_H)
    defines = CFCUtil_cat(defines, "#include <stdlib.h>\n", NULL);
#endif

    defines = CFCUtil_cat(defines, "#define cfish_alloca ",
                          XSTRING(chy_alloca), "\n", NULL);

    return defines;
}


