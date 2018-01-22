struct Window;

int Window(const char *title, int argc, char **argv);
int WindowStarted(void);
void WindowGo(void);
int WindowIsGlError(const char *function);
void WindowToggleFullScreen(void);
void WindowPrint(const char *fmt, ...);
void WindowRasteriseText(void);
