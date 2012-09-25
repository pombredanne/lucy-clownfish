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

#ifndef H_CFCMEMPOOL
#define H_CFCMEMPOOL

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CFCMemPool CFCMemPool;

CFCMemPool*
CFCMemPool_new(size_t arena_size);

CFCMemPool*
CFCMemPool_init(CFCMemPool *self, size_t arena_size);

void*
CFCMemPool_allocate(CFCMemPool *self, size_t size);

void
CFCMemPool_destroy(CFCMemPool *self);

#ifdef __cplusplus
}
#endif

#endif /* H_CFCMEMPOOL */

