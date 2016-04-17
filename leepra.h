#ifndef __LEEPRA_H__
#define __LEEPRA_H__

#ifdef __cplusplus
extern "C" {
#endif

int leepra_open( char* title, int width, int height, int fullscreen );
int leepra_handle_events();
void leepra_update( void* data );
void leepra_close();

#ifdef __cplusplus
}
#endif

#endif /* __LEEPRA_H__ */
