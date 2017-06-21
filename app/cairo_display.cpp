#include <cairo.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <X11/Xlib.h>

#include "cairo_display.h"

#define WINDOW_SIZE_X  800
#define WINDOW_SIZE_Y  600

#define IMAGE_SIZE_X 1920
#define IMAGE_SIZE_Y 1920

static void do_drawing(GTK_DATA *p, cairo_t *cr)
{
    if(!p->g_image){
        printf("Image is not ready.\n");
        return;
    }
    cairo_scale(cr, WINDOW_SIZE_X/(float)IMAGE_SIZE_X, WINDOW_SIZE_Y/(float)IMAGE_SIZE_Y);
    cairo_set_source_surface(cr, p->g_image , 0, 0);
    cairo_paint(cr);    
}

static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr,
    gpointer user_data)
{
    GTK_DATA *p = (GTK_DATA *)user_data;
    do_drawing(p, cr);
    return FALSE;
}

static char *convert_RGB24_to_RGB32(char *p, int n, int w, int h, char *rgb32buf){
    int rgb24_size = w * h * 3;
    if(n != rgb24_size){
        printf("input is not correct %d. expected %d\n", n, 1920*1920*3);
        return NULL;
    }
    memset(rgb32buf, 0x00, 1920 * 1920 * 4);
    char *dst = rgb32buf;
    char *src = p;
    while(1){
        memcpy(dst, src, 3);
        src += 3;
        dst += 4;
        if(src >= (p+n)) {
            break;
        }
    }
    return rgb32buf;
}

static cairo_surface_t *cairo_image_surface_create_from_rgb24(char *rgb24buf, int len, char *rgb32buf){
    int stride;

    //just for information
    stride = cairo_format_stride_for_width (CAIRO_FORMAT_RGB24, 1920);

    convert_RGB24_to_RGB32(rgb24buf, len, 1920, 1920, rgb32buf);
    return cairo_image_surface_create_for_data ((unsigned char *)rgb32buf, CAIRO_FORMAT_RGB24, 1920, 1920, stride);
}

void *draw_thread_main(void *p){
  GtkWidget *darea;

  GTK_DATA *pd = (GTK_DATA *)p;

  printf("Creating draw thread...\n");

  pd->g_main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  darea = gtk_drawing_area_new();
  gtk_container_add(GTK_CONTAINER (pd->g_main_window), darea);

  g_signal_connect(G_OBJECT(darea), "draw", 
      G_CALLBACK(on_draw_event), p); 
  g_signal_connect(pd->g_main_window, "destroy",
      G_CALLBACK (gtk_main_quit), p);

  gtk_window_set_position(GTK_WINDOW(pd->g_main_window), GTK_WIN_POS_CENTER);
  //gtk_window_set_default_size(GTK_WINDOW(pd->g_main_window), 640, 480); 
  //gtk_window_set_default_size(GTK_WINDOW(pd->g_main_window), 1024, 768); 
  gtk_window_set_default_size(GTK_WINDOW(pd->g_main_window), WINDOW_SIZE_X, WINDOW_SIZE_Y); 
  gtk_window_set_title(GTK_WINDOW(pd->g_main_window), pd->title);

  gtk_widget_show_all(pd->g_main_window);
  gtk_widget_queue_draw(pd->g_main_window);

  gtk_main();

  printf("init cairo gtk display is done. \n");

  /*
  printf("after gtk_main\n");
  g_loop = 0;

  cairo_surface_destroy(glob.image);
  cairo_surface_destroy(image2);
  */
  return NULL;
}

void cleanup_display(GTK_DATA *p){
	if(!p){
		return;
	}
    cairo_surface_destroy(p->g_image);
    free(p->rgb32buf);
    pthread_detach(p->draw_thread);
    free(p);
}

int display_image(GTK_DATA *p, char *rgb24buf, int len){
    if(len != 1920 * 1920 * 3){
        printf("display image buf len is not correct %d\n", len);
        return -1;
    }

    p->g_image = cairo_image_surface_create_from_rgb24(rgb24buf, len, p->rgb32buf);

    gtk_widget_queue_draw(p->g_main_window);

    return 1;
}

void init_gtk(){
    XInitThreads();
    gtk_init(NULL, NULL);
}

GTK_DATA *init_display()
{
    GTK_DATA *p = (GTK_DATA *)malloc(sizeof(GTK_DATA));
    memset(p, 0, sizeof(GTK_DATA));
    p->rgb32buf = (char *)malloc(1920 * 1920 * 4);
    pthread_create(&p->draw_thread, NULL, draw_thread_main, (void *)p);

    return p;
}


