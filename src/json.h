
/* vim: set et ts=3 sw=3 sts=3 ft=c:
 *
 * Copyright (C) 2012, 2013, 2014 James McLaughlin et al.  All rights reserved.
 * https://github.com/udp/json-parser
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _JSON_H
#define _JSON_H

#ifndef json_char
   #define json_char char
#endif

#ifndef json_int_t
   #ifndef _MSC_VER
      #include <inttypes.h>
      #define json_int_t int64_t
   #else
      #define json_int_t __int64
   #endif
#endif

#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

struct json_settings {
   unsigned long max_memory;
   int settings;

   /* Custom allocator support (leave null to use malloc/free)
    */

   void * (* mem_alloc) (size_t, int zero, void * user_data);
   void (* mem_free) (void *, void * user_data);

   void * user_data;  /* will be passed to mem_alloc and mem_free */

   size_t value_extra;  /* how much extra space to allocate for values? */
};

#define json_enable_comments  0x01

typedef enum json_type {
   json_type_null,
   json_type_object,
   json_type_array,
   json_type_integer,
   json_type_double,
   json_type_string,
   json_type_boolean,
} json_type;

struct json_object;

extern const struct json_object json_object_none;

struct json_object_entry {
    json_char * name;
    // unsigned int name_length;
    struct json_object * object;
};

struct json_object {
   struct json_object * parent;

   json_type type;

   union
   {
      int boolean;
      json_int_t integer;
      double dbl;

      struct {
         unsigned int length;
         json_char * ptr; /* null terminated */
      } string;

      struct {
         unsigned int length;

         struct json_object_entry * entries;

         #if defined(__cplusplus) && __cplusplus >= 201103L
         decltype(entries) begin () const {
             return entries;
         }
         decltype(entries) end () const {
             return entries + length;
         }
         #endif
      } object;

      struct {
         size_t length;
         struct json_object ** items;

         #if defined(__cplusplus) && __cplusplus >= 201103L
         decltype(items) begin () const {
             return items;
         }
         decltype(items) end () const {
             return items + length;
         }
         #endif
      } array;
   } u;

   union {
      struct json_object * next_alloc;
      void * object_mem;
   } _reserved;

   #ifdef JSON_TRACK_SOURCE

      /* Location of the value in the source JSON
       */
      unsigned int line, col;

   #endif


   /* Some C++ operator sugar */

   #ifdef __cplusplus

      public:

         inline json_object () {
             memset (this, 0, sizeof (struct json_object));
         }

         inline const struct json_object &operator [] (int index) const
         {
            if (type != json_type_array || index < 0
                     || ((unsigned int) index) >= u.array.length)
            {
               return json_object_none;
            }

            return *u.array.items [index];
         }

         inline const struct json_object &operator [] (const char * index) const
         {
            if (type != json_type_object)
               return json_object_none;

            for (unsigned int i = 0; i < u.object.length; ++ i)
               if (!strcmp (u.object.entries [i].name, index))
                  return *u.object.entries [i].object;

            return json_object_none;
         }

         inline operator const char * () const
         {
            switch (type)
            {
               case json_type_string:
                  return u.string.ptr;

               default:
                  return "";
            };
         }

         inline operator json_int_t () const
         {
            switch (type)
            {
               case json_type_integer:
                  return u.integer;

               case json_type_double:
                  return (json_int_t) u.dbl;

               default:
                  return 0;
            };
         }

         inline operator bool () const
         {
            if (type != json_type_boolean)
               return false;

            return u.boolean != 0;
         }

         inline operator double () const
         {
            switch (type)
            {
               case json_type_integer:
                  return (double) u.integer;

               case json_type_double:
                  return u.dbl;

               default:
                  return 0;
            };
         }

   #endif

};

struct json_object * json_parse (const json_char * json, size_t length);

#define json_error_max 128
struct json_object * json_parse_ex (struct json_settings * settings,
                            const json_char * json,
                            size_t length,
                            char * error);

void json_object_free (struct json_object *);


/* Not usually necessary, unless you used a custom mem_alloc and now want to
 * use a custom mem_free.
 */
void json_object_free_ex (struct json_settings * settings, struct json_object *);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
