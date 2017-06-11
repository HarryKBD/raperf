#include <cairo.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>

#include "cairo_display.h"

static GtkWidget *g_main_window = NULL;
static cairo_surface_t *g_image = NULL;
static void do_drawing(cairo_t *);
static char rgb32buf[1920 * 1920 * 4];
static pthread_t draw_thread;


static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, 
    gpointer user_data)
{      
    do_drawing(cr);
    return FALSE;
}

static void do_drawing(cairo_t *cr)
{
    if(!g_image){
        printf("Image is not ready.\n");
        return;
    }
    cairo_set_source_surface(cr, g_image , 10, 10);
    cairo_paint(cr);    
}


static char *convert_RGB24_to_RGB32(char *p, int n, int w, int h){
    int new_size = w * h * 4;
    if(n != 1920 * 1920*3){
        printf("input is not correct %d. expected %d\n", n, 1920*1920*3);
        return NULL;
    }
    memset(rgb32buf, 0x00, sizeof(rgb32buf));
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

static cairo_surface_t *cairo_image_surface_create_from_rgb24(char *rgb24buf, int len){
    char *rgb32_buf;
    int stride;
    int w, h;
    static int idx_tmp = 0;
    char tmp_file_name[10];
    FILE *tmp;

    w = 1920;
    h = 1920;

    //just for information
    stride = cairo_format_stride_for_width (CAIRO_FORMAT_RGB24, w);

    convert_RGB24_to_RGB32(rgb24buf, len, w, h);
    return cairo_image_surface_create_for_data ((unsigned char *)rgb32buf, CAIRO_FORMAT_RGB24, w, h, stride);
}

void *draw_thread_main(void *){
  GtkWidget *darea;
  int stride;

  printf("Creating draw thread...\n");

  g_main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  darea = gtk_drawing_area_new();
  gtk_container_add(GTK_CONTAINER (g_main_window), darea);

  g_signal_connect(G_OBJECT(darea), "draw", 
      G_CALLBACK(on_draw_event), NULL); 
  g_signal_connect(g_main_window, "destroy",
      G_CALLBACK (gtk_main_quit), NULL);

  gtk_window_set_position(GTK_WINDOW(g_main_window), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size(GTK_WINDOW(g_main_window), 1024, 768); 
  gtk_window_set_title(GTK_WINDOW(g_main_window), "Image");

  stride = cairo_format_stride_for_width (CAIRO_FORMAT_RGB24, 1920);
  g_image = cairo_image_surface_create_for_data ((unsigned char *)rgb32buf, CAIRO_FORMAT_RGB24, 1920, 1920, stride);
  gtk_widget_show_all(g_main_window);
  gtk_widget_queue_draw(g_main_window);

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

void cleanup_display(){
  cairo_surface_destroy(g_image);
}

int display_image(char *rgb24buf, int len){
    int stri;
    if(len != 1920 * 1920 * 3){
        printf("display image buf len is not correct %d\n", len);
        return -1;
    }

    g_image = cairo_image_surface_create_from_rgb24(rgb24buf, len);

    gtk_widget_queue_draw(g_main_window);

    return 1;
}

int init_display(int argc, char *argv[])
{
   gtk_init(&argc, &argv);
   pthread_create(&draw_thread, NULL, draw_thread_main, NULL);
   return 1;
}


