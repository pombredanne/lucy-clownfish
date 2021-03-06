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

#ifndef H_CFCGO
#define H_CFCGO

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CFCGo CFCGo;
struct CFCParcel;
struct CFCHierarchy;

/** Clownfish::CFC::Binding::Go - Go bindings for a
 * Clownfish::CFC::Model::Hierarchy.
 *
 * Clownfish::CFC::Binding::Go presents an interface for auto-generating CGO
 * code to bind C code for a Clownfish parcel to Go.
 */

/**
 * @param hierarchy A CFCHierarchy.
 */

CFCGo*
CFCGo_new(struct CFCHierarchy *hierarchy);

/** Set the text which will be prepended to generated CGO files -- for
 * instance, an "autogenerated file" warning.
 */
void
CFCGo_set_header(CFCGo *self, const char *header);

/** Set the text which will be appended to the end of generated CGO files.
 */
void
CFCGo_set_footer(CFCGo *self, const char *footer);

/** If true, suppress autogeneration of the init() function which invokes the
 * Clownfish parcel bootstrapping.  The caller then becomes responsible for
 * writing their own init() which performs the bootstrapping.
 */
void
CFCGo_set_suppress_init(CFCGo *self, int suppress_init);

/** Generate CGO bindings for the specified parcel.
 */
void
CFCGo_write_bindings(CFCGo *self, struct CFCParcel *parcel, const char *dest);

/** Associate a Clownfish parcel with a Go package string suitable for use in
 * a Go `import` statement.
 */
void
CFCGo_register_parcel_package(const char *parcel, const char *package);

#ifdef __cplusplus
}
#endif

#endif /* H_CFCGO */

