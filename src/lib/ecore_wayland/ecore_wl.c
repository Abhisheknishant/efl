#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <fcntl.h>
#include "ecore_wl_private.h"

/* local function prototypes */
static Eina_Bool _ecore_wl_shutdown(Eina_Bool close);
static Eina_Bool _ecore_wl_cb_idle_enterer(void *data);
static Eina_Bool _ecore_wl_cb_handle_data(void *data, Ecore_Fd_Handler *hdl);
static void _ecore_wl_cb_handle_global(void *data, struct wl_registry *registry, unsigned int id, const char *interface, unsigned int version EINA_UNUSED);
static Eina_Bool _ecore_wl_xkb_init(Ecore_Wl_Display *ewd);
static Eina_Bool _ecore_wl_xkb_shutdown(Ecore_Wl_Display *ewd);

/* local variables */
static int _ecore_wl_init_count = 0;
static const struct wl_registry_listener _ecore_wl_registry_listener =
{
   _ecore_wl_cb_handle_global,
   NULL // handle_global_remove
};

/* external variables */
int _ecore_wl_log_dom = -1;
Ecore_Wl_Display *_ecore_wl_disp = NULL;

EAPI int ECORE_WL_EVENT_MOUSE_IN = 0;
EAPI int ECORE_WL_EVENT_MOUSE_OUT = 0;
EAPI int ECORE_WL_EVENT_FOCUS_IN = 0;
EAPI int ECORE_WL_EVENT_FOCUS_OUT = 0;
EAPI int ECORE_WL_EVENT_WINDOW_CONFIGURE = 0;
EAPI int ECORE_WL_EVENT_DND_ENTER = 0;
EAPI int ECORE_WL_EVENT_DND_POSITION = 0;
EAPI int ECORE_WL_EVENT_DND_LEAVE = 0;
EAPI int ECORE_WL_EVENT_DND_DROP = 0;
EAPI int ECORE_WL_EVENT_DATA_SOURCE_TARGET = 0;
EAPI int ECORE_WL_EVENT_DATA_SOURCE_SEND = 0;
EAPI int ECORE_WL_EVENT_SELECTION_DATA_READY = 0;
EAPI int ECORE_WL_EVENT_DATA_SOURCE_CANCELLED = 0;
EAPI int ECORE_WL_EVENT_INTERFACES_BOUND = 0;

