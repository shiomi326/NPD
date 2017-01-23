#include <iostream>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <iterator>
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>
#include "util.h"

// Functions

//c++ version sprintf
std::string strsprintf(const char* format,...){
    va_list ap;
    va_start(ap, format);
    char* alloc;
    if(vasprintf(&alloc,format,ap) == -1) {
     return std::string("");
    }
    va_end(ap);
    std::string retStr = std::string(alloc);
    free(alloc);
    return retStr;
}

//make random string
std::string random_string( size_t length )
{
    auto randchar = []() -> char
    {
        const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
        const size_t max_index = (sizeof(charset) - 1);
        return charset[ rand() % max_index ];
    };
    std::string str(length,0);
    std::generate_n( str.begin(), length, randchar );
    return str;
}


//make dir
bool mkDirs(std::string sBase, std::string sRelative)
{
    std::vector <std::string> path(10);
    // Pathを分割した後にVectorに代入する
    int nStart = 0;
    int nEnd = 0;
    int nCount = 0;
    for( nCount ; nEnd = sRelative.find("/",nStart+1) != sRelative.npos ; nCount++){
        nEnd = sRelative.find( "/" , nStart+1 );
        path[nCount] = sRelative.substr(nStart+1,nEnd - nStart -1) ;
        nStart = nEnd;
    }
    path[nCount] = sRelative.substr(nStart+1);
    path.resize(nCount+1);
    DIR* dir = opendir(sBase.c_str());
    if (dir == NULL)	mkdir(sBase.c_str(),S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    closedir(dir);

    for(int i=0; (nCount+1) != i ; i++){
        sBase = sBase + "/" + path[i];
        std::cout << sBase << std::endl;
        DIR* dir = opendir(sBase.c_str());
        if (dir == NULL){
//            mkdir(sBase.c_str(),0777);
            int dir_err = mkdir(sBase.c_str(),S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
            if(dir_err == -1){
                 printf("Error creating directory!n");
            }
            std::cout << sBase << std::endl;
        }
        closedir(dir);
    }
    return true;
}


//make dir from system function with sudo
//bool mkDirs(char *dirname)
bool mkDirs(std::string dirname)
{
    // is dir exist?
    struct stat st;
    if(stat(dirname.c_str(),&st) == 0){
        mode_t m = st.st_mode;
        if(S_ISDIR(m)){
            return true;
        }
    }

    //create folder
    char buffer[128];
    sprintf (buffer, "sudo mkdir -p %s", dirname.c_str());
    int dir_err = system(buffer);
    if (-1 == dir_err)
    {
        printf("Error creating directory!n");
        return false;
    }
    return true;
}
