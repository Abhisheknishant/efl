/* Portions of this code have been derived from Weston
 *
 * Copyright © 2008-2012 Kristian Høgsberg
 * Copyright © 2010-2012 Intel Corporation
 * Copyright © 2010-2011 Benjamin Franzke
 * Copyright © 2011-2012 Collabora, Ltd.
 * Copyright © 2010 Red Hat <mjg@redhat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "evas_common_private.h"
#include "evas_private.h"
#include "evas_engine.h"
#include <sys/mman.h>

typedef struct _Shm_Pool Shm_Pool;
struct _Shm_Pool
{
   struct wl_shm_pool *pool;
   size_t size, used;
   void *data;
};

typedef struct _Shm_Data Shm_Data;
struct _Shm_Data
{
   struct wl_buffer *buffer;
   Shm_Pool *pool;
   void *map;
};

typedef struct _Shm_Leaf Shm_Leaf;
struct _Shm_Leaf
{
   int w, h, busy, age;
   Shm_Data *data;
   Shm_Pool *resize_pool;
   Eina_Bool valid : 1;
   Eina_Bool reconfigure : 1;
   Eina_Bool drawn : 1;
};

typedef struct _Shm_Surface Shm_Surface;
struct _Shm_Surface
{
   struct wl_display *disp;
   struct wl_shm *shm;
   struct wl_surface *surface;
   int w, h;
   int num_buff;
   int compositor_version;

   Shm_Leaf leaf[MAX_BUFFERS];
   Shm_Leaf *current;

   Eina_Bool alpha : 1;
};

static Eina_Bool _shm_leaf_create(Shm_Surface *surface, Shm_Leaf *leaf, int w, int h);
static void _shm_leaf_release(Shm_Leaf *leaf);
static void _shm_leaf_destroy(Shm_Leaf *leaf);

static struct wl_shm_pool *
_shm_pool_make(struct wl_shm *shm, int size, void **data)
{
   struct wl_shm_pool *pool;
   static const char tmp[] = "/evas-wayland_shm-XXXXXX";
   const char *path;
   char *name;
   int fd = 0;

   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   /* check for valid wl_shm */
   if (!shm) return NULL;

   /* create tmp file name */
   if ((path = getenv("XDG_RUNTIME_DIR")))
     {
        if ((name = malloc(strlen(path) + sizeof(tmp))))
          strcpy(name, path);
     }
   else
     {
        if ((name = malloc(strlen("/tmp") + sizeof(tmp))))
          strcpy(name, "/tmp");
     }

   if (!name) return NULL;

   strcat(name, tmp);

   /* try to create tmp file */
   if ((fd = mkstemp(name)) < 0)
     {
        ERR("Could not create temporary file: %m");
        free(name);
        return NULL;
     }

   unlink(name);
   free(name);

   /* try to truncate file to size */
   if (ftruncate(fd, size) < 0)
     {
        ERR("Could not truncate temporary file: %m");
        goto fd_err;
     }

   /* try to mmap the file */
   *data = mmap(NULL, size, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, 0);
   if (*data == MAP_FAILED)
     {
        ERR("Could not mmap temporary file: %m");
        goto fd_err;
     }

   /* NB: Commented out. Used for debugging rendering issues */
   /* memset(*data, 127, size); */

   /* create wl_shm_pool using fd */
   pool = wl_shm_create_pool(shm, fd, size);

   close(fd);

   return pool;

fd_err:
   close(fd);
   return NULL;
}

static Shm_Pool *
_shm_pool_create(struct wl_shm *shm, size_t size)
{
   Shm_Pool *pool;

   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   if (!(pool = malloc(sizeof(Shm_Pool)))) return NULL;

   pool->pool = _shm_pool_make(shm, size, &pool->data);
   if (!pool->pool) goto err;

   pool->size = size;
   pool->used = 0;

   return pool;

err:
   free(pool);
   return NULL;
}