EAPI int
ecore_wl_init(const char *name)
{
   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   if (++_ecore_wl_init_count != 1) return _ecore_wl_init_count;

   if (!eina_init()) return --_ecore_wl_init_count;

   _ecore_wl_log_dom =
     eina_log_domain_register("ecore_wl", ECORE_WL_DEFAULT_LOG_COLOR);
   if (_ecore_wl_log_dom < 0)
     {
        EINA_LOG_ERR("Cannot create a log domain for Ecore Wayland");
        eina_shutdown();
        return --_ecore_wl_init_count;
     }

   if (!ecore_init())
     {
        ERR("Could not initialize ecore");
        eina_log_domain_unregister(_ecore_wl_log_dom);
        _ecore_wl_log_dom = -1;
        eina_shutdown();
        return --_ecore_wl_init_count;
     }

   if (!ecore_event_init())
     {
        ERR("Could not initialize ecore_event");
        eina_log_domain_unregister(_ecore_wl_log_dom);
        _ecore_wl_log_dom = -1;
        ecore_shutdown();
        eina_shutdown();
        return --_ecore_wl_init_count;
     }

   if (!ECORE_WL_EVENT_MOUSE_IN)
     {
        ECORE_WL_EVENT_MOUSE_IN = ecore_event_type_new();
        ECORE_WL_EVENT_MOUSE_OUT = ecore_event_type_new();
        ECORE_WL_EVENT_FOCUS_IN = ecore_event_type_new();
        ECORE_WL_EVENT_FOCUS_OUT = ecore_event_type_new();
        ECORE_WL_EVENT_WINDOW_CONFIGURE = ecore_event_type_new();
        ECORE_WL_EVENT_DND_ENTER = ecore_event_type_new();
        ECORE_WL_EVENT_DND_POSITION = ecore_event_type_new();
        ECORE_WL_EVENT_DND_LEAVE = ecore_event_type_new();
        ECORE_WL_EVENT_DND_DROP = ecore_event_type_new();
        ECORE_WL_EVENT_DATA_SOURCE_TARGET = ecore_event_type_new();
        ECORE_WL_EVENT_DATA_SOURCE_SEND = ecore_event_type_new();
        ECORE_WL_EVENT_SELECTION_DATA_READY = ecore_event_type_new();
        ECORE_WL_EVENT_DATA_SOURCE_CANCELLED = ecore_event_type_new();
        ECORE_WL_EVENT_INTERFACES_BOUND = ecore_event_type_new();
     }

   if (!(_ecore_wl_disp = malloc(sizeof(Ecore_Wl_Display))))
     {
        ERR("Could not allocate memory for Ecore_Wl_Display structure");
        eina_log_domain_unregister(_ecore_wl_log_dom);
        _ecore_wl_log_dom = -1;
        ecore_event_shutdown();
        ecore_shutdown();
        eina_shutdown();
        return --_ecore_wl_init_count;
     }

   memset(_ecore_wl_disp, 0, sizeof(Ecore_Wl_Display));

   if (!(_ecore_wl_disp->wl.display = wl_display_connect(name)))
     {
        ERR("Could not connect to Wayland display");
        eina_log_domain_unregister(_ecore_wl_log_dom);
        _ecore_wl_log_dom = -1;
        ecore_event_shutdown();
        ecore_shutdown();
        eina_shutdown();
        return --_ecore_wl_init_count;
     }

   _ecore_wl_disp->fd = wl_display_get_fd(_ecore_wl_disp->wl.display);

   _ecore_wl_disp->fd_hdl =
     ecore_main_fd_handler_add(_ecore_wl_disp->fd,
                               ECORE_FD_READ,
                               _ecore_wl_cb_handle_data, _ecore_wl_disp,
                               NULL, NULL);

   _ecore_wl_disp->idle_enterer =
     ecore_idle_enterer_add(_ecore_wl_cb_idle_enterer, _ecore_wl_disp);

   wl_list_init(&_ecore_wl_disp->inputs);
   wl_list_init(&_ecore_wl_disp->outputs);

   _ecore_wl_disp->wl.registry =
     wl_display_get_registry(_ecore_wl_disp->wl.display);
   wl_registry_add_listener(_ecore_wl_disp->wl.registry,
                            &_ecore_wl_registry_listener, _ecore_wl_disp);

   wl_display_dispatch(_ecore_wl_disp->wl.display);

   if (!_ecore_wl_xkb_init(_ecore_wl_disp))
     {
        ERR("Could not initialize XKB");
        free(_ecore_wl_disp);
        eina_log_domain_unregister(_ecore_wl_log_dom);
        _ecore_wl_log_dom = -1;
        ecore_event_shutdown();
        ecore_shutdown();
        eina_shutdown();
        return --_ecore_wl_init_count;
     }

   _ecore_wl_window_init();

   return _ecore_wl_init_count;
}

EAPI int
ecore_wl_shutdown(void)
{
   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   return _ecore_wl_shutdown(EINA_TRUE);
}

EAPI void
ecore_wl_flush(void)
{
//   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   wl_display_flush(_ecore_wl_disp->wl.display);
}

EAPI void
ecore_wl_sync(void)
{
//   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   wl_display_sync(_ecore_wl_disp->wl.display);
}

EAPI struct wl_shm *
ecore_wl_shm_get(void)
{
   return _ecore_wl_disp->wl.shm;
}

EAPI struct wl_display *
ecore_wl_display_get(void)
{
   return _ecore_wl_disp->wl.display;
}

