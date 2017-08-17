#define EFL_GFX_SIZE_HINT_PROTECTED

#include "efl_ui_box_private.h"

// FIXME: Aspect support is not implemented
// FIXME: handle RTL? just invert the horizontal order?

typedef struct _Item_Calc Item_Calc;

struct _Item_Calc
{
   Evas_Object *obj;
   double weight[2];
   double align[2];
   int max[2];
   int pad[4];
   int want[2];
   Efl_Gfx_Size_Hint_Aspect aspect_type;
   int aspect[2];
   int id;
};

void
_efl_ui_box_custom_layout(Efl_Ui_Box *ui_box, Evas_Object_Box_Data *bd)
{
   Efl_Ui_Box_Data *pd = efl_data_scope_get(ui_box, EFL_UI_BOX_CLASS);
   Evas_Object_Box_Option *opt;
   Evas_Object *o;
   Eina_List *li;
   int wantw = 0, wanth = 0; // requested size
   int boxx, boxy, boxw, boxh;
   Item_Calc *items, *item;
   Eina_Bool horiz = efl_ui_dir_is_horizontal(pd->dir, EINA_FALSE);
   Eina_Bool zeroweight = EINA_FALSE;
   int id = 0, count, boxl = 0, boxr = 0, boxt = 0, boxb = 0;
   int length, want, pad, extra = 0, rounding = 0;
   double cur_pos = 0, weight[2] = { 0, 0 }, scale;
   double box_align[2];
   Eina_Bool box_fill[2] = { EINA_FALSE, EINA_FALSE };

   evas_object_geometry_get(ui_box, &boxx, &boxy, &boxw, &boxh);
   efl_gfx_size_hint_margin_get(ui_box, &boxl, &boxr, &boxt, &boxb);
   scale = evas_object_scale_get(ui_box);

   // Box align: used if "item has max size and fill" or "no item has a weight"
   // Note: cells always expand on the orthogonal direction
   box_align[0] = pd->align.h;
   box_align[1] = pd->align.v;
   if (box_align[0] < 0)
     {
        box_fill[0] = EINA_TRUE;
        box_align[0] = 0.5;
     }
   if (box_align[1] < 0)
     {
        box_fill[1] = EINA_TRUE;
        box_align[1] = 0.5;
     }

   count = eina_list_count(bd->children);
   if (!count)
     {
        evas_object_size_hint_min_set(ui_box, 0, 0);
        return;
     }

   items = alloca(count * sizeof(*items));
#ifdef DEBUG
   memset(items, 0, count * sizeof(*items));
#endif

   // scan all items, get their properties, calculate total weight & min size
   EINA_LIST_FOREACH(bd->children, li, opt)
     {
        item = &items[id];
        o = item->obj = opt->obj;

        efl_gfx_size_hint_weight_get(o, &item->weight[0], &item->weight[1]);
        efl_gfx_size_hint_align_get(o, &item->align[0], &item->align[1]);
        efl_gfx_size_hint_margin_get(o, &item->pad[0], &item->pad[1], &item->pad[2], &item->pad[3]);
        efl_gfx_size_hint_max_get(o, &item->max[0], &item->max[1]);
        efl_gfx_size_hint_combined_min_get(o, &item->want[0], &item->want[1]);
        efl_gfx_size_hint_aspect_get(o, &item->aspect_type, &item->aspect[0], &item->aspect[1]);

        if (item->aspect[0] <= 0 || item->aspect[1] <= 0)
          {
             if (item->aspect_type >= EFL_GFX_SIZE_HINT_ASPECT_HORIZONTAL)
               ERR("Invalid aspect parameter for obj: %p", item->obj);
             item->aspect[0] = item->aspect[1] = 0;
             item->aspect_type = EFL_GFX_SIZE_HINT_ASPECT_NONE;
          }

        if (item->aspect_type >= EFL_GFX_SIZE_HINT_ASPECT_HORIZONTAL)
          {
             double w, h;

             w = item->want[0];
             h = w * item->aspect[1] / item->aspect[0];
             if (h < item->want[1])
               {
                  h = item->want[1];
                  w = h * item->aspect[0] / item->aspect[1];
               }

             if (horiz)
               {
                  if ((item->align[1] < 0) && (h < boxh))
                    {
                       double w1, h1;
                       h1 = item->max[1] > 0 ? MIN(boxh, item->max[1]) : boxh;
                       h1 = MAX(h, h1);
                       w1 = h1 * item->aspect[0] / item->aspect[1];
                       w = item->max[0] > 0 ? MIN(w1, item->max[0]) : w1;
                    }
               }
             else
               {
                  if ((item->align[0] < 0) && (w < boxw))
                    {
                       double w1, h1;
                       w1 = item->max[0] > 0 ? MIN(boxw, item->max[0]) : boxw;
                       w1 = MAX(w, w1);
                       h1 = w1 * item->aspect[1] / item->aspect[0];
                       h = item->max[1] > 0 ? MIN(h1, item->max[1]) : h1;
                    }
               }
             item->want[0] = w;
             item->want[1] = h;
          }

        if (item->weight[0] < 0) item->weight[0] = 0;
        if (item->weight[1] < 0) item->weight[1] = 0;

        if (item->align[0] < 0) item->align[0] = -1;
        if (item->align[1] < 0) item->align[1] = -1;
        if (item->align[0] > 1) item->align[0] = 1;
        if (item->align[1] > 1) item->align[1] = 1;

        if (item->want[0] < 0) item->want[0] = 0;
        if (item->want[1] < 0) item->want[1] = 0;

        if (item->max[0] < 0) item->max[0] = INT_MAX;
        if (item->max[1] < 0) item->max[1] = INT_MAX;
        if (item->max[0] < item->want[0]) item->max[0] = item->want[0];
        if (item->max[1] < item->want[1]) item->max[1] = item->want[1];

        item->want[0] += item->pad[0] + item->pad[1];
        item->want[1] += item->pad[2] + item->pad[3];

        weight[0] += item->weight[0];
        weight[1] += item->weight[1];
        if (horiz)
          {
             wantw += item->want[0];
             if (item->want[1] > wanth)
               wanth = item->want[1];
          }
        else
          {
             wanth += item->want[1];
             if (item->want[0] > wantw)
               wantw = item->want[0];
          }

        item->id = id++;
     }

   // box outer margin
   boxw -= boxl + boxr;
   boxh -= boxt + boxb;
   boxx += boxl;
   boxy += boxt;

   // total space & available space
   if (horiz)
     {
        length = boxw;
        want = wantw;
        pad = pd->pad.scalable ? (pd->pad.h * scale) : pd->pad.h;
     }
   else
     {
        length = boxh;
        want = wanth;
        pad = pd->pad.scalable ? (pd->pad.v * scale) : pd->pad.v;
     }

   // padding can not be squeezed (note: could make it an option)
   length -= pad * (count - 1);

   // available space. if <0 we overflow
   extra = length - want;

   if (horiz)
     {
        evas_object_size_hint_min_set(ui_box,
                                      wantw + boxl + boxr + pad * (count - 1),
                                      wanth + boxt + boxb);
     }
   else
     {
        evas_object_size_hint_min_set(ui_box,
                                      wantw + boxl + boxr,
                                      wanth + pad * (count - 1) + boxt + boxb);
     }

   if (extra < 0) extra = 0;

   if (EINA_DBL_EQ(weight[!horiz], 0))
     {
        if (box_fill[!horiz])
          {
             // box is filled, set all weights to be equal
             zeroweight = EINA_TRUE;
          }
        else
          {
             // move bounding box according to box align
             cur_pos = extra * box_align[!horiz];
          }
        weight[!horiz] = count;
     }

   for (id = 0; id < count; id++)
     {
        double cx, cy, cw, ch, x, y, w, h;
        item = &items[id];

        // extra rounding up (compensate cumulative error)
        if ((item->id == (count - 1)) && (cur_pos - floor(cur_pos) >= 0.5))
          rounding = 1;

        if (horiz)
          {
             cx = boxx + cur_pos;
             cy = boxy;
             cw = item->want[0] + rounding + (zeroweight ? 1.0 : item->weight[0]) * extra / weight[0];
             ch = boxh;
             cur_pos += cw + pad;
          }
        else
          {
             cx = boxx;
             cy = boxy + cur_pos;
             cw = boxw;
             ch = item->want[1] + rounding + (zeroweight ? 1.0 : item->weight[1]) * extra / weight[1];
             cur_pos += ch + pad;
          }

        if (item->aspect_type >= EFL_GFX_SIZE_HINT_ASPECT_HORIZONTAL)
          {
             if (horiz)
               {
                  w = item->want[0];
                  h = item->want[1];
                  if (weight[0] > 0)
                    w = item->want[0] + extra * item->weight[0] / weight[0];
                  h = w * item->aspect[1] / item->aspect[0];

                  if (item->aspect_type == EFL_GFX_SIZE_HINT_ASPECT_BOTH ||
                      item->aspect_type == EFL_GFX_SIZE_HINT_ASPECT_VERTICAL)
                    {
                       if (h > boxh)
                         {
                            h = boxh;
                            w = h * item->aspect[0] / item->aspect[1];
                         }
                    }
               }
             else
               {
                  w = item->want[0];
                  h = item->want[1];
                  if (weight[1] > 0)
                    h = item->want[1] + extra * item->weight[1] / weight[1];
                  w = h * item->aspect[0] / item->aspect[1];

                  if (item->aspect_type == EFL_GFX_SIZE_HINT_ASPECT_BOTH ||
                      item->aspect_type == EFL_GFX_SIZE_HINT_ASPECT_HORIZONTAL)
                    {
                       if (w > boxw)
                         {
                            w = boxw;
                            h = w * item->aspect[1] / item->aspect[0];
                         }
                    }
               }

             if ((item->max[0] != INT_MAX) && (item->max[1] != INT_MAX))
               {
                  double mar, ar;
                  mar = item->max[0] / (double)item->max[1];
                  ar = item->aspect[0] / (double)item->aspect[1];
                  if (ar < mar)
                    {
                       if (h > item->max[1])
                         {
                            h = item->max[1];
                            w = h * item->aspect[0] / item->aspect[1];
                         }
                    }
                  else
                    {
                       if (w > item->max[0])
                         {
                            w = item->max[0];
                            h = w * item->aspect[1] / item->aspect[0];
                         }
                    }
               }
             else if (item->max[0] != INT_MAX)
               {
                  w = MIN(w, MAX(item->want[0], item->max[0]));
                  h = w * item->aspect[1] / item->aspect[0];
               }
             else
               {
                  h = MIN(h, MAX(item->want[1], item->max[1]));
                  w = h * item->aspect[0] / item->aspect[1];
               }
             w = w - item->pad[0] - item->pad[1];
             h = h - item->pad[2] - item->pad[3];

             if (item->align[0] < 0)
               x = cx + (cw - w) * 0.5 + item->pad[0];
             else
               x = cx + (cw - w) * item->align[0] + item->pad[0];

             if (item->align[1] < 0)
               y = cy + (ch - h) * 0.5 + item->pad[2];
             else
               y = cy + (ch - h) * item->align[1] + item->pad[2];
          }
        else
          {
             // horizontally
             if (item->max[0] < INT_MAX)
               {
                  w = MIN(MAX(item->want[0] - item->pad[0] - item->pad[1], item->max[0]), cw);
                  if (item->align[0] < 0)
                    {
                       // bad case: fill+max are not good together
                       x = cx + ((cw - w) * box_align[0]) + item->pad[0];
                    }
                  else
                    x = cx + ((cw - w) * item->align[0]) + item->pad[0];
               }
             else if (item->align[0] < 0)
               {
                  // fill x
                  w = cw - item->pad[0] - item->pad[1];
                  x = cx + item->pad[0];
               }
             else
               {
                  if (horiz && item->weight[0] > 0)
                    w = cw - item->pad[0] - item->pad[1];
                  else
                    w = item->want[0] - item->pad[0] - item->pad[1];
                  x = cx + ((cw - w) * item->align[0]) + item->pad[0];
               }

             // vertically
             if (item->max[1] < INT_MAX)
               {
                  h = MIN(MAX(item->want[1] - item->pad[2] - item->pad[3], item->max[1]), ch);
                  if (item->align[1] < 0)
                    {
                       // bad case: fill+max are not good together
                       y = cy + ((ch - h) * box_align[1]) + item->pad[2];
                    }
                  else
                    y = cy + ((ch - h) * item->align[1]) + item->pad[2];
               }
             else if (item->align[1] < 0)
               {
                  // fill y
                  h = ch - item->pad[2] - item->pad[3];
                  y = cy + item->pad[2];
               }
             else
               {
                  if (!horiz && item->weight[1] > 0)
                    h = ch - item->pad[2] - item->pad[3];
                  else
                    h = item->want[1] - item->pad[2] - item->pad[3];
                  y = cy + ((ch - h) * item->align[1]) + item->pad[2];
               }
          }

        //DBG("[%2d/%2d] cell: %.0f,%.0f %.0fx%.0f item: %.0f,%.0f %.0fx%.0f",
        //    id, count, cx, cy, cw, ch, x, y, w, h);
        evas_object_geometry_set(item->obj, x, y, w, h);
     }
}