static void 
_shm_pool_destroy(Shm_Pool *pool)
{
   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   munmap(pool->data, pool->size);
   wl_shm_pool_destroy(pool->pool);
   free(pool);
}

static void *
_shm_pool_allocate(Shm_Pool *pool, size_t size, int *offset)
{
   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   if ((pool->used + size) > pool->size)
     {
        WRN("Shm Pool Too Small");
        return NULL;
     }

   *offset = pool->used;
   pool->used += size;

   return (char *)pool->data + *offset;
}

static void 
_shm_pool_reset(Shm_Pool *pool)
{
   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   pool->used = 0;
}

/* static void  */
/* _shm_frame_release(void *data, struct wl_callback *callback, uint32_t timestamp EINA_UNUSED) */
/* { */
/*    Shm_Surface *surf; */

/*    LOGFN(__FILE__, __LINE__, __FUNCTION__); */

/*    if (!(surf = data)) return; */

/*    wl_callback_destroy(callback); */
/* } */

/* static const struct wl_callback_listener _shm_frame_listener =  */
/* { */
/*    _shm_frame_release */
/* }; */

static Shm_Data *
_shm_data_create_from_pool(Shm_Pool *pool, int w, int h, Eina_Bool alpha)
{
   Shm_Data *data;
   int len, offset;
   uint32_t wl_format = WL_SHM_FORMAT_XRGB8888;

   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   /* try to malloc space for data */
   if (!(data = malloc(sizeof(Shm_Data))))
     {
        ERR("Could not allocate space for data");
        return NULL;
     }

   len = (w * sizeof(int)) * h;
   data->pool = NULL;

   if (!(data->map = _shm_pool_allocate(pool, len, &offset)))
     {
        ERR("Could not map leaf data");
        goto err;
     }

   if (alpha) 
     wl_format = WL_SHM_FORMAT_ARGB8888;

   data->buffer = 
     wl_shm_pool_create_buffer(pool->pool, offset, w, h, 
                               (w * sizeof(int)), wl_format);
   if (!data->buffer)
     {
        ERR("Could not create buffer from pool");
        goto err;
     }

   return data;

err:
   free(data);
   return NULL;
}

static void 
_shm_data_create(Shm_Pool *alt_pool, Shm_Data **ret, Shm_Surface *surface, int w, int h)
{
   Shm_Pool *pool;
   Shm_Data *data;

   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   if (ret) *ret = NULL;

   if (alt_pool)
     {
        _shm_pool_reset(alt_pool);
        if ((data = _shm_data_create_from_pool(alt_pool, w, h, surface->alpha)))
          goto out;
     }

   if (!(pool = _shm_pool_create(surface->shm, ((w * sizeof(int)) * h))))
     {
        ERR("Could not create shm pool");
        return;
     }

   if (!(data = _shm_data_create_from_pool(pool, w, h, surface->alpha)))
     {
        ERR("Could not create data from pool");
        _shm_pool_destroy(pool);
        return;
     }

   data->pool = pool;

out:
   if (ret) *ret = data;
}

static void 
_shm_data_destroy(Shm_Data *data)
{
   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   if (data->buffer) wl_buffer_destroy(data->buffer);
   if (data->pool) _shm_pool_destroy(data->pool);
   free(data);
}

static void 
_shm_buffer_release(void *data, struct wl_buffer *buffer)
{
   Shm_Surface *surf;
   Shm_Leaf *leaf;
   int i = 0;

   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   surf = data;
   for (; i < surf->num_buff; i++)
     {
        leaf = &surf->leaf[i];
        if ((leaf->data) && (leaf->data->buffer == buffer))
          {
//             DBG("Buffer Released: %d", (int)(leaf - &surf->leaf[0]));
             leaf->busy = 0;

             if (leaf->reconfigure)
               {
                  _shm_leaf_release(leaf);
                  _shm_leaf_create(surf, leaf, surf->w, surf->h);
               }

             break;
          }
     }
}