EAPI void
ecore_wl_screen_size_get(int *w, int *h)
{
   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   if (w) *w = 0;
   if (h) *h = 0;

   if (!_ecore_wl_disp->output) return;

   if (w) *w = _ecore_wl_disp->output->allocation.w;
   if (h) *h = _ecore_wl_disp->output->allocation.h;
}

/* @since 1.2 */
EAPI void
ecore_wl_pointer_xy_get(int *x, int *y)
{
   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   _ecore_wl_input_pointer_xy_get(x, y);
}

EAPI int
ecore_wl_dpi_get(void)
{
   int w, mw;

   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   if (!_ecore_wl_disp->output) return 75;

   mw = _ecore_wl_disp->output->mw;
   if (mw <= 0) return 75;

   w = _ecore_wl_disp->output->allocation.w;
   /* FIXME: NB: Hrrrmmm, need to verify this. xorg code is using a different
    * formula to calc this */
   return (((w * 254) / mw) + 5) / 10;
}

EAPI void
ecore_wl_display_iterate(void)
{
   wl_display_dispatch(_ecore_wl_disp->wl.display);
}

EAPI struct wl_cursor *
ecore_wl_cursor_get(const char *cursor_name)
{
   if ((!_ecore_wl_disp) || (!_ecore_wl_disp->cursor_theme))
     return NULL;

   return wl_cursor_theme_get_cursor(_ecore_wl_disp->cursor_theme,
                                     cursor_name);
}

/* local functions */
static Eina_Bool
_ecore_wl_shutdown(Eina_Bool close)
{
   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   if (--_ecore_wl_init_count != 0) return _ecore_wl_init_count;
   if (!_ecore_wl_disp) return _ecore_wl_init_count;

   _ecore_wl_window_shutdown();

   if (_ecore_wl_disp->fd_hdl)
     ecore_main_fd_handler_del(_ecore_wl_disp->fd_hdl);
   if (_ecore_wl_disp->idle_enterer)
      ecore_idle_enterer_del(_ecore_wl_disp->idle_enterer);

   if (close)
     {
        Ecore_Wl_Output *out, *tout;
        Ecore_Wl_Input *in, *tin;

        wl_list_for_each_safe(out, tout, &_ecore_wl_disp->outputs, link)
          _ecore_wl_output_del(out);

        wl_list_for_each_safe(in, tin, &_ecore_wl_disp->inputs, link)
          _ecore_wl_input_del(in);

        _ecore_wl_xkb_shutdown(_ecore_wl_disp);

        if (_ecore_wl_disp->wl.shell)
          wl_shell_destroy(_ecore_wl_disp->wl.shell);
        if (_ecore_wl_disp->wl.shm) wl_shm_destroy(_ecore_wl_disp->wl.shm);
        if (_ecore_wl_disp->wl.data_device_manager)
          wl_data_device_manager_destroy(_ecore_wl_disp->wl.data_device_manager);
        if (_ecore_wl_disp->wl.compositor)
          wl_compositor_destroy(_ecore_wl_disp->wl.compositor);
        if (_ecore_wl_disp->wl.display)
          {
             wl_registry_destroy(_ecore_wl_disp->wl.registry);
             wl_display_flush(_ecore_wl_disp->wl.display);
             wl_display_disconnect(_ecore_wl_disp->wl.display);
          }
        free(_ecore_wl_disp);
     }

   ecore_event_shutdown();
   ecore_shutdown();

   eina_log_domain_unregister(_ecore_wl_log_dom);
   _ecore_wl_log_dom = -1;
   eina_shutdown();

   return _ecore_wl_init_count;
}

