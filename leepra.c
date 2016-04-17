#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>

#include <stdio.h>

static HWND win = NULL;
static LPDIRECTDRAW ddraw = NULL;
static LPDIRECTDRAWSURFACE primarybuffer = NULL;
static LPDIRECTDRAWSURFACE backbuffer = NULL;
static LPDIRECTDRAWCLIPPER clipper = NULL;

typedef HRESULT (WINAPI * DIRECTDRAWCREATE) (GUID FAR *lpGUID,LPDIRECTDRAW FAR *lplpDD,IUnknown FAR *pUnkOuter);
static HMODULE library = 0;

static LRESULT CALLBACK WndProc(HWND hWnd,UINT message,WPARAM wParam,LPARAM lParam);

static int width, height;
static BOOL fullscreen = TRUE;
static BOOL active=FALSE;
static HINSTANCE inst;

/* Converter-stuff */
typedef void (*CONVERTER) (void *dst,const void *src, unsigned int count);
CONVERTER converter;
CONVERTER get_converter( unsigned int bpp, unsigned int r_mask, unsigned int g_mask, unsigned int b_mask );

int leepra_open( char* title, int width_, int height_, BOOL fullscreen_) {
	DIRECTDRAWCREATE DirectDrawCreate;
	HRESULT result;
	RECT rect;
	WNDCLASS wc;
	DDSURFACEDESC desc;
	DDSCAPS caps;
	unsigned int style;
	DDPIXELFORMAT pixelformat;

	inst = GetModuleHandle(NULL);
	fullscreen = fullscreen_;
	width = width_;
	height = height_;

	library = (HMODULE) LoadLibrary("ddraw.dll");
	if (!library) return FALSE;
	
	DirectDrawCreate = (DIRECTDRAWCREATE) GetProcAddress(library,"DirectDrawCreate");
	if (!DirectDrawCreate) return FALSE;

	result = DirectDrawCreate( 0, &ddraw, 0 );
	if (FAILED(result)) return FALSE;

	if (fullscreen) {
		result = IDirectDraw_SetDisplayMode( ddraw, width, height, 32 );
		if (FAILED(result)) {
			result = IDirectDraw_SetDisplayMode( ddraw, width, height, 24 );
			if (FAILED(result)) {
				result = IDirectDraw_SetDisplayMode( ddraw, width, height, 16 );
				if (FAILED(result)) return FALSE;
			}
		}
	}


	rect.left = 0;
	rect.top = 0;
	rect.right = width;
	rect.bottom = height;

	if (!fullscreen) {
		style = WS_VISIBLE|WS_OVERLAPPEDWINDOW;//WS_VISIBLE|WS_OVERLAPPED|WS_THICKFRAME|WS_CAPTION;
		AdjustWindowRect( &rect, WS_OVERLAPPEDWINDOW, FALSE );
	} else {
		style = WS_VISIBLE|WS_POPUP;
	}
	
	wc.style = CS_VREDRAW | CS_HREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = inst;
	wc.hIcon = 0;
	wc.hCursor = LoadCursor(0,IDC_ARROW);
	wc.hbrBackground = 0;
	wc.lpszMenuName = 0;
	wc.lpszClassName = "leepra";
	RegisterClass(&wc);

	win = CreateWindowEx(0, "leepra", title, style, 0, 0, rect.right-rect.left, rect.bottom-rect.top, 0, 0, inst, 0);
	if (!win) return FALSE;

	if (fullscreen) result = IDirectDraw_SetCooperativeLevel( ddraw, win, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN );
	else result = IDirectDraw_SetCooperativeLevel( ddraw, win, DDSCL_NORMAL );
	if (FAILED(result)) return FALSE;

	memset( &desc, 0, sizeof(DDSURFACEDESC) );
	desc.dwSize = sizeof(DDSURFACEDESC);

	if (fullscreen) {
		ShowCursor(0);
		desc.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
		desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_VIDEOMEMORY | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
		desc.dwBackBufferCount = 1;
	} else {
		desc.dwFlags = DDSD_CAPS;
		desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
	}

	result = IDirectDraw_CreateSurface( ddraw, &desc, &primarybuffer, NULL );
	if (FAILED(result)) return FALSE;

	if (fullscreen) {
		int i;
		caps.dwCaps = DDSCAPS_BACKBUFFER;
		result = IDirectDrawSurface_GetAttachedSurface( primarybuffer, &caps, &backbuffer );
		if( FAILED(result) ) return FALSE;

		for (i=0; i<2; i++) {
			DDSURFACEDESC descriptor;
			int pitch, y;
			char* dst;

			IDirectDrawSurface_Restore(primarybuffer);
			descriptor.dwSize = sizeof(DDSURFACEDESC);
			IDirectDrawSurface_Lock(backbuffer,0,&descriptor,DDLOCK_WAIT,0);
			pitch = descriptor.lPitch;

			dst = (char*)descriptor.lpSurface;
			for( y=height; y; y-- ){
				memset(dst, 0, width*4);
				dst += pitch;
			}
			IDirectDrawSurface_Unlock( backbuffer, descriptor.lpSurface);
			IDirectDrawSurface_Flip( primarybuffer, NULL, DDFLIP_WAIT );
		}
	} else {
		memset( &desc, 0, sizeof(DDSURFACEDESC) );
		desc.dwSize = sizeof(DDSURFACEDESC);
		desc.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
		desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
		desc.dwWidth = width;
		desc.dwHeight = height;

		result = IDirectDraw_CreateSurface( ddraw, &desc, &backbuffer, NULL );
		if (FAILED(result)) return FALSE;

		IDirectDraw_CreateClipper( ddraw, 0, &clipper, 0 );
		IDirectDrawClipper_SetHWnd( clipper, 0,win);
		IDirectDrawSurface_SetClipper( primarybuffer, clipper );
	}

	pixelformat.dwSize = sizeof(DDPIXELFORMAT);
	IDirectDrawSurface_GetPixelFormat( primarybuffer, &pixelformat );
	if (!(pixelformat.dwFlags & DDPF_RGB)) return FALSE;

#ifdef _DEBUG
	printf("%u\n", pixelformat.dwRGBBitCount );
#endif

	converter = get_converter( pixelformat.dwRGBBitCount, pixelformat.dwRBitMask, pixelformat.dwGBitMask, pixelformat.dwBBitMask );
	if(converter == NULL) return FALSE;

	active = TRUE;
	return TRUE;
}

