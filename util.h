#ifndef UTIL_H
#define UTIL_H


#define FRAME1    0
#define FRAME2    1
#define FRAME3    2

struct npd_image {
    std::string id;
    std::string location;
    std::string timestamp;
    std::string irImage;
    std::string colorImage;
};

struct lblElement
{
    int  area;
    int  x;
    int  y;
    int  width;
    int  height;
    double  centerX;
    double  centerY;
};

extern std::string strsprintf(const char* format,...);
extern std::string random_string( size_t length );
extern bool mkDirs(std::string sBase, std::string sRelative);
extern bool mkDirs(std::string dirname);
//extern bool mkDirs(char* dirname);


#endif // UTIL_H
