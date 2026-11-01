#include <windows.h>
#include <iostream>
int wmain(int argc,wchar_t**argv){
 if(argc!=4){std::cerr<<"Usage: Dlss5ProbeHost <probe DLL> <Streamline folder> <NR DLL>\n";return 2;}
 auto dll=LoadLibraryExW(argv[1],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
 if(!dll){std::cerr<<"Probe load failed "<<GetLastError()<<std::endl;return 1;}
 auto run=reinterpret_cast<int(*)(int,wchar_t**)>(GetProcAddress(dll,"RunNrProbe"));
 if(!run)return 1;return run(argc-1,argv+1);
}
