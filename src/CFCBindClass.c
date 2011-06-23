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

#define CFC_NEED_BASE_STRUCT_DEF

#include <stdio.h>
#include <string.h>
#include "CFCBindClass.h"
#include "CFCBase.h"
#include "CFCClass.h"
#include "CFCFunction.h"
#include "CFCMethod.h"
#include "CFCParamList.h"
#include "CFCType.h"
#include "CFCVariable.h"
#include "CFCUtil.h"

struct CFCBindClass {
    CFCBase base;
    CFCClass *client;
    char *full_callbacks_var;
    char *full_name_var;
    char *short_names_macro;
};

CFCBindClass*
CFCBindClass_new(CFCClass *client) {
    CFCBindClass *self 
        = (CFCBindClass*)CFCBase_allocate(sizeof(CFCBindClass),
                                          "Clownfish::Binding::Core::Class");
    return CFCBindClass_init(self, client);
}

CFCBindClass*
CFCBindClass_init(CFCBindClass *self, CFCClass *client) {
    CFCUTIL_NULL_CHECK(client);
    self->client = (CFCClass*)CFCBase_incref((CFCBase*)client);

    const char *full_vtable_var = CFCClass_full_vtable_var(client);
    const char *PREFIX = CFCClass_get_PREFIX(client);
    self->full_callbacks_var = (char*)MALLOCATE(strlen(full_vtable_var) + 20);
    self->full_name_var      = (char*)MALLOCATE(strlen(full_vtable_var) + 20);
    self->short_names_macro  = (char*)MALLOCATE(strlen(PREFIX) + 20);
    sprintf(self->full_callbacks_var, "%s_CALLBACKS", full_vtable_var);
    sprintf(self->full_name_var, "%s_CLASS_NAME", full_vtable_var);
    sprintf(self->short_names_macro, "%sUSE_SHORT_NAMES", PREFIX);

    return self;
}

void
CFCBindClass_destroy(CFCBindClass *self)
{
    FREEMEM(self->full_callbacks_var);
    FREEMEM(self->full_name_var);
    FREEMEM(self->short_names_macro);
    CFCBase_decref((CFCBase*)self->client);
    CFCBase_destroy((CFCBase*)self);
}

CFCClass*
CFCBindClass_get_client(CFCBindClass *self) {
    return self->client;
}

const char*
CFCBindClass_full_callbacks_var(CFCBindClass *self)
{
    return self->full_callbacks_var;
}

const char*
CFCBindClass_full_name_var(CFCBindClass *self)
{
    return self->full_name_var;
}

const char*
CFCBindClass_short_names_macro(CFCBindClass *self)
{
    return self->short_names_macro;
}