void leepra_update( void* data ) {
	DDSURFACEDESC descriptor;
	int pitch;

	if (active == FALSE && fullscreen == TRUE) return;

	if (IDirectDrawSurface_IsLost(primarybuffer)) IDirectDrawSurface_Restore(primarybuffer);
	if (!fullscreen&&IDirectDrawSurface_IsLost(backbuffer)) IDirectDrawSurface_Restore(backbuffer);

	descriptor.dwSize = sizeof(DDSURFACEDESC);
	IDirectDrawSurface_Lock(backbuffer, 0,&descriptor,DDLOCK_WAIT,0);
	pitch = descriptor.lPitch;

	if (pitch==width) {
		converter(data, descriptor.lpSurface, width*height*4);
	} else {
		int y;
		char* dst = (char*)descriptor.lpSurface;
		char* src = (char*)data;
		for (y=height; y; y--) {
			converter(dst, src, width*4);
			src += width*4;
			dst += pitch;
		}
	}

	IDirectDrawSurface_Unlock(backbuffer,descriptor.lpSurface);

	if (!fullscreen) {
		RECT srcrect;
		RECT destrect;
		POINT point;

		point.x = 0;
		point.y = 0;
		ClientToScreen(win, &point);
		GetClientRect(win, &destrect);
		OffsetRect(&destrect, point.x, point.y);
		SetRect(&srcrect, 0, 0, width, height);
		IDirectDrawSurface_Blt(primarybuffer, &destrect, backbuffer, &srcrect, DDBLT_WAIT, NULL);
	} else {
		IDirectDrawSurface_Flip(primarybuffer, NULL, DDFLIP_WAIT);
	}
}

int leepra_handle_events() {
	int ret = 0;
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (msg.message == WM_QUIT) ret = 1;
	}
	if (GetAsyncKeyState(VK_ESCAPE)) ret = 1;
	return ret;
}

void leepra_close() {
	if (primarybuffer) {
		IDirectDrawSurface_Release(primarybuffer);
		primarybuffer = NULL;
	}

	if (!fullscreen) {
		if (backbuffer) {
			IDirectDrawSurface_Release(backbuffer);
			backbuffer = NULL;
		}
		if (clipper) {
			IDirectDrawClipper_Release(clipper);
			clipper = NULL;
		}
	}

	if (ddraw) {
		IDirectDraw_Release(ddraw);
		ddraw = NULL;
	}

	FreeLibrary(library);
	DestroyWindow( win );
	UnregisterClass("leepra", inst);
	ShowCursor( TRUE );
}


