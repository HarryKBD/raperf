#include <cairo.h>
#include <gtk/gtk.h>

#ifdef __cplusplus
    extern "C" {
#endif

typedef struct tagGTK_DATA{
    pthread_t draw_thread;
    GtkWidget *g_main_window = NULL;
    cairo_surface_t *g_image = NULL;
    char *rgb32buf;
}GTK_DATA;

GTK_DATA *init_display();
void cleanup_display(GTK_DATA *p);
int display_image(GTK_DATA *p, char *rgb24buf, int len);

#ifdef __cplusplus
    }
#endif
