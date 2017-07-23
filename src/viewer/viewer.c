#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <stdlib.h>

#include  <X11/Xlib.h>
#include  <X11/Xatom.h>
#include  <X11/Xutil.h>

#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "packet.h"

EGLNativeDisplayType eglNativeDisplay;
EGLNativeWindowType  eglNativeWindow;
EGLDisplay  eglDisplay;

static Display * x_display = NULL;
static Atom s_wmDeleteMessage;

int main(int argc ,char *argv[])
{
	int width = 640;
	int height = 480;
	char *title = "Camera";

	Window root;
	Window win;
    XSetWindowAttributes swa;
    XSetWindowAttributes  xattr;
    XWMHints hints;
    Atom wm_state;
    XEvent xev;

	x_display = XOpenDisplay(NULL);
	if(x_display == NULL){
		return -1;
	}

	root = DefaultRootWindow(x_display);
    swa.event_mask  =  ExposureMask | PointerMotionMask | KeyPressMask;
	win = XCreateWindow(x_display,root,0,0,width,height,0,
			CopyFromParent, InputOutput,
            CopyFromParent, CWEventMask,
	        &swa);

    s_wmDeleteMessage = XInternAtom(x_display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(x_display, win, &s_wmDeleteMessage, 1);
    xattr.override_redirect = 0;
    XChangeWindowAttributes ( x_display, win, CWOverrideRedirect, &xattr );
    
	hints.input = 1;
    hints.flags = InputHint;
    XSetWMHints(x_display, win, &hints);
    XMapWindow (x_display, win);
    XStoreName (x_display, win, title);
	wm_state = XInternAtom (x_display, "_NET_WM_STATE", 0);

    memset ( &xev, 0, sizeof(xev) );
    xev.type                 = ClientMessage;
    xev.xclient.window       = win;
    xev.xclient.message_type = wm_state;
    xev.xclient.format       = 32;
    xev.xclient.data.l[0]    = 1;
    xev.xclient.data.l[1]    = 0;
    XSendEvent (
       x_display,
       DefaultRootWindow ( x_display ),
       0,
       SubstructureNotifyMask,
       &xev );

	eglNativeDisplay = (EGLNativeDisplayType) x_display;
	eglNativeWindow = (EGLNativeWindowType) win;
	eglDisplay = eglGetDisplay(eglNativeDisplay );
	if (eglDisplay == EGL_NO_DISPLAY ){
		printf("no disply\n");
      return -1;
    }
	
	EGLint majorVersion;
	EGLint minorVersion;
	if ( !eglInitialize (eglDisplay, &majorVersion, &minorVersion ) ){
		printf("init failed\n");
      return -1;
   }
   
	EGLConfig config;
    EGLint numConfigs = 0;
    EGLint attribList[] ={
	   EGL_RED_SIZE,       5,
       EGL_GREEN_SIZE,     6,
       EGL_BLUE_SIZE,      5,
       EGL_ALPHA_SIZE,     EGL_DONT_CARE,
       EGL_DEPTH_SIZE,     EGL_DONT_CARE,
       EGL_STENCIL_SIZE,   EGL_DONT_CARE,
       EGL_SAMPLE_BUFFERS, 0,
       EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT_KHR,
       EGL_NONE};
	if ( !eglChooseConfig(eglDisplay, attribList, &config, 1, &numConfigs ) ){
		 printf("failed eglchooseConfig \n");
         return -1;
	}
	if ( numConfigs < 1 ){
	   printf("failed eglchooseConfig numconfig\n"); 
	   return -1;
	}
	EGLSurface	eglSurface = eglCreateWindowSurface(eglDisplay, config,eglNativeWindow, NULL );

   if (eglSurface == NULL){
	   printf("failed eglsurface \n");
	   return -1;
   }

   
   EGLint contextAttribs[] = {0x3098, 3, 0x3038};
   EGLContext eglContext = eglCreateContext (eglDisplay, config,NULL,contextAttribs );

   if (eglContext == NULL){
	   printf("failed eglcontext\n");
       return -1;
   }

   // Make the context current
   if ( !eglMakeCurrent(eglDisplay,eglSurface,eglSurface,eglContext)){
	   printf("failed eglMakeCurrent\n");
	   return -1;
   }



   /*
	*
	* */

	char vShaderStr_[] =
      "#version 300 es                            \n"
      "layout(location = 0) in vec4 a_position;   \n"
      "layout(location = 1) in vec2 a_texCoord;   \n"
      "out vec2 v_texCoord;                       \n"
      "void main()                                \n"
      "{                                          \n"
      "   gl_Position = a_position;               \n"
      "   v_texCoord = a_texCoord;                \n"
      "}                                          \n";

   char fShaderStr_[] =
      "#version 300 es                                     \n"
      "precision mediump float;                            \n"
      "in vec2 v_texCoord;                                 \n"
      "layout(location = 0) out vec4 outColor;             \n"
      "uniform sampler2D s_texture;                        \n"
      "void main()                                         \n"
      "{                                                   \n"
      "  outColor = texture( s_texture, v_texCoord);      \n"
      "}                                                   \n";
	const char * vShaderStr = vShaderStr_;
	const char * fShaderStr = fShaderStr_;
	GLuint vshader = 0;
	GLuint fshader = 0;
	GLuint compiled = 0;
	vshader = glCreateShader (GL_VERTEX_SHADER);
	if(vshader == 0){
		printf("Failed Create Shader !\n");
		return 0;
	}
	glShaderSource(vshader,1,(const GLchar *const*)&vShaderStr,NULL);
	glCompileShader(vshader);
	glGetShaderiv(vshader,GL_COMPILE_STATUS,&compiled);
	if(!compiled){
      GLint infoLen = 0;
      glGetShaderiv (vshader, GL_INFO_LOG_LENGTH, &infoLen );
      glDeleteShader (vshader);
	  printf("Failed Compile shader v!\n");
	  return -1;
	}

	fshader = glCreateShader (GL_FRAGMENT_SHADER);
	if(fshader == 0){
		printf("Failed Create Shader !\n");
		return 0;
	}
	glShaderSource(fshader,1,(const GLchar *const*)&fShaderStr,NULL);
	glCompileShader(fshader);
	glGetShaderiv(fshader,GL_COMPILE_STATUS,&compiled);
	if(!compiled){
		printf("Failed Compile shader v!\n");
		return -1;
	}

	GLuint linked = 0;
	GLuint programObject = glCreateProgram();
	if(programObject == 0 ){
		return -1;
	}
	glAttachShader(programObject,vshader);
	glAttachShader(programObject,fshader);
	glLinkProgram (programObject);
	glGetProgramiv (programObject, GL_LINK_STATUS, &linked );
	if(!linked){
		printf("Failed program linked !\n");
		return -1;
	}
	glDeleteShader(vshader );
	glDeleteShader(fshader );
	
	GLint sampler_loc = 0;
	sampler_loc = glGetUniformLocation(programObject, "s_texture");

	
//	GLubyte pixels_[640*480*3];
    GLubyte rec[1459200+512];
	GLuint textureId;

/*************************************************/
	int result;
	int serv_fd;

    do{
        serv_fd = create_client("127.0.0.1",6666,10);
        if(serv_fd < 0){
            perror(strerror(serv_fd));
            return serv_fd;
        }

        result = video_send_identity(serv_fd,0x1111,VIDEO_IDENTITY_WYA_VIEW,"12345",6);
        if(result < 0){
            close(serv_fd);
            serv_fd = -1;
            continue;
        }

        result = rev_pkt_with_mem(serv_fd,(void*)rec,1024);
        if(result < 0){
            close(serv_fd);
            serv_fd = -1;
            continue;
        }

        if(is_reply_packet(rec) == 0){
            close(serv_fd);
            serv_fd = -1;
            continue;
        }

        if(is_reply_identity_ok(rec) == 0){
            close(serv_fd);
            serv_fd = -1;
            continue;
        }

    }while(serv_fd < 0);

/*************************************************/
	int len = 0;
	int max = 0;
LOOP:
	len = 0;
	max = 0;

    rev_pkt_with_mem(serv_fd,rec,1459200+512);
	printf("%ld\n",(((struct video_data *) &((struct video_head *)rec)->data[0])->vdata_len));
	float Y,U,V;
	unsigned short R,G,B;
    unsigned char * data = &(((struct video_data *) &((struct video_head *)rec)->data[0])->vdata[0]);
	
	/*for(int i=0;i<640*480*2;i+=4){
		
		Y = data[i];
		U = data[i+1];
		V = data[i+3];
			
		pixels_[i/2*3+2] = 1.164*(Y - 16)+ 2.018*(U - 128);
		pixels_[i/2*3+1] = 1.164*(Y - 16) - 0.813*(V - 128) - 0.391*(U - 128);
		pixels_[i/2*3+0] = 1.164*(Y - 16) + 1.596*(V - 128);


		Y = data[i+2];
			
		pixels_[i/2*3+5] = 1.164*(Y - 16)+ 2.018*(U - 128);
		pixels_[i/2*3+4] = 1.164*(Y - 16) - 0.813*(V - 128) - 0.391*(U - 128);
		pixels_[i/2*3+3] = 1.164*(Y - 16) + 1.596*(V - 128);
			
	}*/


	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glGenTextures(1, &textureId );
	glBindTexture(GL_TEXTURE_2D, textureId);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 800, 600, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glClearColor(1.0f, 1.0f, 1.0f, 0.0f );
	
	
	GLfloat vVertices[] = { -1.0f,  1.0f, -0.5f,  // Position 0
                            0.0f,  0.0f,        // TexCoord 0 
                           -1.0f, -1.0f, -0.5f,  // Position 1
                            0.0f,  1.0f,        // TexCoord 1
                            1.0f, -1.0f, -0.5f,  // Position 2
                            1.0f,  1.0f,        // TexCoord 2
                            1.0f,  1.0f, -0.0f,  // Position 3
                            1.0f,  0.0f         // TexCoord 3
                         };
	GLushort indices[] = { 0, 1, 2, 0, 2, 3 };
	glViewport(0,0,width,height);
	glDepthRangef(0.5,1.0);
	//glFrustum(0.5,0.5,1,1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(programObject);
	glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,5*sizeof(GLfloat),vVertices);
	glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,5*sizeof(GLfloat),&vVertices[3]);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D,textureId);
	glUniform1i(sampler_loc,0);

	glDrawElements ( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices );
	eglSwapBuffers(eglDisplay,eglSurface);        

	int loop =10;
	while(loop >0){
		eglSwapBuffers(eglDisplay,eglSurface);        
		loop --;
	}
	goto LOOP;
	printf("Hello \n");
	return 0;
}




