#ifdef __cplusplus
    extern "C" {
#endif

int init_display(int argc, char *argv[]);
void cleanup_display();
int display_image(char *rgb24buf, int len);

#ifdef __cplusplus
    }
#endif