static LRESULT CALLBACK WndProc(HWND hWnd,UINT message,WPARAM wParam,LPARAM lParam){
	switch (message) {
	case WM_PAINT:
		if (!fullscreen) {
			RECT srcrect;
			RECT destrect;
			POINT point;

			point.x = 0;
			point.y = 0;
			ClientToScreen(win, &point);
			GetClientRect(win, &destrect);
			OffsetRect(&destrect, point.x, point.y);
			SetRect(&srcrect, 0, 0, width, height);
			IDirectDrawSurface_Blt( primarybuffer, &destrect, backbuffer, &srcrect, DDBLT_WAIT, NULL);
		} else {
			IDirectDrawSurface_Flip( primarybuffer, NULL, DDFLIP_WAIT );
		}
	break;
	case WM_ACTIVATEAPP:
		active = (BOOL) wParam;
	break;
	
	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;

	case WM_SYSCOMMAND:
		switch (wParam) {
		case SC_SCREENSAVE:
		case SC_MONITORPOWER:
			return 0;
		break;
		}
	break;

	}
    return DefWindowProc(hWnd,message,wParam,lParam);
}

/* pixelkonvertering */
void convert_argb32_argb888(void *dst, const void *src, unsigned int count);
void convert_argb32_rgb888(void *dst, const void *src, unsigned int count);
void convert_argb32_rgb565(void *dst, const void *src, unsigned int count);
void convert_argb32_rgb555(void *dst, const void *src, unsigned int count);

CONVERTER get_converter( unsigned int bpp, unsigned int r_mask, unsigned int g_mask, unsigned int b_mask ) {
	if (bpp==32) return &convert_argb32_argb888;
	if (bpp==24) return &convert_argb32_rgb888;
	if (bpp==16) return &convert_argb32_rgb565;
	if (bpp==15) return &convert_argb32_rgb555;

#ifdef _DEBUG
	printf("bpp:%u rmask:%x gmask:%x bmask:%x\n", bpp, r_mask, g_mask, b_mask);
#endif

	return NULL;
}


#pragma warning(disable:4731)
void convert_argb32_argb888(void *dst, const void *src, unsigned int num) {
	_asm{
		mov esi, src
		mov edi, dst
		mov ecx, num
		shr ecx, 6

align 16
lup:
		movq mm0, [esi]
		movq mm1, [esi+8]
		movq mm2, [esi+16]
		movq mm3, [esi+24]
		movq mm4, [esi+32]
		movq mm5, [esi+40]
		movq mm6, [esi+48]
		movq mm7, [esi+56]

		movq [edi], mm0
		movq [edi+8], mm1
		movq [edi+16], mm2
		movq [edi+24], mm3
		movq [edi+32], mm4
		movq [edi+40], mm5
		movq [edi+48], mm6
		movq [edi+56], mm7

		add esi, 64
		add edi, 64

		dec ecx
		jnz lup

		emms
	}
}

void convert_argb32_rgb565(void *dst, const void *src, unsigned int num) {
	_asm {
		push ebp
		mov esi,src
		mov edi,dst
		mov ecx,num
		xor edx,edx
align 16
lup:
		mov eax,[esi+edx*4]
		mov ebx,eax
		shr ebx,8
		and ebx,1111100000000000b
		mov ebp,eax
		shr ebp,3
		and ebp,0000000000011111b
		add ebx,ebp
		shr eax,5
		and eax,0000011111100000b
		add ebx,eax
		mov [edi+edx*2],bx
		inc edx
		dec ecx
		jnz lup
		pop ebp
	}
}

void convert_argb32_rgb555(void *dst, const void *src, unsigned int num) {
	_asm {
		push ebp
		mov esi,src
		mov edi,dst
		mov ecx,num
		xor edx,edx
align 16
lup:
		mov eax,[esi+edx*4]
		mov ebx,eax
		shr ebx,9
		and ebx,0111110000000000b
		mov ebp,eax
		shr ebp,3
		and ebp,0000000000011111b
		add ebx,ebp
		shr eax,6
		and eax,0000001111100000b
		add ebx,eax
		mov [edi+edx*2],bx
		inc edx
		dec ecx
		jnz lup
		pop ebp
	}
}


void convert_argb32_rgb888(void *dst, const void *src, unsigned int num) {
	_asm {
		mov esi,src
		mov edi,dst
		mov ecx,num
		xor edx,edx
align 16
lup:
		mov eax,[esi]
		mov [edi],eax
		add esi,4
		add edi,3
		dec ecx
		jnz lup
	}
}
