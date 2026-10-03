"""Production mouse-edge regressions: real SDL queue and extracted Win32 dispatch.
Win32 uses a capture API model here, not a native Windows compiler/window.
"""
from pathlib import Path
from check_learning_text_layout import run
from check_learning_utf8 import cut

def main():
    root=Path(__file__).resolve().parents[1];out=root/'out/learning-checkpoints/68-immediate-ui-check';src=(root/'platform/win32.cpp').read_text()
    helper=cut(src,'static void update_mouse_position(')+cut(src,'static void release_mouse_capture_if_idle(')
    cases=src[src.index('    case WM_LBUTTONDOWN:'):src.index('    case WM_MOUSEWHEEL:')]
    a=src.index('    case WM_CAPTURECHANGED:');b=src.index('    case WM_CHAR:',a);cases+=src[a:b]
    head=r'''#include "core/key_edges.h"
    #include <cstdio>
    #include <cstdlib>
    #define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK %d %s\n",__LINE__,#e);std::exit(1);}}while(false)
    using HWND=int;
    using LPARAM=long long;
    static LPARAM lparam=0;
    static int s_mouse_x=0,s_mouse_y=0;
    #define LOWORD(v) (static_cast<unsigned long long>(v)&65535u)
    #define HIWORD(v) ((static_cast<unsigned long long>(v)>>16)&65535u)
    enum {WM_LBUTTONDOWN,WM_LBUTTONUP,WM_RBUTTONDOWN,WM_RBUTTONUP,WM_MBUTTONDOWN,WM_MBUTTONUP,WM_CAPTURECHANGED};
    static input_detail::KeyEdges<3> s_mouse;
    static HWND capture=0;
    static int releases=0;
    static int dispatch(HWND,int);
    static HWND GetCapture(){return capture;}
    static void SetCapture(HWND h){capture=h;}
    static void ReleaseCapture(){++releases;const auto old=capture;capture=0;dispatch(old,WM_CAPTURECHANGED);}
    '''
    body=head+helper+'\nstatic int dispatch(HWND hwnd,int message){switch(message){\n'+cases+'default:return 0;}}\n'+r'''
    int main(){
    for(int n=0;n<6;++n){lparam=0xFFFDFFFE;dispatch(1,n);CHECK(s_mouse_x==-2&&s_mouse_y==-3);}
    lparam=0;
    for(int n=0;n<3;++n){s_mouse.reset();s_mouse.begin_frame();dispatch(1,2*n);dispatch(1,2*n+1);CHECK(!s_mouse.down(n)&&s_mouse.pressed(n)&&s_mouse.released(n)&&capture==0);}
    s_mouse.reset();s_mouse.begin_frame();dispatch(1,WM_LBUTTONDOWN);dispatch(1,WM_RBUTTONDOWN);const int old=releases;
    dispatch(1,WM_LBUTTONUP);CHECK(capture==1&&releases==old&&s_mouse.down(1)&&s_mouse.pressed(0));
    dispatch(1,WM_RBUTTONUP);CHECK(capture==0&&releases==old+1&&s_mouse.pressed(0)&&s_mouse.pressed(1));
    s_mouse.begin_frame();dispatch(1,WM_MBUTTONDOWN);capture=2;dispatch(1,WM_CAPTURECHANGED);CHECK(!s_mouse.down(2)&&!s_mouse.pressed(2)&&s_mouse.released(2));
    std::puts("Win32 source dispatch: normal capture release preserves quick-click edges, last-held release and interrupted capture cancellation passed");}
    '''
    p=out/'win32-mouse.cpp';p.write_text(body);run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Wpedantic','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(root),str(p),'-o',str(out/'win32-mouse')]);r=run([str(out/'win32-mouse')]);(out/'win32-mouse.log').write_text(r.stdout);print(r.stdout)
    flags=run(['pkg-config','--cflags','--libs','sdl2']).stdout.split()
    binary=out/'root-pointer'
    run(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic',str(root/'tests/learning/current_platform_focus.cpp'),*flags,'-o',str(binary)])
    result=run([str(binary)]);(out/'root-pointer.log').write_text(result.stdout);print(result.stdout.strip(),flush=True)

if __name__=='__main__':main()