static Eina_Bool
_ecore_wl_cb_idle_enterer(void *data)
{
   Ecore_Wl_Display *ewd;
   int ret;

   if (!(ewd = data)) return ECORE_CALLBACK_RENEW;

   ret = wl_display_flush(ewd->wl.display);
   if ((ret < 0) && (errno == EAGAIN))
     {
        ecore_main_fd_handler_active_set(ewd->fd_hdl, 
                                         (ECORE_FD_READ | ECORE_FD_WRITE));
     }
   else if (ret < 0)
     {
      /* FIXME: need do error processing? */
     }

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_ecore_wl_cb_handle_data(void *data, Ecore_Fd_Handler *hdl)
{
   Ecore_Wl_Display *ewd;
   int ret;

   /* LOGFN(__FILE__, __LINE__, __FUNCTION__); */

   if (!(ewd = data)) return ECORE_CALLBACK_RENEW;

   /* FIXME: This should also catch ECORE_FD_ERROR and exit */

   /* wl_display_dispatch_pending(ewd->wl.display); */

   if (ecore_main_fd_handler_active_get(hdl, ECORE_FD_READ))
     wl_display_dispatch(ewd->wl.display);
   else if (ecore_main_fd_handler_active_get(hdl, ECORE_FD_WRITE))
     {
        ret = wl_display_flush(ewd->wl.display);
        if (ret == 0)
          ecore_main_fd_handler_active_set(hdl, ECORE_FD_READ);
        else if ((ret == -1) && (errno != EAGAIN))
          {
            /* FIXME: need do error processing? */
          }
     }

   return ECORE_CALLBACK_RENEW;
}

static void
_ecore_wl_cb_handle_global(void *data, struct wl_registry *registry, unsigned int id, const char *interface, unsigned int version EINA_UNUSED)
{
   Ecore_Wl_Display *ewd;

   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   ewd = data;

   if (!strcmp(interface, "wl_compositor"))
     {
        ewd->wl.compositor =
          wl_registry_bind(registry, id, &wl_compositor_interface, 1);
     }
   else if (!strcmp(interface, "wl_output"))
     _ecore_wl_output_add(ewd, id);
   else if (!strcmp(interface, "wl_seat"))
     _ecore_wl_input_add(ewd, id);
   else if (!strcmp(interface, "wl_shell"))
     {
        ewd->wl.shell =
          wl_registry_bind(registry, id, &wl_shell_interface, 1);
     }
   else if (!strcmp(interface, "wl_shm"))
     {
        ewd->wl.shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);

        /* FIXME: We should not hard-code a cursor size here, and we should
         * also import the theme name from a config or env variable */
        ewd->cursor_theme = wl_cursor_theme_load(NULL, 32, ewd->wl.shm);
     }
   else if (!strcmp(interface, "wl_data_device_manager"))
     {
        ewd->wl.data_device_manager =
          wl_registry_bind(registry, id, &wl_data_device_manager_interface, 1);
     }

   if ((ewd->wl.compositor) && (ewd->wl.shm) && (ewd->wl.shell))
     {
        Ecore_Wl_Event_Interfaces_Bound *ev;

        if (!(ev = calloc(1, sizeof(Ecore_Wl_Event_Interfaces_Bound))))
          return;

        ev->compositor = (ewd->wl.compositor != NULL);
        ev->shm = (ewd->wl.shm != NULL);
        ev->shell = (ewd->wl.shell != NULL);

        ecore_event_add(ECORE_WL_EVENT_INTERFACES_BOUND, ev, NULL, NULL);
     }
}

static Eina_Bool
_ecore_wl_xkb_init(Ecore_Wl_Display *ewd)
{
   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   if (!(ewd->xkb.context = xkb_context_new(0)))
     return EINA_FALSE;

   return EINA_TRUE;
}

static Eina_Bool
_ecore_wl_xkb_shutdown(Ecore_Wl_Display *ewd)
{
   LOGFN(__FILE__, __LINE__, __FUNCTION__);

   xkb_context_unref(ewd->xkb.context);

   return EINA_TRUE;
}

struct wl_data_source *
_ecore_wl_create_data_source(Ecore_Wl_Display *ewd)
{
   return wl_data_device_manager_create_data_source(ewd->wl.data_device_manager);
}