static const struct wl_buffer_listener _shm_buffer_listener = 
{
   _shm_buffer_release
};

static Eina_Bool 
_shm_leaf_create(Shm_Surface *surface, Shm_Leaf *leaf, int w, int h)
{
   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   _shm_data_create(leaf->resize_pool, &leaf->data, surface, w, h);
   if (!leaf->data)
     {
        CRI("Failed to create leaf data");
        abort();
     }

   leaf->w = w;
   leaf->h = h;
   leaf->valid = EINA_TRUE;
   leaf->drawn = EINA_FALSE;
   leaf->age = 0;
   wl_buffer_add_listener(leaf->data->buffer, &_shm_buffer_listener, surface);

   return EINA_TRUE;
}

static void 
_shm_leaf_release(Shm_Leaf *leaf)
{
   LOGFN(__FILE__, __LINE__, __FUNCTION__);
   Shm_Pool *resize_pool;

   /* if we delete resize_pool here we blow away the clever optimization
    * it provides (and end up doing two allocations per resize when we
    * might have done none at all).
    */
   resize_pool = leaf->resize_pool;
   if (leaf->data) _shm_data_destroy(leaf->data);
   memset(leaf, 0, sizeof(*leaf));
   leaf->valid = EINA_FALSE;
   leaf->resize_pool = resize_pool;
}

static void
_shm_leaf_destroy(Shm_Leaf *leaf)
{
   _shm_leaf_release(leaf);
   if (leaf->resize_pool) _shm_pool_destroy(leaf->resize_pool);
   leaf->resize_pool = NULL;
}

void 
_evas_shm_surface_destroy(Surface *surface)
{
   int i = 0;

   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   if (!surface) return;

   for (; i < surface->surf.shm->num_buff; i++)
     _shm_leaf_destroy(&surface->surf.shm->leaf[i]);

   free(surface->surf.shm);
   free(surface);
}

void 
_evas_shm_surface_reconfigure(Surface *s, int w, int h, int num_buff, uint32_t flags)
{
   Shm_Surface *surface;
   int i = 0, resize = 0;

   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   surface = s->surf.shm;
   resize = !!flags;

   for (; i < surface->num_buff; i++)
     {
        /* don't resize any busy leafs */
        if (surface->leaf[i].busy)
          {
             surface->leaf[i].reconfigure = EINA_TRUE;
             continue;
          }

        /* clear this leaf */
        _shm_leaf_release(&surface->leaf[i]);
     }

   surface->w = w;
   surface->h = h;
   surface->num_buff = num_buff;

   for (i = 0; i < surface->num_buff; i++)
     {
        if (surface->leaf[i].busy) continue;

        if ((resize) && (!surface->leaf[i].resize_pool))
          {
             surface->leaf[i].resize_pool = 
               _shm_pool_create(surface->shm, 6 * 1024 * 1024);
          }

        if (!_shm_leaf_create(surface, &surface->leaf[i], w, h))
          {
             CRI("Failed to create leaf data");
             abort();
          }
     }
}

static Shm_Leaf *
_evas_shm_surface_wait(Shm_Surface *surface)
{
   int iterations = 0, i;

   while (iterations++ < 10)
     {
        for (i = 0; i < surface->num_buff; i++)
          {
             if (surface->leaf[i].busy) continue;
             if (surface->leaf[i].valid) return &surface->leaf[i];
          }

        wl_display_dispatch_pending(surface->disp);
     }
   return NULL;
}

int
_evas_shm_surface_assign(Surface *s)
{
   int i;
   Shm_Surface *surface;

   surface = s->surf.shm;
   surface->current = _evas_shm_surface_wait(surface);

   /* If we ran out of buffers we're in trouble, reset all ages */
   if (!surface->current)
     {
        WRN("No free SHM buffers, dropping a frame");
        for (i = 0; i < surface->num_buff; i++)
          {
             if (surface->leaf[i].valid)
               {
                  surface->leaf[i].drawn = EINA_FALSE;
                  surface->leaf[i].age = 0;
               }
          }
        return 0;
     }

   /* Increment ages of all valid buffers */
   for (i = 0; i < surface->num_buff; i++)
     {
        if (surface->leaf[i].valid && surface->leaf[i].drawn)
          {
             surface->leaf[i].age++;
             if (surface->leaf[i].age > surface->num_buff)
               {
                  surface->leaf[i].age = 0;
                  surface->leaf[i].drawn = EINA_FALSE;
               }
          }
     }

   return surface->current->age;
}

