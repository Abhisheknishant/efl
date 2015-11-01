#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <Ector.h>
#include <software/Ector_Software.h>

#include "ector_private.h"
#include "ector_software_private.h"

typedef struct _Ector_Renderer_Software_Base_Data Ector_Renderer_Software_Base_Data;
struct _Ector_Renderer_Software_Base_Data
{
};

static Ector_Renderer *
_ector_software_surface_ector_generic_surface_renderer_factory_new(Eo *obj,
                                                                   Ector_Software_Surface_Data *pd EINA_UNUSED,
                                                                   const Eo_Class *type)
{
   Eo* o = NULL;
   if (type == ECTOR_RENDERER_GENERIC_SHAPE_MIXIN)
     eo_add(o, ECTOR_RENDERER_SOFTWARE_SHAPE_CLASS, obj);
   else if (type == ECTOR_RENDERER_GENERIC_GRADIENT_LINEAR_MIXIN)
     eo_add(o, ECTOR_RENDERER_SOFTWARE_GRADIENT_LINEAR_CLASS, obj);
   else if (type == ECTOR_RENDERER_GENERIC_GRADIENT_RADIAL_MIXIN)
     eo_add(o, ECTOR_RENDERER_SOFTWARE_GRADIENT_RADIAL_CLASS, obj);
   else 
     ERR("Couldn't find class for type: %s\n", eo_class_name_get(type));
   return o;
}

static void
_ector_software_surface_context_set(Eo *obj EINA_UNUSED,
                                    Ector_Software_Surface_Data *pd,
                                    Software_Rasterizer *ctx)
{
   pd->software = ctx;
}

static Software_Rasterizer *
_ector_software_surface_context_get(Eo *obj EINA_UNUSED,
                                    Ector_Software_Surface_Data *pd)
{
   return pd->software;
}

void
_ector_software_surface_surface_set(Eo *obj EINA_UNUSED,
                                    Ector_Software_Surface_Data *pd,
                                    void *pixels, unsigned int width, unsigned int height)
{
   pd->software->fill_data.raster_buffer.buffer = pixels;
   pd->software->fill_data.raster_buffer.width = width;
   pd->software->fill_data.raster_buffer.height = height;
}

void
_ector_software_surface_surface_get(Eo *obj EINA_UNUSED,
                                    Ector_Software_Surface_Data *pd,
                                    void **pixels, unsigned int *width, unsigned int *height)
{
   *pixels = pd->software->fill_data.raster_buffer.buffer;
   *width = pd->software->fill_data.raster_buffer.width;
   *height = pd->software->fill_data.raster_buffer.height;
}

static Eo *
_ector_software_surface_eo_base_constructor(Eo *obj,
                                            Ector_Software_Surface_Data *pd EINA_UNUSED)
{
   obj = eo_super_eo_constructor( ECTOR_SOFTWARE_SURFACE_CLASS, obj);
   pd->software = (Software_Rasterizer *) calloc(1, sizeof(Software_Rasterizer));
   ector_software_rasterizer_init(pd->software);
  return obj;
}

static void
_ector_software_surface_eo_base_destructor(Eo *obj EINA_UNUSED,
                                           Ector_Software_Surface_Data *pd EINA_UNUSED)
{
   ector_software_rasterizer_done(pd->software);
   free(pd->software);
   pd->software = NULL;
   eo_super_eo_destructor(ECTOR_SOFTWARE_SURFACE_CLASS, obj);
}

static void
_ector_software_surface_ector_generic_surface_reference_point_set(Eo *obj EINA_UNUSED,
                                                                  Ector_Software_Surface_Data *pd,
                                                                  int x, int y)
{
   pd->x = x;
   pd->y = y;
}

#include "ector_software_surface.eo.c"
#include "ector_renderer_software_base.eo.c"
