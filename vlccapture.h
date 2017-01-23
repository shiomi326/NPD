#ifndef VLCCAPTURE_H
#define VLCCAPTURE_H


extern void* lock(void *data, void**p_pixels);
extern void display(void *data, void *id);
extern void unlock(void *data, void *id, void *const *p_pixels);
extern int lvc_capture();

#endif // VLCCAPTURE_H