void *
_evas_shm_surface_data_get(Surface *s, int *w, int *h)
{
   Shm_Surface *surface;
   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   surface = s->surf.shm;
   if (w) *w = 0;
   if (h) *h = 0;

   if (!surface->current)
     {
        /* WRN("All buffers held by server"); */
        return NULL;
     }

   /* DBG("Leaf Data Get %d", (int)(leaf - &surface->leaf[0])); */

   if (w) *w = surface->current->w;
   if (h) *h = surface->current->h;

   return surface->current->data->map;
}

void
_evas_shm_surface_post(Surface *s, Eina_Rectangle *rects, unsigned int count)
{
   /* struct wl_callback *frame_cb; */
   Shm_Surface *surface;
   Shm_Leaf *leaf;

   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   surface = s->surf.shm;
   leaf = surface->current;
   if (!leaf) return;

   if (!surface->surface) return;

   wl_surface_attach(surface->surface, leaf->data->buffer, 0, 0);

   if ((rects) && (count > 0))
     {
        unsigned int k = 0;

        for (; k < count; k++)
          if (surface->compositor_version >= WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION)
            wl_surface_damage_buffer(surface->surface,
                                      rects[k].x, rects[k].y,
                                      rects[k].w, rects[k].h);
          else
            wl_surface_damage(surface->surface,
                               rects[k].x, rects[k].y,
                               rects[k].w, rects[k].h);
     }
   else
     if (surface->compositor_version >= WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION)
       wl_surface_damage_buffer(surface->surface, 0, 0, leaf->w, leaf->h);
     else
       wl_surface_damage(surface->surface, 0, 0, leaf->w, leaf->h);

   /* frame_cb = wl_surface_frame(surface->surface); */
   /* wl_callback_add_listener(frame_cb, &_shm_frame_listener, surface); */

   wl_surface_commit(surface->surface);

   leaf->busy = EINA_TRUE;
   leaf->drawn = EINA_TRUE;
   leaf->age = 0;
   surface->current = NULL;
}

Surface *
_evas_shm_surface_create(struct wl_display *disp, struct wl_shm *shm, struct wl_surface *surface, int w, int h, int num_buff, Eina_Bool alpha, int compositor_version)
{
   Surface *s;
   Shm_Surface *surf;
   int i = 0;

   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   if (!(s = calloc(1, sizeof(Surface)))) return NULL;
   if (!(s->surf.shm = calloc(1, sizeof(Shm_Surface)))) goto err;
   s->type = SURFACE_SHM;
   surf = s->surf.shm;

   surf->w = w;
   surf->h = h;
   surf->disp = disp;
   surf->shm = shm;
   surf->surface = surface;
   surf->num_buff = num_buff;
   surf->alpha = alpha;
   surf->compositor_version = compositor_version;

   /* create surface buffers */
   for (; i < surf->num_buff; i++)
     {
        if (!_shm_leaf_create(surf, &(surf->leaf[i]), w, h))
          {
             ERR("Could not create surface leaf");
             goto err;
          }
     }

   s->funcs.destroy = _evas_shm_surface_destroy;
   s->funcs.reconfigure = _evas_shm_surface_reconfigure;
   s->funcs.data_get = _evas_shm_surface_data_get;
   s->funcs.assign = _evas_shm_surface_assign;
   s->funcs.post = _evas_shm_surface_post;

   return s;

err:
   _evas_shm_surface_destroy(s);
   return NULL;
}
